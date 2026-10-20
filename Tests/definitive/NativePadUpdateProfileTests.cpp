/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify actual updater qualification without executing native code
 *****************************************************************************/
#include "../../Client/game_de/NativePadUpdateProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{
    namespace P = mta::game_de::NativePadUpdateProfile;
    unsigned checks = 0;
    void     Check(bool value, const char* label)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", label);
            std::exit(EXIT_FAILURE);
        }
    }
    bool Same(const P::Binding& first, const P::Binding& second)
    {
        return first.image == second.image && first.updater == second.updater && first.pads == second.pads;
    }
}

#if defined(MTA_TEST_PAD_UPDATE_PROFILE_CONTROLLED)
namespace
{
    std::uintptr_t fixtureImageAddress = 0;
    bool           buildAllowed = true, denyCode = false, denyPads = false, denyReads = false;
    unsigned       pageCalls = 0;
    HMODULE WINAPI TestModuleHandle(LPCWSTR name) noexcept
    {
        return name ? ::GetModuleHandleW(name) : reinterpret_cast<HMODULE>(fixtureImageAddress);
    }
}
namespace mta::game_de::PadUpdateTestImage
{
    // Only this included profile TU sees controlled image/page evidence. The
    // shipping wrong-image executable below links the genuine guard unchanged.
    bool CheckExecutableBuild() noexcept
    {
        return buildAllowed;
    }
    bool ImagePages(std::uintptr_t base, std::uintptr_t address, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        ++pageCalls;
        if (!fixtureImageAddress || base != fixtureImageAddress || address < fixtureImageAddress || address - fixtureImageAddress > P::ImageSize ||
            size > P::ImageSize - (address - fixtureImageAddress))
            return false;
        if (writable)
            return !denyPads && address >= fixtureImageAddress + P::PadArrayRva && size <= P::PadCount * P::PadStride &&
                   address - fixtureImageAddress - P::PadArrayRva <= P::PadCount * P::PadStride - size;
        return !executable || !denyCode;
    }
    bool ReadImage(std::uintptr_t base, std::uintptr_t address, void* output, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        SIZE_T copied = 0;
        return !denyReads && ImagePages(base, address, size, writable, executable) &&
               ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &copied) && copied == size;
    }
    bool SectionContains(const IMAGE_SECTION_HEADER& section, std::size_t rva, std::size_t size, DWORD required, DWORD forbidden) noexcept
    {
        return NativeImageAccess::SectionContains(section, rva, size, required, forbidden);
    }
}
    #define NativeImageAccess PadUpdateTestImage
    #define GetModuleHandleW  TestModuleHandle
#endif

// Compile the actual production functions unchanged. Controlled dependencies
// are limited to image identity/pages; real OS unwind lookup remains in use.
#include "../../Client/game_de/NativePadUpdateProfile.cpp"

#if defined(MTA_TEST_PAD_UPDATE_PROFILE_CONTROLLED)
    #undef NativeImageAccess
    #undef GetModuleHandleW
namespace
{
    struct OwnedImage
    {
        unsigned char* bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, P::ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        bool           registered = false;
        OwnedImage()
        {
            Check(bytes != nullptr, "reserve only fixture-owned image memory");
            fixtureImageAddress = reinterpret_cast<std::uintptr_t>(bytes);
            auto& dos = *reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            auto& nt = Headers();
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.NumberOfSections = 4;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION] = {P::RuntimeEntries.front().rva,
                                                                                static_cast<DWORD>(sizeof(RUNTIME_FUNCTION) * 5)};
            auto* sections = Sections();
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0x3f40000;
            sections[0].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            sections[1].VirtualAddress = 0x5000000;
            sections[1].Misc.VirtualSize = 0x800000;
            sections[1].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            sections[2].VirtualAddress = 0x5900000;
            sections[2].Misc.VirtualSize = 0x100000;
            sections[2].Characteristics = IMAGE_SCN_MEM_READ;
            sections[3].VirtualAddress = 0x4c00000;
            sections[3].Misc.VirtualSize = 0x100000;
            sections[3].Characteristics = IMAGE_SCN_MEM_READ;
            for (const auto& pin : P::Pins)
                std::memcpy(bytes + pin.rva, pin.bytes, pin.size);
            std::memcpy(bytes + P::UpdaterRva, P::EntryBytes.data(), P::EntryBytes.size());
            for (const auto& entry : P::RuntimeEntries)
            {
                auto& runtime = *reinterpret_cast<RUNTIME_FUNCTION*>(bytes + entry.rva);
                runtime.BeginAddress = entry.begin;
                runtime.EndAddress = entry.end;
                runtime.UnwindData = entry.unwind;
            }
            registered = RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(bytes + P::RuntimeEntries.front().rva), 5, fixtureImageAddress) != FALSE;
            Check(registered, "register only fixture-owned metadata for real OS unwind lookup");
        }
        ~OwnedImage()
        {
            if (registered)
                RtlDeleteFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(bytes + P::RuntimeEntries.front().rva));
            fixtureImageAddress = 0;
            if (bytes)
                VirtualFree(bytes, 0, MEM_RELEASE);
        }
        IMAGE_NT_HEADERS64&   Headers() { return *reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + 0x100); }
        IMAGE_SECTION_HEADER* Sections() { return reinterpret_cast<IMAGE_SECTION_HEADER*>(bytes + 0x100 + sizeof(IMAGE_NT_HEADERS64)); }
    };
}
#endif

int main()
{
    P::Binding sentinel{11, 22, 33};
#if defined(MTA_TEST_PAD_UPDATE_PROFILE_CONTROLLED)
    OwnedImage owned;
    buildAllowed = false;
    auto result = sentinel;
    Check(!P::Qualify(result) && Same(result, sentinel) && !pageCalls, "build refusal preserves output before reading any native profile");
    buildAllowed = true;
    Check(P::Qualify(result) && result.image == fixtureImageAddress && result.updater == fixtureImageAddress + P::UpdaterRva &&
              result.pads == fixtureImageAddress + P::PadArrayRva,
          "matching controlled image publishes only updater and pad identities");
    Check(P::Revalidate(result), "matching OS unwind entries and all code pins revalidate");
    const auto refusal = [&]
    {
        auto output = sentinel;
        Check(!P::Qualify(output) && Same(output, sentinel) && !P::Revalidate(result), "changed qualification evidence refuses without clobbering output");
    };
    // Every known instruction has independent refusal coverage. Hooked mode
    // can vary only the explicitly owner-checked entry prefix, never callsites.
    for (const auto& pin : P::Pins)
    {
        owned.bytes[pin.rva] ^= 1;
        refusal();
        const bool inEntry = pin.rva >= P::UpdaterRva && pin.rva - P::UpdaterRva < P::EntryBytes.size();
        Check(P::Revalidate(result, false) == inEntry, "owned-entry mode cannot excuse a changed external pin or pad layout instruction");
        owned.bytes[pin.rva] ^= 1;
    }
    owned.bytes[P::UpdaterRva + 4] ^= 1;
    refusal();
    Check(P::Revalidate(result, false), "only the owner's separately checked complete entry prefix can differ in hooked mode");
    owned.bytes[P::UpdaterRva + 4] ^= 1;
    auto changed = result;
    ++changed.updater;
    Check(!P::Revalidate(changed), "updater binding cannot move within a matched function");
    changed = result;
    changed.pads += P::PadStride;
    Check(!P::Revalidate(changed), "binding cannot silently select the second pad as the array base");
    changed = result;
    ++changed.image;
    Check(!P::Revalidate(changed), "borrowed image identity must remain the current main module");
    auto&      nt = owned.Headers();
    auto*      sections = owned.Sections();
    const auto mutate = [&](auto& field, auto value)
    {
        const auto before = field;
        field = value;
        refusal();
        field = before;
    };
    mutate(reinterpret_cast<IMAGE_DOS_HEADER*>(owned.bytes)->e_magic, WORD{0});
    mutate(reinterpret_cast<IMAGE_DOS_HEADER*>(owned.bytes)->e_lfanew, LONG{-1});
    mutate(nt.FileHeader.Machine, WORD{IMAGE_FILE_MACHINE_I386});
    mutate(nt.OptionalHeader.Magic, WORD{IMAGE_NT_OPTIONAL_HDR32_MAGIC});
    mutate(nt.OptionalHeader.SizeOfImage, DWORD{4096});
    mutate(nt.FileHeader.NumberOfSections, WORD{0});
    mutate(nt.FileHeader.SizeOfOptionalHeader, WORD{0});
    mutate(nt.OptionalHeader.NumberOfRvaAndSizes, DWORD{0});
    mutate(sections[0].Characteristics, DWORD{IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE});
    mutate(sections[1].Characteristics, DWORD{IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE});
    mutate(sections[2].Characteristics, DWORD{IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE});
    mutate(sections[3].Characteristics, DWORD{IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE});
    mutate(nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size, DWORD{0});
    for (const auto& entry : P::RuntimeEntries)
        mutate(reinterpret_cast<RUNTIME_FUNCTION*>(owned.bytes + entry.rva)->EndAddress, entry.end - 1);
    denyCode = true;
    refusal();
    denyCode = false;
    denyPads = true;
    refusal();
    denyPads = false;
    denyReads = true;
    refusal();
    denyReads = false;
    Check(RtlDeleteFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(owned.bytes + P::RuntimeEntries.front().rva)) != FALSE,
          "retire only this fixture's dynamic unwind metadata");
    owned.registered = false;
    refusal();
    std::puts("Controlled native profile evidence does not qualify an engine hook, call or mutable pad scope.");
    std::printf("Passed %u native pad updater profile checks (64-bit; controlled-image)\n", checks);
#else
    auto result = sentinel;
    Check(!P::Qualify(result) && Same(result, sentinel), "shipping exact-executable gate refuses the ordinary owned host without altering output");
    const auto actual = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    Check(!P::Revalidate({actual, actual + P::UpdaterRva, actual + P::PadArrayRva}), "arbitrary main-module addresses cannot manufacture a valid DE profile");
    std::printf("Passed %u native pad updater profile checks (64-bit; shipping-wrong-image)\n", checks);
#endif
    return 0;
}

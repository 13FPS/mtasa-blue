/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual timestamp qualification in owned ordinary hosts
 *****************************************************************************/
#include "../../Client/game_de/NativePadActivityProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    namespace P = mta::game_de::NativePadActivityProfile;
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
    bool Same(const P::Binding& left, const P::Binding& right)
    {
        return left.image == right.image && left.pads == right.pads;
    }
}
#if defined(MTA_TEST_PAD_ACTIVITY_PROFILE_CONTROLLED)
namespace
{
    std::uintptr_t fixtureImageAddress = 0;
    bool           buildAllowed = true, denyCode = false, denyData = false, denyField = false, denyTimer = false, denyReads = false;
    unsigned       pageCalls = 0;
    HMODULE WINAPI TestModuleHandle(LPCWSTR name) noexcept
    {
        return name ? ::GetModuleHandleW(name) : reinterpret_cast<HMODULE>(fixtureImageAddress);
    }
}
namespace mta::game_de::PadActivityTestImage
{
    bool CheckExecutableBuild() noexcept
    {
        return buildAllowed;
    }
    bool ImagePages(std::uintptr_t base, std::uintptr_t address, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        ++pageCalls;
        if (!fixtureImageAddress || base != fixtureImageAddress || address < base || address - base > P::ImageSize || size > P::ImageSize - (address - base))
            return false;
        const auto rva = address - base;
        if (writable && (denyData || (denyTimer && rva == P::NativeTimerRva)))
            return false;
        for (std::size_t pad = 0; pad < P::PadCount; ++pad)
            if (denyField && writable && rva == P::PadArrayRva + pad * P::PadStride + P::FieldOffset && size == P::FieldSize)
                return false;
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
    #define NativeImageAccess PadActivityTestImage
    #define GetModuleHandleW  TestModuleHandle
#endif
// The production qualifier is compiled unchanged. Only this controlled TU's
// image evidence is substituted; the shipping host below links the real guard.
#include "../../Client/game_de/NativePadActivityProfile.cpp"
#if defined(MTA_TEST_PAD_ACTIVITY_PROFILE_CONTROLLED)
    #undef NativeImageAccess
    #undef GetModuleHandleW
namespace
{
    struct OwnedImage
    {
        unsigned char* bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, P::ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        OwnedImage()
        {
            Check(bytes != nullptr, "reserve fixture memory only");
            fixtureImageAddress = reinterpret_cast<std::uintptr_t>(bytes);
            auto& dos = *reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            auto& nt = Headers();
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.NumberOfSections = 2;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            auto* sections = Sections();
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0x3f40000;
            sections[0].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            sections[1].VirtualAddress = 0x5000000;
            sections[1].Misc.VirtualSize = 0x800000;
            sections[1].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            for (const auto& pin : P::Pins)
                std::memcpy(bytes + pin.rva, pin.bytes, pin.size);
            for (std::size_t pad = 0; pad < P::PadCount; ++pad)
                Identity(pad) = static_cast<std::uint32_t>(pad);
        }
        ~OwnedImage()
        {
            fixtureImageAddress = 0;
            if (bytes)
                VirtualFree(bytes, 0, MEM_RELEASE);
        }
        IMAGE_NT_HEADERS64&   Headers() { return *reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + 0x100); }
        IMAGE_SECTION_HEADER* Sections() { return reinterpret_cast<IMAGE_SECTION_HEADER*>(bytes + 0x100 + sizeof(IMAGE_NT_HEADERS64)); }
        std::uint32_t&        Identity(std::size_t pad)
        {
            return *reinterpret_cast<std::uint32_t*>(bytes + P::PadArrayRva + pad * P::PadStride + P::PadIdentityOffset);
        }
    };
}
#endif
int main()
{
    const P::Binding sentinel{11, 22};
#if defined(MTA_TEST_PAD_ACTIVITY_PROFILE_CONTROLLED)
    OwnedImage owned;
    buildAllowed = false;
    auto result = sentinel;
    Check(!P::Qualify(result) && Same(result, sentinel) && !pageCalls, "exact-build refusal preserves output before any page read");
    buildAllowed = true;
    Check(P::Qualify(result) && result.image == fixtureImageAddress && result.pads == fixtureImageAddress + P::PadArrayRva,
          "matching constructed physical pads qualify");
    Check(P::Revalidate(result), "unchanged code, physical IDs and field pages revalidate");
    const auto refusal = [&]
    {
        auto output = sentinel;
        Check(!P::Qualify(output) && Same(output, sentinel) && !P::Revalidate(result), "changed evidence refuses without output publication");
    };
    for (const auto& pin : P::Pins)
    {
        owned.bytes[pin.rva] ^= 1;
        refusal();
        owned.bytes[pin.rva] ^= 1;
    }
    // This independent field profile intentionally has no updater-entry pin.
    // No synthetic bytes are executed and this grants no hook ownership.
    std::memset(owned.bytes + 0x1154c00, 0xcc, 32);
    Check(P::Revalidate(result), "an independent owned updater entry is outside timestamp qualification");
    for (std::size_t pad = 0; pad < P::PadCount; ++pad)
    {
        owned.Identity(pad) = 99;
        refusal();
        owned.Identity(pad) = static_cast<std::uint32_t>(pad);
    }
    owned.Identity(1) = 0;
    refusal();
    owned.Identity(1) = 1;
    auto changed = result;
    ++changed.image;
    Check(!P::Revalidate(changed), "main-module identity is immutable");
    changed = result;
    changed.pads += P::PadStride;
    Check(!P::Revalidate(changed), "binding cannot silently switch array origin");
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
    mutate(nt.Signature, DWORD{0});
    mutate(nt.FileHeader.Machine, WORD{IMAGE_FILE_MACHINE_I386});
    mutate(nt.OptionalHeader.Magic, WORD{IMAGE_NT_OPTIONAL_HDR32_MAGIC});
    mutate(nt.OptionalHeader.SizeOfImage, DWORD{4096});
    mutate(nt.FileHeader.NumberOfSections, WORD{0});
    mutate(nt.FileHeader.SizeOfOptionalHeader, WORD{0});
    mutate(sections[0].Characteristics, DWORD{IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE});
    mutate(sections[1].Characteristics, DWORD{IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE});
    denyCode = true;
    refusal();
    denyCode = false;
    denyData = true;
    refusal();
    denyData = false;
    denyTimer = true;
    refusal();
    denyTimer = false;
    denyField = true;
    refusal();
    denyField = false;
    denyReads = true;
    refusal();
    denyReads = false;
    Check(P::Revalidate(result), "all restored evidence revalidates without any native operation");
    std::printf("Passed %u native pad activity profile checks (64-bit; controlled-image)\n", checks);
#else
    auto result = sentinel;
    Check(!P::Qualify(result) && Same(result, sentinel), "shipping exact-image guard refuses this ordinary host before binding");
    const auto actual = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    Check(!P::Revalidate({actual, actual + P::PadArrayRva}), "ordinary host RVAs cannot manufacture authority");
    std::printf("Passed %u native pad activity profile checks (64-bit; shipping-wrong-image)\n", checks);
#endif
    return 0;
}

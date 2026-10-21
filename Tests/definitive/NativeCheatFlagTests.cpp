/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual four-flag qualifier and atomic native access in owned hosts
 *****************************************************************************/
#include "../../Client/game_de/NativeCheatProfile.h"
#include "../../Client/game_de/NativeCheatFlagAccess.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{
    namespace P = mta::game_de::NativeCheatProfile;
    using Access = mta::game_de::NativeCheatFlagAccess;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, why);
            std::exit(1);
        }
    }
}
#if defined(MTA_TEST_CHEAT_CONTROLLED)
namespace
{
    std::uintptr_t selectedImage = 0;
    bool           buildAllowed = true, denyReads = false, denyCode = false, denyData = false;
    unsigned       readCalls = 0, failReadAt = 0, accessPages = 0;
    bool           denyAccessRead = false, denyAccessWrite = false, unconfirmWrite = false, foreignBeforeCompare = false, faultBeforeCompare = false;
    DWORD          faultProtection = 0;
    HMODULE WINAPI FixtureModule(LPCWSTR name)
    {
        return name ? ::GetModuleHandleW(name) : reinterpret_cast<HMODULE>(selectedImage);
    }
}
namespace mta::game_de::CheatFixtureImage
{
    bool CheckExecutableBuild() noexcept
    {
        return buildAllowed;
    }
    bool ImagePages(std::uintptr_t image, std::uintptr_t address, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        if (!selectedImage || image != selectedImage || address < image || address - image > P::ImageSize || size > P::ImageSize - (address - image))
            return false;
        return !(writable && denyData) && !(executable && denyCode);
    }
    bool ReadImage(std::uintptr_t image, std::uintptr_t address, void* output, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        ++readCalls;
        if (denyReads || readCalls == failReadAt || !ImagePages(image, address, size, writable, executable))
        {
            std::memset(output, 0xa7, size);
            return false;
        }
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &copied) && copied == size;
    }
    bool SectionContains(const IMAGE_SECTION_HEADER& section, std::size_t rva, std::size_t size, DWORD required, DWORD forbidden) noexcept
    {
        return NativeImageAccess::SectionContains(section, rva, size, required, forbidden);
    }
}
    #define NativeImageAccess CheatFixtureImage
    #define GetModuleHandleW  FixtureModule
#endif
#include "../../Client/game_de/NativeCheatProfile.cpp"
#if defined(MTA_TEST_CHEAT_CONTROLLED)
    #undef NativeImageAccess
    #undef GetModuleHandleW
namespace
{
    class OwnedImage
    {
    public:
        unsigned char* bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, P::ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        OwnedImage()
        {
            Check(bytes != nullptr, "allocate only owned image storage");
            selectedImage = reinterpret_cast<std::uintptr_t>(bytes);
            auto& dos = *reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            auto& nt = Headers();
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = 3;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            auto* sections = Sections();
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0x3f40000;
            sections[0].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            sections[1].VirtualAddress = 0x3f41000;
            sections[1].Misc.VirtualSize = 0x5000000 - 0x3f41000;
            sections[1].Characteristics = IMAGE_SCN_MEM_READ;
            sections[2].VirtualAddress = 0x5000000;
            sections[2].Misc.VirtualSize = static_cast<DWORD>(P::ImageSize - 0x5000000);
            sections[2].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            for (const auto& pin : P::Code)
                std::memcpy(bytes + pin.rva, pin.bytes.data(), pin.size);
            for (const auto& registration : P::Registrations)
            {
                std::memcpy(bytes + registration.nameRva, registration.name, std::strlen(registration.name) + 1);
                std::array<unsigned char, 7> instruction = {0x4c, 0x8d, 0x05};
                const auto                   relative = static_cast<std::int32_t>(registration.nameRva - (registration.instructionRva + 7));
                std::memcpy(instruction.data() + 3, &relative, sizeof(relative));
                std::memcpy(bytes + registration.instructionRva, instruction.data(), instruction.size());
            }
            std::memset(bytes + P::FlagsRva, 0xa5, P::NativeFlagCount);
            for (const auto& member : P::Members)
            {
                bytes[P::FlagsRva + member.nativeIndex] = 0;
                std::memset(bytes + P::DispatchTableRva + member.nativeIndex * 8, 0, 8);
            }
        }
        ~OwnedImage()
        {
            selectedImage = 0;
            if (bytes)
                VirtualFree(bytes, 0, MEM_RELEASE);
        }
        IMAGE_NT_HEADERS64&   Headers() { return *reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + 0x100); }
        IMAGE_SECTION_HEADER* Sections() { return reinterpret_cast<IMAGE_SECTION_HEADER*>(bytes + 0x100 + sizeof(IMAGE_NT_HEADERS64)); }
    };
    bool AccessPages(std::uintptr_t address, std::size_t size, bool writable, bool executable) noexcept
    {
        ++accessPages;
        if (!selectedImage || size != 1 || executable || (writable ? denyAccessWrite : denyAccessRead))
            return false;
        bool known = false;
        for (const auto& member : P::Members)
            known |= address == selectedImage + P::FlagsRva + member.nativeIndex;
        if (!known)
            return false;
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
            reinterpret_cast<std::uintptr_t>(info.AllocationBase) != selectedImage || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        if (writable && foreignBeforeCompare)
        {
            foreignBeforeCompare = false;
            *reinterpret_cast<unsigned char*>(address) = 2;
        }
        if (writable && unconfirmWrite)
        {
            unconfirmWrite = false;
            denyAccessRead = true;
        }
        if (writable && faultBeforeCompare)
        {
            faultBeforeCompare = false;
            if (!VirtualProtect(reinterpret_cast<void*>(address), 1, PAGE_NOACCESS, &faultProtection))
                std::abort();
        }
        return true;
    }
}
#endif

int main()
{
    P::Flag flag = P::Flag::ExtraJump;
    for (std::size_t index = 0; index < P::Members.size(); ++index)
        Check(P::Find(P::Members[index].sdkName, flag) && static_cast<std::size_t>(flag) == index,
              "canonical actual MTA SDK names select immutable native indices");
    flag = P::Flag::ExtraJump;
    Check(!P::Find(nullptr, flag) && flag == P::Flag::ExtraJump, "null name preserves lookup output");
    Check(!P::Find("noreload", flag) && flag == P::Flag::ExtraJump, "unproved registry entries cannot become invented false native flags");
    Check(!P::Find("HoverCars", flag) && flag == P::Flag::ExtraJump, "classic registry name comparison remains case-sensitive");
    P::Binding binding{11, 22};
#if !defined(MTA_TEST_CHEAT_CONTROLLED)
    Check(!P::Qualify(binding) && binding.image == 11 && binding.flags == 22, "shipping exact-image guard refuses ordinary host and preserves output");
    Check(!P::Revalidate(binding), "shipping binding cannot borrow ordinary host memory");
    std::printf("Passed %u native cheat flag checks (64-bit; shipping-wrong-image)\n", checks);
#else
    OwnedImage owned;
    Check(P::Qualify(binding) && binding.image == selectedImage && binding.flags == selectedImage + P::FlagsRva,
          "qualify exact controlled native field/code/name relationships");
    Check(P::Revalidate(binding), "revalidate the immutable same-image binding");
    const auto refusal = [&]
    {
        P::Binding canary{11, 22};
        Check(!P::Qualify(canary) && canary.image == 11 && canary.flags == 22, "failed qualification preserves binding output");
        Check(!P::Revalidate(binding), "changed native proof invalidates existing binding");
    };
    const auto byteRefusal = [&](std::uintptr_t rva)
    {
        const auto original = owned.bytes[rva];
        owned.bytes[rva] ^= 1;
        refusal();
        owned.bytes[rva] = original;
    };
    for (const auto& pin : P::Code)
        byteRefusal(pin.rva);
    for (const auto& registration : P::Registrations)
    {
        byteRefusal(registration.instructionRva);
        byteRefusal(registration.instructionRva + 3);
        byteRefusal(registration.nameRva);
        byteRefusal(registration.nameRva + std::strlen(registration.name));
    }
    for (const auto& member : P::Members)
    {
        byteRefusal(P::DispatchTableRva + member.nativeIndex * 8);
        owned.bytes[P::FlagsRva + member.nativeIndex] = 2;
        refusal();
        owned.bytes[P::FlagsRva + member.nativeIndex] = 1;
        Check(P::Revalidate(binding), "active native binary flags are valid, not forced back to default");
        owned.bytes[P::FlagsRva + member.nativeIndex] = 0;
    }
    buildAllowed = false;
    P::Binding hashCanary{11, 22};
    Check(!P::Qualify(hashCanary) && hashCanary.image == 11 && hashCanary.flags == 22, "wrong executable cannot acquire a new native binding");
    Check(P::Revalidate(binding), "revalidation intentionally uses live proof rather than repeating the disk hash");
    buildAllowed = true;
    denyReads = true;
    refusal();
    denyReads = false;
    denyCode = true;
    refusal();
    denyCode = false;
    denyData = true;
    refusal();
    denyData = false;
    const auto mutate = [&](auto& field, auto value)
    {
        const auto original = field;
        field = value;
        refusal();
        field = original;
    };
    auto& nt = owned.Headers();
    mutate(reinterpret_cast<IMAGE_DOS_HEADER*>(owned.bytes)->e_magic, WORD{0});
    mutate(reinterpret_cast<IMAGE_DOS_HEADER*>(owned.bytes)->e_lfanew, LONG{-1});
    mutate(nt.Signature, DWORD{0});
    mutate(nt.FileHeader.Machine, WORD{IMAGE_FILE_MACHINE_I386});
    mutate(nt.FileHeader.NumberOfSections, WORD{97});
    mutate(nt.OptionalHeader.Magic, WORD{0});
    mutate(nt.OptionalHeader.SizeOfImage, DWORD{1});
    mutate(nt.FileHeader.SizeOfOptionalHeader, WORD{0});
    mutate(owned.Sections()[0].Characteristics, DWORD{IMAGE_SCN_MEM_READ});
    mutate(owned.Sections()[1].Characteristics, DWORD{IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE});
    mutate(owned.Sections()[2].Characteristics, DWORD{IMAGE_SCN_MEM_READ});
    auto changed = binding;
    ++changed.image;
    Check(!P::Revalidate(changed), "changed image identity refuses");
    changed = binding;
    ++changed.flags;
    Check(!P::Revalidate(changed), "array origin cannot drift to a neighboring field");
    const auto beforeReads = readCalls;
    Check(P::Revalidate(binding), "measure full checked profile read sequence");
    const auto count = readCalls - beforeReads;
    for (unsigned index = 1; index <= count; ++index)
    {
        failReadAt = readCalls + index;
        Check(!P::Revalidate(binding), "every partial controlled image copy refuses");
        failReadAt = 0;
    }

    Access     access(binding, AccessPages);
    const auto checkNeighbors = [&]
    {
        for (std::size_t index = 0; index < P::NativeFlagCount; ++index)
        {
            bool known = false;
            for (const auto& member : P::Members)
                known |= index == member.nativeIndex;
            if (!known)
                Check(owned.bytes[P::FlagsRva + index] == 0xa5, "writes preserve every other native cheat flag");
        }
    };
    for (std::size_t index = 0; index < P::Members.size(); ++index)
    {
        const auto selected = static_cast<P::Flag>(index);
        const auto rva = P::FlagsRva + P::Members[index].nativeIndex;
        bool       output = true;
        Check(access.Read(selected, output) && !output, "read the selected actual zero native flag");
        Check(access.WriteIf(selected, false, true) == Access::WriteResult::Written && owned.bytes[rva] == 1,
              "atomic write touches exactly the selected one-byte flag");
        Check(access.Read(selected, output) && output, "readback observes the actual enabled native byte");
        Check(access.WriteIf(selected, false, false) == Access::WriteResult::Conflict && owned.bytes[rva] == 1,
              "stale expected value preserves a foreign or newer native value");
        Check(access.WriteIf(selected, true, true) == Access::WriteResult::Unchanged && owned.bytes[rva] == 1, "same-value operation is explicitly unchanged");
        Check(access.WriteIf(selected, true, false) == Access::WriteResult::Written && owned.bytes[rva] == 0,
              "owned restoration compares its last native value");
        denyAccessRead = true;
        output = true;
        Check(!access.Read(selected, output) && output, "refused read preserves caller output");
        denyAccessRead = false;
        denyAccessWrite = true;
        Check(access.WriteIf(selected, false, true) == Access::WriteResult::Refused && owned.bytes[rva] == 0, "refused write cannot alter native data");
        denyAccessWrite = false;
        foreignBeforeCompare = true;
        Check(access.WriteIf(selected, false, true) == Access::WriteResult::Conflict && owned.bytes[rva] == 2,
              "foreign edit after page admission survives compare/write");
        output = true;
        Check(!access.Read(selected, output) && output, "nonbinary native flag is explicit refusal");
        owned.bytes[rva] = 0;
        unconfirmWrite = true;
        Check(access.WriteIf(selected, false, true) == Access::WriteResult::Unconfirmed && owned.bytes[rva] == 1,
              "failed readback retains explicit mutation debt");
        denyAccessRead = false;
        Check(access.WriteIf(selected, true, false) == Access::WriteResult::Written, "an unconfirmed mutation can be restored by its retained owner");
        faultBeforeCompare = true;
        Check(access.WriteIf(selected, false, true) == Access::WriteResult::Refused, "protection race around atomic byte write refuses without process crash");
        DWORD ignored = 0;
        Check(VirtualProtect(owned.bytes + rva, 1, faultProtection, &ignored) != FALSE && owned.bytes[rva] == 0,
              "faulted compare/write preserved original bytes");
    }
    checkNeighbors();
    for (const auto bad :
         {P::Binding{}, P::Binding{1, 2}, P::Binding{(std::numeric_limits<std::uintptr_t>::max)() - 1, 0}, P::Binding{binding.image, binding.flags + 1}})
    {
        Access     refused(bad, AccessPages);
        bool       output = true;
        const auto before = accessPages;
        Check(!refused.Read(P::Flag::HoverCars, output) && output, "malformed borrowed binding preserves read output");
        Check(refused.WriteIf(P::Flag::HoverCars, false, true) == Access::WriteResult::Refused && accessPages == before,
              "malformed borrowed binding refuses before page policy");
    }
    bool       output = true;
    const auto before = accessPages;
    Check(!access.Read(static_cast<P::Flag>(255), output) && output && accessPages == before, "invalid group enum never escapes four known native fields");
    Access noPages(binding, nullptr);
    Check(!noPages.Read(P::Flag::HoverCars, output) && output, "missing borrowed page policy refuses");
    Check(P::Revalidate(binding), "field operations never mutate the native profile or neighboring flags");
    std::printf("Passed %u native cheat flag checks (64-bit; controlled-image)\n", checks);
#endif
    return 0;
}

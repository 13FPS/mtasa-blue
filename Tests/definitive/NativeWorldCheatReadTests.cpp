/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual fourteen-row native cheat reads in owned image fixtures
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
            for (const auto& member : P::ReadMembers)
            {
                std::memcpy(bytes + member.consumer.rva, member.consumer.bytes.data(), member.consumer.size);
                const std::uint64_t action = member.actionRva ? selectedImage + member.actionRva : 0;
                std::memcpy(bytes + P::DispatchTableRva + member.nativeIndex * sizeof(action), &action, sizeof(action));
                bytes[P::FlagsRva + member.nativeIndex] = 0;
            }
            for (const auto& registered : P::ReadRegistrations)
            {
                const auto&                  registration = registered.native;
                std::array<unsigned char, 7> instruction{0x4c, 0x8d, 0x05};
                const auto                   displacement = static_cast<std::int32_t>(registration.nameRva - registration.instructionRva - instruction.size());
                std::memcpy(instruction.data() + 3, &displacement, sizeof(displacement));
                std::memcpy(bytes + registration.instructionRva, instruction.data(), instruction.size());
                std::memcpy(bytes + registration.nameRva, registration.name, std::strlen(registration.name) + 1);
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
        if (!writable)
            for (const auto& member : P::ReadMembers)
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
    using Policy = MtaGame::CWorldCheatPolicy;
    using Id = Policy::Id;
    static_assert(P::ReadMembers.size() == 14 && P::Members.size() == 4);
    unsigned supported = 0, unavailable = 0;
    for (const auto& logical : Policy::Members)
    {
        const auto* native = P::FindRead(logical.id);
        if (native)
        {
            ++supported;
            Check(native->id == logical.id && native->nativeIndex < P::NativeFlagCount, "logical MTA identities resolve only to the exact admitted native row");
        }
        else
            ++unavailable;
    }
    Check(supported == 14 && unavailable == 5, "fourteen known byte rows never manufacture bindings for five unresolved names");
    Check(!P::FindRead(Id::Count) && !P::FindRead(static_cast<Id>(255)), "invalid logical identity refuses");
    P::Binding binding{11, 22};
#if !defined(MTA_TEST_CHEAT_CONTROLLED)
    Check(!P::Qualify(binding) && binding.image == 11 && binding.flags == 22, "shipping image admission refuses an ordinary host");
    for (const auto& member : P::ReadMembers)
        Check(!P::RevalidateRead(binding, member.id), "a wrong-image binding cannot qualify a named native read");
    std::printf("Passed %u native world cheat read checks (64-bit; shipping-wrong-image)\n", checks);
#else
    OwnedImage owned;
    Check(P::Qualify(binding), "existing exact-build qualification establishes the owned fixture binding");
    Access     access(binding, AccessPages);
    const auto snapshot = [&]
    {
        std::array<unsigned char, P::NativeFlagCount> value{};
        std::memcpy(value.data(), owned.bytes + P::FlagsRva, value.size());
        return value;
    };
    for (const auto& member : P::ReadMembers)
    {
        Check(P::RevalidateRead(binding, member.id), "real member code, names, slot and image qualify a fresh named read");
        const auto field = P::FlagsRva + member.nativeIndex;
        for (const auto raw : {0u, 1u, 2u, 255u})
        {
            owned.bytes[field] = static_cast<unsigned char>(raw);
            const auto before = snapshot();
            bool       value = raw == 0;
            Check(P::RevalidateRead(binding, member.id) && access.ReadWorld(member.id, value) && value == (raw != 0),
                  "actual IsCheatEnabled read semantics preserve every raw nonzero byte, not only canonical1");
            Check(snapshot() == before, "profile/read path performs no flag or neighbor writes");
        }
        owned.bytes[field] = 0;
        const auto mutate = [&](std::uintptr_t rva)
        {
            const auto saved = owned.bytes[rva];
            owned.bytes[rva] ^= 1;
            Check(!P::RevalidateRead(binding, member.id), "changed selected native proof refuses rather than publishing disabled state");
            owned.bytes[rva] = saved;
        };
        mutate(member.consumer.rva);
        mutate(P::DispatchTableRva + member.nativeIndex * sizeof(std::uint64_t));
        for (const auto& registered : P::ReadRegistrations)
            if (registered.id == member.id)
            {
                mutate(registered.native.instructionRva);
                mutate(registered.native.instructionRva + 3);
                mutate(registered.native.nameRva);
                mutate(registered.native.nameRva + std::strlen(registered.native.name));
            }
        bool value = true;
        denyAccessRead = true;
        Check(!access.ReadWorld(member.id, value) && value, "refused borrowed read preserves output");
        denyAccessRead = false;
        denyReads = true;
        Check(!P::RevalidateRead(binding, member.id), "refused image copy is not a false native flag");
        denyReads = false;
        denyCode = true;
        Check(!P::RevalidateRead(binding, member.id), "changed or unreadable native code refuses");
        denyCode = false;
    }
    for (const auto& logical : Policy::Members)
        if (!P::FindRead(logical.id))
        {
            bool       value = true;
            const auto pagesBefore = accessPages;
            Check(!P::RevalidateRead(binding, logical.id) && !access.ReadWorld(logical.id, value) && value && accessPages == pagesBefore,
                  "known unavailable rows refuse before any borrowed memory access");
        }
    bool       value = true;
    const auto pagesBefore = accessPages;
    Check(!access.ReadWorld(static_cast<Id>(255), value) && value && accessPages == pagesBefore,
          "invalid identity preserves output and refuses before page callback");
    for (const auto invalid :
         {P::Binding{}, P::Binding{1, 2}, P::Binding{binding.image, binding.flags + 1}, P::Binding{(std::numeric_limits<std::uintptr_t>::max)() - 1, 0}})
    {
        Access bad(invalid, AccessPages);
        Check(!P::RevalidateRead(invalid, Id::TankMode) && !bad.ReadWorld(Id::TankMode, value) && value,
              "missing foreign shifted and overflowing native bindings refuse without replacing output");
    }
    const auto before = snapshot();
    DWORD      previousProtection = 0, ignored = 0;
    Check(VirtualProtect(owned.bytes + P::FlagsRva, P::NativeFlagCount, PAGE_READONLY, &previousProtection) != FALSE,
          "owned native flag page can be read only for a read-only request");
    for (const auto& member : P::ReadMembers)
        Check(P::RevalidateRead(binding, member.id) && access.ReadWorld(member.id, value) && !value,
              "read qualification requires readable native data, not write permission");
    Check(snapshot() == before, "read-only page tests preserve all117 bytes");
    Check(VirtualProtect(owned.bytes + P::FlagsRva, P::NativeFlagCount, previousProtection, &ignored) != FALSE, "restore only this fixture's page protection");
    const auto oldMachine = owned.Headers().FileHeader.Machine;
    owned.Headers().FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    Check(!P::RevalidateRead(binding, Id::TankMode), "foreign PE architecture cannot qualify a read even at matching offsets");
    owned.Headers().FileHeader.Machine = oldMachine;
    buildAllowed = false;
    P::Binding canary{11, 22};
    Check(!P::Qualify(canary) && canary.image == 11 && canary.flags == 22, "wrong disk build cannot create a new borrowed binding");
    buildAllowed = true;
    Check(P::Revalidate(binding), "raw named reads leave the strict four-property write profile unchanged");
    std::printf("Passed %u native world cheat read checks (64-bit; controlled-image)\n", checks);
#endif
    return 0;
}

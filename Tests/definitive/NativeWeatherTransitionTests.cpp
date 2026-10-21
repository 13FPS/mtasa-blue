/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native weather proof, exact-width writes and partial recovery only
 *****************************************************************************/
#include "../../Client/game_de/NativeWeatherTransitionProfile.h"
#include "../../Client/game_de/NativeWeatherTransitionJournal.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{
    namespace P = mta::game_de::NativeWeatherTransitionProfile;
    using Memory = mta::game_de::NativeWeatherTransitionMemory;
    using Journal = mta::game_de::NativeWeatherTransitionJournal;
    using Result = mta::game_de::ClockResult;
    using Field = P::Field;
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
#if defined(MTA_TEST_WEATHER_CONTROLLED)
namespace
{
    std::uintptr_t selectedImage = 0, deniedImageRva = 0;
    bool           buildAllowed = true, denyImageReads = false, denyCode = false, denyData = false;
    unsigned       imageReads = 0, failImageReadAt = 0;
    unsigned       accessCalls = 0, readCalls = 0, writeCalls = 0;
    unsigned       denyReadAt = 0, denyWriteAt = 0, denyWriteFrom = 0, unconfirmWriteAt = 0, foreignWriteAt = 0, faultWriteAt = 0;
    bool           denyReads = false, denyWrites = false, mutateClock = false;
    std::uintptr_t foreignRva = 0;
    std::uint32_t  foreignValue = 0;
    DWORD          faultProtection = 0;
    HMODULE WINAPI FixtureModule(LPCWSTR name)
    {
        return name ? ::GetModuleHandleW(name) : reinterpret_cast<HMODULE>(selectedImage);
    }
    void Store(Field field, std::uint32_t value)
    {
        const auto& info = P::Fields[static_cast<std::size_t>(field)];
        std::memcpy(reinterpret_cast<void*>(selectedImage + info.rva), &value, info.size);
    }
    std::uint32_t Load(Field field)
    {
        std::uint32_t value = 0;
        const auto&   info = P::Fields[static_cast<std::size_t>(field)];
        std::memcpy(&value, reinterpret_cast<const void*>(selectedImage + info.rva), info.size);
        return value;
    }
    void ResetFaults()
    {
        accessCalls = readCalls = writeCalls = 0;
        denyReadAt = denyWriteAt = denyWriteFrom = unconfirmWriteAt = foreignWriteAt = faultWriteAt = 0;
        denyReads = denyWrites = mutateClock = false;
        foreignRva = 0;
        foreignValue = 0;
    }
    void Seed()
    {
        ResetFaults();
        for (const auto& field : P::Fields)
            std::memset(reinterpret_cast<void*>(selectedImage + field.rva - 1), 0xa5, field.size + 2);
        Store(Field::OldType, 2);
        Store(Field::NewType, 3);
        Store(Field::ForcedType, 4);
        Store(Field::PreviousClockSeconds, 175);
        Store(Field::InterpolationBits, 0x3f400000);
        *reinterpret_cast<unsigned char*>(selectedImage + P::MinuteRva) = 8;
        *reinterpret_cast<unsigned char*>(selectedImage + P::SecondRva) = 25;
    }
}
namespace mta::game_de::WeatherFixtureImage
{
    bool CheckExecutableBuild() noexcept
    {
        return buildAllowed;
    }
    bool ImagePages(std::uintptr_t image, std::uintptr_t address, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        if (!selectedImage || image != selectedImage || address < image || address - image > P::ImageSize || size > P::ImageSize - (address - image))
            return false;
        if (deniedImageRva && address <= image + deniedImageRva && size > image + deniedImageRva - address)
            return false;
        return !(writable && denyData) && !(executable && denyCode);
    }
    bool ReadImage(std::uintptr_t image, std::uintptr_t address, void* output, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        ++imageReads;
        if (denyImageReads || imageReads == failImageReadAt || !ImagePages(image, address, size, writable, executable))
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
    #define NativeImageAccess WeatherFixtureImage
    #define GetModuleHandleW  FixtureModule
#endif
#include "../../Client/game_de/NativeWeatherTransitionProfile.cpp"
#if defined(MTA_TEST_WEATHER_CONTROLLED)
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
            nt.FileHeader.NumberOfSections = 2;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            auto* sections = Sections();
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0x3f40000;
            sections[0].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            sections[1].VirtualAddress = 0x3f41000;
            sections[1].Misc.VirtualSize = static_cast<DWORD>(P::ImageSize - 0x3f41000);
            sections[1].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            for (const auto& pin : P::Code)
                std::memcpy(bytes + pin.rva, pin.bytes.data(), pin.size);
            Seed();
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
        ++accessCalls;
        if (!selectedImage || executable || address < selectedImage || address - selectedImage > P::ImageSize ||
            size > P::ImageSize - (address - selectedImage))
            return false;
        bool known = false;
        for (const auto& field : P::Fields)
            known |= address == selectedImage + field.rva && size == field.size;
        known |= !writable && size == 1 && (address == selectedImage + P::MinuteRva || address == selectedImage + P::SecondRva);
        if (!known)
            return false;
        if (writable)
        {
            ++writeCalls;
            if (writeCalls == foreignWriteAt)
                std::memcpy(reinterpret_cast<void*>(selectedImage + foreignRva), &foreignValue, 2);
            if (denyWrites || writeCalls == denyWriteAt || (denyWriteFrom && writeCalls >= denyWriteFrom))
                return false;
            if (writeCalls == unconfirmWriteAt)
                denyReadAt = readCalls + 1;
            if (writeCalls == faultWriteAt && !VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_NOACCESS, &faultProtection))
                std::abort();
        }
        else
        {
            ++readCalls;
            if (denyReads || readCalls == denyReadAt)
                return false;
            if (mutateClock && address == selectedImage + P::SecondRva)
                ++*reinterpret_cast<unsigned char*>(selectedImage + P::SecondRva);
        }
        return true;
    }
    void CheckNeighbors(const OwnedImage& image)
    {
        for (const auto& field : P::Fields)
            Check(image.bytes[field.rva - 1] == 0xa5 && image.bytes[field.rva + field.size] == 0xa5, "native writes preserve adjacent padding/fields");
    }
}
#endif
int main()
{
    P::Binding binding{11};
#if !defined(MTA_TEST_WEATHER_CONTROLLED)
    Check(!P::Qualify(binding) && binding.image == 11, "shipping image hash refuses ordinary owned host without publishing a binding");
    Check(!P::Revalidate(binding), "shipping live profile cannot borrow an ordinary executable");
    std::printf("Passed %u native weather transition checks (64-bit; shipping-wrong-image)\n", checks);
#else
    OwnedImage image;
    Check(P::Qualify(binding) && binding.image == selectedImage, "qualify controlled exact native code/data relationships");
    Check(P::Revalidate(binding), "revalidate same immutable image binding");
    const auto refusal = [&]
    {
        P::Binding canary{11};
        Check(!P::Qualify(canary) && canary.image == 11, "failed qualification preserves output binding");
        Check(!P::Revalidate(binding), "changed proof refuses existing binding");
    };
    const auto byteRefusal = [&](std::uintptr_t rva)
    {
        const auto original = image.bytes[rva];
        image.bytes[rva] ^= 1;
        refusal();
        image.bytes[rva] = original;
    };
    for (const auto& pin : P::Code)
    {
        byteRefusal(pin.rva);
        byteRefusal(pin.rva + pin.size - 1);
    }
    byteRefusal(0);
    byteRefusal(0x100);
    byteRefusal(0x100 + offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, Machine));
    byteRefusal(0x100 + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + offsetof(IMAGE_OPTIONAL_HEADER64, Magic));
    const auto sections = image.Headers().FileHeader.NumberOfSections;
    image.Headers().FileHeader.NumberOfSections = 97;
    refusal();
    image.Headers().FileHeader.NumberOfSections = sections;
    image.Sections()[0].Characteristics |= IMAGE_SCN_MEM_WRITE;
    refusal();
    image.Sections()[0].Characteristics &= ~IMAGE_SCN_MEM_WRITE;
    image.Sections()[1].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
    refusal();
    image.Sections()[1].Characteristics &= ~IMAGE_SCN_MEM_EXECUTE;
    for (const auto& field : P::Fields)
    {
        deniedImageRva = field.rva;
        refusal();
    }
    deniedImageRva = P::TableRva;
    refusal();
    deniedImageRva = P::MinuteRva;
    refusal();
    deniedImageRva = P::SecondRva;
    refusal();
    deniedImageRva = 0;
    denyImageReads = true;
    refusal();
    denyImageReads = false;
    denyCode = true;
    refusal();
    denyCode = false;
    denyData = true;
    refusal();
    denyData = false;
    imageReads = 0;
    Check(P::Revalidate(binding), "profile healthy after each independent refusal");
    const auto qualifiedReads = imageReads;
    for (unsigned read = 1; read <= qualifiedReads; ++read)
    {
        imageReads = 0;
        failImageReadAt = read;
        Check(!P::Revalidate(binding), "each actual profile read failure refuses qualification");
    }
    failImageReadAt = 0;
    buildAllowed = false;
    P::Binding unchanged{19};
    Check(!P::Qualify(unchanged) && unchanged.image == 19, "disk executable mismatch cannot grant authority");
    Check(P::Revalidate(binding), "live revalidation does not repeat disk executable hashing");
    buildAllowed = true;
    Check(!P::Revalidate({selectedImage + 8}), "existing binding cannot substitute another image base");
    // Values are admission policy, not static pins: cleanup must be able to
    // relinquish later native values even if they no longer fit SDK bounds.
    Store(Field::OldType, 0xffff);
    Check(P::Revalidate(binding), "later invalid data cannot block static proof and partial-debt retirement");
    Seed();
    const Memory     memory(binding, AccessPages);
    std::uint32_t    value = 0x12345678;
    Memory::Snapshot snapshot{};
    Check(memory.Read(snapshot) && snapshot.bits == std::array<std::uint32_t, 5>{2, 3, 4, 175, 0x3f400000},
          "matching native-width snapshot uses actual owned memory");
    std::uint16_t clock = 0;
    Check(memory.ReadClockSeconds(clock) && clock == 505, "resync clock follows actual minute/second byte consumers");
    image.bytes[P::SecondRva + 1] = 0xff;
    Check(memory.ReadClockSeconds(clock) && clock == 505, "native weather clock ignores neighboring high byte");
    image.bytes[P::SecondRva + 1] = 0;
    image.bytes[P::MinuteRva] = 60;
    image.bytes[P::SecondRva] = 59;
    Check(memory.ReadClockSeconds(clock) && clock == 3659, "native minute60 remains valid at clock boundary");
    image.bytes[P::MinuteRva] = 61;
    clock = 777;
    Check(!memory.ReadClockSeconds(clock) && clock == 777, "invalid native minute preserves output");
    Seed();
    image.bytes[P::SecondRva] = 60;
    Check(!memory.ReadClockSeconds(clock) && clock == 777, "invalid native second refuses resync");
    Seed();
    mutateClock = true;
    Check(!memory.ReadClockSeconds(clock) && clock == 777, "changing native clock cannot produce a torn sample");
    Seed();
    denyReads = true;
    Check(!memory.Read(Field::OldType, value) && value == 0x12345678, "page refusal preserves scalar output");
    ResetFaults();
    const Memory invalid({0}, AccessPages), overflow({(std::numeric_limits<std::uintptr_t>::max)() - 1}, AccessPages);
    const auto   calls = accessCalls;
    Check(!invalid.Read(Field::OldType, value) && !overflow.Read(Field::OldType, value) && accessCalls == calls,
          "invalid image bounds refuse before page callbacks");
    Check(!memory.Read(static_cast<Field>(255), value) && accessCalls == calls, "unknown field cannot become a caller-chosen pointer");
    const Memory noPolicy(binding, nullptr);
    Check(!noPolicy.Read(Field::OldType, value), "borrowed memory without page policy grants no access");
    for (std::size_t index = 0; index < P::Fields.size(); ++index)
    {
        Seed();
        const auto          field = static_cast<Field>(index);
        const auto          original = Load(field);
        const std::uint32_t replacement = index == 4 ? 0x3e800000 : 12;
        Check(memory.WriteIf(field, original, replacement) == Memory::WriteResult::Written && Load(field) == replacement,
              "exact native-width CAS writes confirmed replacement");
        Check(memory.WriteIf(field, original, 8) == Memory::WriteResult::Conflict && Load(field) == replacement, "stale expected value preserves a later edit");
        Check(memory.WriteIf(field, replacement, replacement) == Memory::WriteResult::Unchanged, "identical native value is a confirmed no-op");
        CheckNeighbors(image);
    }
    Seed();
    for (const auto field : {Field::OldType, Field::NewType, Field::ForcedType})
        for (std::uint32_t type = P::WeatherTypeCount; type < 256; ++type)
            Check(memory.WriteIf(field, Load(field), type) == Memory::WriteResult::Refused && writeCalls == 0,
                  "native23-column bound rejects malformed SDK bytes before any write admission");
    Check(memory.WriteIf(Field::OldType, 2, 0xffff) == Memory::WriteResult::Refused, "old/new cannot borrow forced sentinel");
    Check(memory.WriteIf(Field::ForcedType, 4, 0xffff) == Memory::WriteResult::Written && Load(Field::ForcedType) == 0xffff,
          "forced release is full signed native WORD sentinel");
    Check(memory.WriteIf(Field::OldType, 0x10002, 1) == Memory::WriteResult::Refused, "native WORD expected value cannot silently truncate");
    Seed();
    unconfirmWriteAt = 1;
    Check(memory.WriteIf(Field::OldType, 2, 5) == Memory::WriteResult::Unconfirmed && Load(Field::OldType) == 5,
          "failed readback retains distinction from refused mutation");
    Seed();
    faultWriteAt = 1;
    Check(memory.WriteIf(Field::OldType, 2, 5) == Memory::WriteResult::Refused, "protection race after admission is caught without unsafe write");
    DWORD ignored = 0;
    Check(VirtualProtect(image.bytes + P::Fields[0].rva, 2, faultProtection, &ignored) != FALSE, "restore only owned fixture protection after race");
    Check(Load(Field::OldType) == 2, "refused protection race did not alter native width");
    Seed();
    {
        Journal journal;
        Check(journal.ReadForced(memory, clock) == Result::Ok && clock == 4, "journal reads actual forced type");
        Check(journal.SetTypes(memory, 21, 22) == Result::Ok && Load(Field::OldType) == 21 && Load(Field::NewType) == 22 && Load(Field::ForcedType) == 4,
              "native extra-colour columns21/22 are valid persistent SDK setters without forced side effects");
        Check(!journal.HasPending() && journal.Recover(memory) == Result::Ok && Load(Field::OldType) == 21 && Load(Field::NewType) == 22,
              "successful SDK setter never becomes generic End-time rollback");
        Check(journal.ReleaseForced(memory) == Result::Ok && Load(Field::ForcedType) == 0xffff, "release persists only native forced signed sentinel");
        Check(journal.ReadForced(memory, clock) == Result::Ok && clock == 0xffff, "native forced release remains distinguishable for SDK low-byte mapping");
        Check(journal.ResyncWithClock(memory) == Result::Ok && Load(Field::PreviousClockSeconds) == 505 && Load(Field::InterpolationBits) == 0 &&
                  Load(Field::OldType) == 21 && Load(Field::NewType) == 22 && Load(Field::ForcedType) == 0xffff,
              "resync updates only paired integer wrap bookend and float interpolation");
        CheckNeighbors(image);
    }
    for (std::uint32_t type = P::WeatherTypeCount; type < 256; ++type)
    {
        Seed();
        Journal journal;
        Check(journal.SetTypes(memory, static_cast<std::uint8_t>(type), 2) == Result::InvalidArgument && accessCalls == 0,
              "invalid primary SDK index refuses before reads or mutation");
        Check(journal.SetTypes(memory, 2, static_cast<std::uint8_t>(type)) == Result::InvalidArgument && accessCalls == 0,
              "invalid secondary SDK index refuses before reads or mutation");
    }
    for (const auto field : {Field::OldType, Field::NewType, Field::ForcedType})
    {
        Seed();
        Journal journal;
        Store(field, 23);
        clock = 777;
        const auto result = field == Field::ForcedType ? journal.ReadForced(memory, clock) : journal.SetTypes(memory, 5, 6);
        Check(result == Result::BindingChanged && writeCalls == 0 && clock == 777,
              "new SDK operation refuses invalid current native type without publishing false value");
    }
    Seed();
    {
        Journal journal;
        denyWriteAt = 1;
        Check(journal.SetTypes(memory, 5, 6) == Result::WriteFailure && !journal.HasPending() && Load(Field::OldType) == 2 && Load(Field::NewType) == 3,
              "refused first write has no invented recovery debt");
    }
    Seed();
    {
        Journal journal;
        denyWriteAt = 2;
        Check(journal.SetTypes(memory, 5, 6) == Result::WriteFailure && !journal.HasPending() && Load(Field::OldType) == 2 && Load(Field::NewType) == 3,
              "partial pair failure immediately restores only first owned scalar");
        Check(writeCalls == 3, "failed second write uses one narrow reverse rollback");
    }
    for (const auto phase : {1u, 2u})
    {
        Seed();
        Journal journal;
        unconfirmWriteAt = phase;
        Check(journal.SetTypes(memory, 5, 6) == Result::WriteFailure && !journal.HasPending() && Load(Field::OldType) == 2 && Load(Field::NewType) == 3,
              "unconfirmed write is journaled before rollback instead of forgotten");
    }
    Seed();
    {
        Journal journal;
        denyWriteFrom = 2;
        Check(journal.SetTypes(memory, 5, 6) == Result::RestoreFailure && journal.PendingCount() == 1 && Load(Field::OldType) == 5,
              "failed reverse write retains actual partial debt");
        Check(journal.SetTypes(memory, 7, 8) == Result::Busy && journal.ReleaseForced(memory) == Result::Busy &&
                  journal.ResyncWithClock(memory) == Result::Busy && journal.ReadForced(memory, clock) == Result::Busy,
              "no other SDK operation crosses unresolved partial transaction");
        const Memory other({selectedImage + 8}, AccessPages);
        Check(journal.Recover(other) == Result::BindingChanged && journal.PendingCount() == 1, "debt cannot move to another image identity");
        ResetFaults();
        Check(journal.Recover(memory) == Result::Ok && !journal.HasPending() && Load(Field::OldType) == 2,
              "later owned callback resolves exact remaining scalar debt");
    }
    Seed();
    {
        Journal journal;
        denyWriteAt = 2;
        denyReadAt = 6;
        Check(journal.SetTypes(memory, 5, 6) == Result::RestoreFailure && journal.HasPending(),
              "unreadable partial scalar retains debt even though writes may be available");
        ResetFaults();
        Store(Field::OldType, 23);
        Check(P::Revalidate(binding), "superseding invalid scalar cannot invalidate immutable cleanup proof");
        Check(journal.Recover(memory) == Result::OwnerConflict && !journal.HasPending() && journal.SupersededCount() == 1 && Load(Field::OldType) == 23,
              "later foreign scalar is preserved and retires only its own debt with explicit supersession");
    }
    Seed();
    {
        Journal journal;
        denyWriteAt = foreignWriteAt = 2;
        foreignRva = P::Fields[0].rva;
        foreignValue = 19;
        Check(journal.SetTypes(memory, 5, 6) == Result::OwnerConflict && !journal.HasPending() && journal.SupersededCount() == 1 && Load(Field::OldType) == 19,
              "same-callback foreign edit survives immediate reverse recovery without pinning owner");
        ResetFaults();
        Check(journal.SetTypes(memory, 7, 8) == Result::Ok && journal.SupersededCount() == 0, "admitted next operation resets prior supersession metric");
    }
    Seed();
    {
        Journal journal;
        unconfirmWriteAt = 2;
        denyWriteFrom = 3;
        Check(journal.SetTypes(memory, 5, 6) == Result::RestoreFailure && journal.PendingCount() == 2,
              "unconfirmed second write and failed two reverse writes retain two entry journal");
        ResetFaults();
        Store(Field::NewType, 22);
        denyReadAt = 2;
        Check(journal.Recover(memory) == Result::RestoreFailure && journal.PendingCount() == 1 && journal.SupersededCount() == 1 && Load(Field::NewType) == 22,
              "supersession clears only its own scalar while another unreadable debt remains");
        ResetFaults();
        Check(journal.Recover(memory) == Result::Ok && !journal.HasPending() && journal.SupersededCount() == 0 && Load(Field::OldType) == 2 &&
                  Load(Field::NewType) == 22,
              "remaining still-owned scalar restores without overwriting earlier superseding native value");
    }
    Seed();
    {
        Journal journal;
        denyWriteFrom = 2;
        Check(journal.ResyncWithClock(memory) == Result::RestoreFailure && journal.HasPending() && Load(Field::PreviousClockSeconds) == 505,
              "clock resync pair shares same partial rollback debt contract");
        ResetFaults();
        Check(journal.Recover(memory) == Result::Ok && Load(Field::PreviousClockSeconds) == 175 && Load(Field::InterpolationBits) == 0x3f400000,
              "clock resync failure restores exact original integer and float bits");
    }
    CheckNeighbors(image);
    std::printf("Passed %u native weather transition checks (64-bit; controlled-image)\n", checks);
#endif
    return 0;
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native streaming predicate over real owned image pages and owner checks
 ****************************************************************************/
#define main NativeModelLoadedBankFixtureMain
#include "NativeModelBankTests.cpp"
#undef main
#include "../../Client/game_de/NativeModelLoadedReader.h"
#include "../../Client/game_de/NativeModelLoadedProfile.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeModelLoadedReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace L = NativeModelLoadedProfile;
    using LoadedReader = NativeModelLoadedReader;
    struct LoadedFixture
    {
        BankFixture bank;
        explicit LoadedFixture(const std::filesystem::path& directory) : bank(directory)
        {
            for (const auto& pin : L::Code)
                bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        void Store(std::uint32_t index, unsigned char flags, unsigned char state)
        {
            bank.Global(L::ArrayRva + static_cast<std::uintptr_t>(index) * L::RowStride + L::FlagsOffset, std::array<unsigned char, 2>{flags, state});
        }
        void Reset()
        {
            bank.Reset();
            Store(0, 0, 0);
            Store(17, 0x96, 1);
            Store(L::ModelCount - 1, 0, 0);
        }
    };
    LoadedReader::Value LoadedSentinel()
    {
        return {0xdeadbeefu, 0xabcd, 0xcd, 0xef, true};
    }
    bool LoadedUntouched(const LoadedReader::Value& value)
    {
        const auto expected = LoadedSentinel();
        return value.modelIndex == expected.modelIndex && value.rowAddress == expected.rowAddress && value.rawFlags == expected.rawFlags &&
               value.rawLoadState == expected.rawLoadState && value.streamedIn == expected.streamedIn;
    }
    LoadedFixture* changingLoaded = nullptr;
    unsigned       loadedChange = 0;
    void           ChangeLoaded()
    {
        if (loadedChange == 0)
            changingLoaded->Store(17, 0x96, 0);
        if (loadedChange == 1)
            changingLoaded->Store(17, 0x95, 1);
        if (loadedChange == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "loaded predicate evidence remains in an owned absolute workspace directory");
    LoadedReader::Binding refused{123};
    Check(!LoadedReader::Qualify(refused) && refused.image == 123, "shipping loaded-query qualification refuses the actual non-game host without output");
    LoadedFixture fixture(argv[1]);
    PoolTestHost::image = fixture.bank.transform.image.image;
    const LoadedReader::Binding binding{fixture.bank.transform.image.image};
    LoadedReader                reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Check(LoadedReader::Revalidate(binding), "actual row initializer, bank bound, independent state1 consumers and native state-clear code qualify");
    auto value = LoadedSentinel();
    Check(unowned.Read(17, value) == ClockResult::NotInitialized && LoadedUntouched(value), "streaming bytes do not supply their own callback owner");
    for (unsigned state = 0; state != 256; ++state)
    {
        fixture.Reset();
        const auto flags = static_cast<unsigned char>(state ^ 0xa5u);
        fixture.Store(17, flags, static_cast<unsigned char>(state));
        value = LoadedSentinel();
        Check(reader.Read(17, value) == ClockResult::Ok && value.modelIndex == 17 && value.rowAddress == binding.image + L::ArrayRva + 17 * L::RowStride &&
                  value.rawFlags == flags && value.rawLoadState == state && value.streamedIn == (state == 1),
              "all raw state/flag bytes survive; streamedIn is exactly the native state1 predicate rather than any nonzero state");
        Check(reader.Recheck(value) == ClockResult::Ok, "an unchanged immediate raw streaming tuple rechecks inside the owner");
    }
    for (const auto index : {std::uint32_t{0},     std::uint32_t{1},     std::uint32_t{19999}, std::uint32_t{20000}, std::uint32_t{24999}, std::uint32_t{25000},
                             std::uint32_t{25254}, std::uint32_t{25255}, std::uint32_t{25510}, std::uint32_t{25511}, std::uint32_t{25574}, std::uint32_t{25575},
                             std::uint32_t{25754}, std::uint32_t{25755}, std::uint32_t{26229}, std::uint32_t{26230}, L::ModelCount - 5,    L::ModelCount - 4,
                             L::ModelCount - 3,    L::ModelCount - 2,    L::ModelCount - 1})
        for (const auto state : {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(2), static_cast<unsigned char>(255)})
        {
            fixture.Reset();
            fixture.Store(index, 0xff, state);
            value = LoadedSentinel();
            Check(reader.Read(index, value) == ClockResult::Ok &&
                      value.rowAddress == binding.image + L::ArrayRva + static_cast<std::uintptr_t>(index) * L::RowStride && value.rawLoadState == state &&
                      value.rawFlags == 0xff && value.streamedIn == (state == 1),
                  "DFF,TXD,COL,IPL,DAT,IFP,RRR,SCM and all four sentinel boundaries use the actual complete16byte resource domain");
        }
    for (const auto index : {L::ModelCount, L::ModelCount + 1, std::uint32_t{32768}, std::uint32_t{65535}, std::uint32_t{65536}, 0xffffffffu})
    {
        fixture.Reset();
        value = LoadedSentinel();
        Check(reader.Read(index, value) == ClockResult::InvalidArgument && LoadedUntouched(value) && PoolTestHost::reads == 0 && PoolTestHost::ownerCalls == 0,
              "full unsigned key refuses beyond-final-row, rawffff and truncation aliases before reads or owner callbacks");
    }
    fixture.Reset();
    fixture.bank.Slot(17, 0);
    value = LoadedSentinel();
    Check(reader.Read(17, value) == ClockResult::Ok && value.streamedIn,
          "state1 observation remains independent of absent model/render object and grants no SDK IsLoaded success");
    fixture.Reset();
    Check(reader.Read(17, value) == ClockResult::Ok, "measure actual loaded reader read/owner boundaries");
    const auto readCount = PoolTestHost::reads, ownerCount = PoolTestHost::ownerCalls;
    for (unsigned fail = 1; fail <= readCount; ++fail)
    {
        fixture.Reset();
        PoolTestHost::failAt = fail;
        value = LoadedSentinel();
        Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value), "every failed real checked image read preserves the caller output");
    }
    for (unsigned revoke = 1; revoke <= ownerCount; ++revoke)
    {
        fixture.Reset();
        PoolTestHost::revokeAt = revoke;
        value = LoadedSentinel();
        Check(reader.Read(17, value) == ClockResult::WrongThread && LoadedUntouched(value),
              "every callback owner bookend may revoke without publishing stream state");
    }
    for (unsigned state = 0; state != 9; ++state)
    {
        fixture.Reset();
        fixture.bank.Global(NativeGameStateProfile::stateRva, static_cast<std::uint32_t>(state));
        value = LoadedSentinel();
        Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value),
              "shared state qualification rejects unsupported states and all qualified non-playing states");
    }
    fixture.Reset();
    fixture.bank.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    value = LoadedSentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value), "native GC marker refuses ordinary model state output");
    fixture.Reset();
    fixture.bank.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
    value = LoadedSentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value), "an uninitialized native GameThread pair refuses loading facts");
    fixture.Reset();
    value = LoadedSentinel();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = reader.Read(17, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && LoadedUntouched(value), "a producer thread cannot replace the genuine initialized native GameThread");
    for (loadedChange = 0; loadedChange != 3; ++loadedChange)
    {
        fixture.Reset();
        changingLoaded = &fixture;
        PoolTestHost::watched = binding.image + L::ArrayRva + 17 * L::RowStride + L::FlagsOffset;
        PoolTestHost::after = &ChangeLoaded;
        value = LoadedSentinel();
        Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value) && PoolTestHost::after == nullptr,
              "between-read state change, flag change or owner expiry prevents publication");
    }
    fixture.Reset();
    Check(reader.Read(17, value) == ClockResult::Ok, "seed tuple for recheck mutation");
    auto forged = value;
    ++forged.rowAddress;
    Check(reader.Recheck(forged) == ClockResult::BindingChanged, "recheck recomputes the native row address instead of trusting supplied addresses");
    forged = value;
    forged.streamedIn = false;
    Check(reader.Recheck(forged) == ClockResult::BindingChanged, "derived predicate cannot disagree with the captured raw state");
    fixture.Store(17, 0x96, 0);
    Check(reader.Recheck(value) == ClockResult::BindingChanged, "native unloading state invalidates a formerly loaded snapshot");
    fixture.Reset();
    const auto address = binding.image + L::ArrayRva + 17 * L::RowStride + L::FlagsOffset;
    DWORD      previous = 0, ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(address), 2, PAGE_READONLY, &previous) != FALSE, "make only owned stream row page readonly");
    value = LoadedSentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value), "unexpected readonly native data page refuses the writable-image contract");
    Check(VirtualProtect(reinterpret_cast<void*>(address), 2, previous, &ignored) != FALSE, "restore only owned stream row protection");
    fixture.Reset();
    IMAGE_SECTION_HEADER dataSection{};
    const auto           sectionAt = 0x80u + sizeof(IMAGE_NT_HEADERS64) + 2 * sizeof(IMAGE_SECTION_HEADER);
    std::memcpy(&dataSection, reinterpret_cast<void*>(binding.image + sectionAt), sizeof(dataSection));
    auto shortSection = dataSection;
    shortSection.Misc.VirtualSize = static_cast<DWORD>(L::ArrayRva + L::ArrayBytes - shortSection.VirtualAddress - 1);
    fixture.bank.transform.image.Patch(sectionAt, &shortSection, sizeof(shortSection), PAGE_READONLY);
    value = LoadedSentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value),
          "whole initialized resource-row streaming domain must fit the qualified image data section even for an earlier queried row");
    fixture.bank.transform.image.Patch(sectionAt, &dataSection, sizeof(dataSection), PAGE_READONLY);
    for (const auto& pin : L::Code)
    {
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
        fixture.bank.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        value = LoadedSentinel();
        Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value), "each changed initializer/consumer/retirement pin refuses stream output");
        fixture.bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    fixture.Reset();
    PoolTestHost::image = 0;
    value = LoadedSentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && LoadedUntouched(value), "a changed actual main image cannot reuse a loaded-state snapshot");
    std::puts("Production native model loaded primitive fixture completed");
    std::printf("Passed %u native model-loaded checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}

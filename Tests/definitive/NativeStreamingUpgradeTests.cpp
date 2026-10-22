/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real paired-upgrade guards and single-entry effect accounting
 ****************************************************************************/
// Reuse the same actual owned image/registry/model fixture. Its old main is
// retained only to keep its helpers referenced; this suite does not rerun it.
#pragma push_macro("main")
#undef main
#define main HistoricalNativeStreamingRequestFixtureMain
#include "NativeStreamingRequestTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStreamingUpgradeCalls.h"
namespace
{
    mta::game_de::ClockResult InvokeUpgrade(mta::game_de::NativeStreamingUpgradeCalls::NativeFunction, std::uint32_t, std::uint32_t, std::uint32_t&) noexcept;
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeStreamingUpgradeReader.cpp"
#define MTA_DE_STREAMING_UPGRADE_NATIVE_INVOKE ::InvokeUpgrade
#include "../../Client/game_de/NativeStreamingUpgradeCalls.cpp"
#undef MTA_DE_STREAMING_UPGRADE_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace U = NativeStreamingUpgradeProfile;
    namespace UC = NativeStreamingUpgradeCalls;
    using UR = NativeStreamingUpgradeReader;
    struct UpgradeFixture : RequestFixture
    {
        explicit UpgradeFixture(const std::filesystem::path& directory) : RequestFixture(directory) { Reset(); }
        void Pair(std::size_t index, std::uint16_t first, std::uint16_t second)
        {
            Global(U::FirstLinksRva + index * sizeof(first), first);
            Global(U::SecondLinksRva + index * sizeof(second), second);
        }
        void Count(std::int32_t count) { Global(U::LinkCountRva, count); }
        void Reset()
        {
            RequestFixture::Reset();
            for (const auto& pin : U::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (std::size_t row = 0; row < U::LinkBudget; ++row)
                Pair(row, 0xffff, 0xffff);
            Count(1);
            Pair(0, 1000, 1001);
            for (auto model : {std::uint32_t{17}, std::uint32_t{1000}, std::uint32_t{1001}, std::uint32_t{1002}, std::uint32_t{1193}})
            {
                Slot(model, Address(metadata));
                Stream(model, 0x40, 0);
            }
        }
    };
    UpgradeFixture*    activeUpgrade = nullptr;
    unsigned           upgradeCalls = 0, upgradeMode = 0;
    std::uint32_t      upgradeModel = 0, upgradeFlags = 0;
    UC::NativeFunction upgradeFunction = nullptr;
    ClockResult        InvokeUpgrade(UC::NativeFunction function, std::uint32_t model, std::uint32_t flags, std::uint32_t& exception) noexcept
    {
        ++upgradeCalls;
        upgradeFunction = function;
        upgradeModel = model;
        upgradeFlags = flags;
        auto& fixture = *activeUpgrade;
        if (upgradeMode == 1)
        {
            exception = 0xe0580001;
            return ClockResult::OriginalException;
        }
        UR::Links links;
        // The native wrapper, rather than this production adapter, owns both
        // ordered requests. The last-boundary stub models those real effects
        // once; no supported EXE instruction is executed by the fixture.
        if (UR({fixture.transform.image.image}, &PoolTestHost::Owner).ReadLinks(model, links) != ClockResult::Ok)
            return ClockResult::BindingChanged;
        const auto apply = [&](std::uint32_t key)
        {
            auto old = fixture.Stream(key);
            fixture.Stream(key, static_cast<unsigned char>(old[0] | flags), 2);
        };
        apply(model);
        if (links.companionIndex >= 0)
            apply(static_cast<std::uint32_t>(links.companionIndex));
        if (upgradeMode == 2)
        {
            exception = 0xe0580002;
            return ClockResult::OriginalException;
        }
        if (upgradeMode == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (upgradeMode == 4)
            fixture.Global(SP::RootRva, std::uintptr_t{0});
        if (upgradeMode == 5)
            fixture.Slot(model, fixture.Address(fixture.metadata) + 0x80);
        if (upgradeMode == 6)
            fixture.Pair(0, 1000, 1002);
        if (upgradeMode == 7)
            fixture.Put(fixture.items, 48 + 0x10, std::uint32_t{99});
        if (upgradeMode == 8)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        return ClockResult::Ok;
    }
    UR::Value UpgradeSentinel()
    {
        UR::Value value;
        value.selected.modelIndex = 0xdeadbeefu;
        value.companion.modelIndex = 0xaabbccddu;
        value.links.count = -99;
        value.linksObserved = true;
        value.streamedIn = true;
        return value;
    }
    bool UpgradeUntouched(const UR::Value& value)
    {
        return value.selected.modelIndex == 0xdeadbeefu && value.companion.modelIndex == 0xaabbccddu && value.links.count == -99 && value.linksObserved &&
               value.streamedIn;
    }
    unsigned upgradeChange = 0;
    void     ChangeUpgrade()
    {
        if (upgradeChange == 0)
            activeUpgrade->Count(2);
        if (upgradeChange == 1)
            activeUpgrade->Pair(0, 1000, 1002);
        if (upgradeChange == 2)
            activeUpgrade->Stream(1000, 0x40, 0);
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "paired upgrade evidence uses an explicit owned workspace directory");
    UpgradeFixture fixture(argv[1]);
    activeUpgrade = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    const auto reset = [&]()
    {
        fixture.Reset();
        upgradeCalls = upgradeMode = 0;
        upgradeFunction = nullptr;
        upgradeModel = upgradeFlags = 0;
    };
    UR          reader({image}, &PoolTestHost::Owner), unowned({image}, nullptr);
    UC::Receipt receipt;
    Check(UR::Revalidate({image}), "exact native separate request,paired pure loaded leaf and both script consumers qualify");
    auto value = UpgradeSentinel();
    Check(unowned.Read(1000, value) == ClockResult::NotInitialized && UpgradeUntouched(value),
          "paired raw bytes cannot supply their own actual callback owner");
    for (auto bad : {std::uint32_t{0}, std::uint32_t{999}, std::uint32_t{1194}, std::uint32_t{20000}, std::uint32_t{65535}, 0xffffffffu})
    {
        reset();
        value = UpgradeSentinel();
        Check(reader.Read(bad, value) == ClockResult::InvalidArgument && UpgradeUntouched(value) && PoolTestHost::reads == 0 && PoolTestHost::ownerCalls == 0,
              "query refuses keys outside the actual shared inclusive MTA upgrade range before reads");
        Check(UC::Request(image, bad, 0x16, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered && upgradeCalls == 0 &&
                  PoolTestHost::reads == 0,
              "request refuses full unsigned out-of-upgrade keys without truncation or native entry");
    }
    for (unsigned state = 0; state < 256; ++state)
    {
        reset();
        fixture.Stream(1000, 0xa5, static_cast<unsigned char>(state));
        fixture.Stream(1001, 0x96, 1);
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) == ClockResult::Ok && value.selected.rawLoadState == state && value.selected.rawFlags == 0xa5 &&
                  value.streamedIn == (state == 1) && value.linksObserved == (state == 1),
              "every raw selected state uses the real state1 predicate; an unloaded selected model skips dependent links");
    }
    reset();
    fixture.Count(-1);
    fixture.Stream(1000, 0, 0);
    value = UpgradeSentinel();
    Check(reader.Read(1000, value) == ClockResult::Ok && !value.streamedIn && !value.linksObserved,
          "native unloaded short circuit does not claim or inspect a currently invalid link table");
    for (auto companionState : {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(2), static_cast<unsigned char>(255)})
    {
        reset();
        fixture.Stream(1000, 0x40, 1);
        fixture.Stream(1001, 0x80, companionState);
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) == ClockResult::Ok && value.linksObserved && value.links.companionIndex == 1001 && value.companion.modelIndex == 1001 &&
                  value.streamedIn == (companionState == 1),
              "paired query requires the current companion's real state1");
    }
    for (auto count : {std::int32_t{0}, std::int32_t{1}, std::int32_t{30}})
    {
        reset();
        fixture.Count(count);
        if (count)
            fixture.Pair(static_cast<std::size_t>(count - 1), 1000, 0xffff);
        fixture.Stream(1000, 0x40, 1);
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) == ClockResult::Ok && value.streamedIn && value.links.companionIndex == -1,
              "zero table and matched signedffff sentinel admit no companion within the conservative disjoint table budget");
    }
    reset();
    fixture.Count(2);
    fixture.Pair(0, 1000, 1001);
    fixture.Pair(1, 1002, 1000);
    fixture.Stream(1000, 0, 1);
    fixture.Stream(1002, 0, 1);
    value = UpgradeSentinel();
    Check(reader.Read(1000, value) == ClockResult::Ok && value.streamedIn && value.links.matchingRow == 1 && value.links.companionIndex == 1002,
          "reverse native row priority and second-array matches select the actual latest companion");
    reset();
    fixture.Pair(0, 1000, 1000);
    fixture.Stream(1000, 0, 1);
    value = UpgradeSentinel();
    Check(reader.Read(1000, value) == ClockResult::Ok && value.streamedIn && value.links.companionIndex == 1000,
          "a row matching both arrays follows the native first-array comparison and supports the actual self companion");
    for (auto badCount : {std::int32_t{-1}, std::int32_t{31}, std::int32_t{0x7fffffff}})
    {
        reset();
        fixture.Count(badCount);
        fixture.Stream(1000, 0, 1);
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) != ClockResult::Ok && UpgradeUntouched(value), "negative or over-budget native count never claims unrelated globals");
        Check(UC::Request(image, 1000, 0x16, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && upgradeCalls == 0,
              "unsafe native reverse loop count refuses before either real upgrade effect");
    }
    for (auto badCompanion : {std::uint16_t{0xfffe}, std::uint16_t{20000}, std::uint16_t{32767}})
    {
        reset();
        fixture.Pair(0, 1000, badCompanion);
        fixture.Stream(1000, 0, 1);
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) != ClockResult::Ok && UpgradeUntouched(value), "matched companion signed key must be absent or a real DFF bank key");
        Check(UC::Request(image, 1000, 0x16, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered,
              "an unqualified companion cannot be admitted by a successful selected-model preflight");
    }
    for (auto model : {std::uint32_t{1000}, std::uint32_t{1193}})
        for (auto flags : {std::uint32_t{0}, std::uint32_t{6}, std::uint32_t{0x16}, std::uint32_t{0x100}, std::uint32_t{0xffff0016}})
            for (auto state : {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(2), static_cast<unsigned char>(3),
                               static_cast<unsigned char>(4)})
            {
                reset();
                fixture.Pair(0, static_cast<std::uint16_t>(model), 1001);
                fixture.Stream(model, 0x40, state);
                fixture.Stream(1001, 0x80, state);
                Check(UC::Request(image, model, flags, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && upgradeCalls == 1 && receipt.entered &&
                          receipt.returned && receipt.entryRva == U::RequestRva && receipt.modelIndex == model && receipt.flags == flags &&
                          receipt.companionIndex == 1001 && receipt.root == fixture.Address(fixture.application) &&
                          receipt.provider == fixture.Address(fixture.provider) && receipt.selectedStateBefore == state &&
                          receipt.companionStateBefore == state && receipt.selectedStateAfter == 2 && receipt.companionStateAfter == 2 &&
                          upgradeFunction == reinterpret_cast<UC::NativeFunction>(image + U::RequestRva) && upgradeModel == model && upgradeFlags == flags &&
                          fixture.Stream(model)[0] == static_cast<unsigned char>(0x40 | flags) &&
                          fixture.Stream(1001)[0] == static_cast<unsigned char>(0x80 | flags),
                      "both inclusive MTA boundaries and full32bit flags call the genuine paired wrapper exactly once; async2 is not loaded1");
            }
    reset();
    fixture.Pair(0, 1000, 17);
    Check(UC::Request(image, 1000, 6, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.companionIndex == 17 && upgradeCalls == 1,
          "the native paired companion need not itself be in MTA's upgrade dispatch range");
    for (unsigned mode = 1; mode <= 8; ++mode)
    {
        reset();
        upgradeMode = mode;
        const auto result = UC::Request(image, 1000, 0x16, &PoolTestHost::Owner, &receipt);
        Check(result != ClockResult::Ok && receipt.entered && receipt.returned == (mode > 2) && upgradeCalls == 1,
              "entered exception or lost post-root,bank,links,registry,state or callback owner is reported once for enclosing Stop");
        if (mode >= 2)
            Check(fixture.Stream(1000)[1] == 2 && fixture.Stream(1001)[1] == 2 && fixture.Stream(1000)[0] == 0x56 && fixture.Stream(1001)[0] == 0x56,
                  "an entered failure never rewinds either selected/companion flags or replays a native request");
        else
            Check(fixture.Stream(1000)[1] == 0 && receipt.exceptionCode == 0xe0580001,
                  "pre-effect exception still records native entry and exception identity");
    }
    reset();
    // Raw UObject membership may legitimately precede weak-serial creation.
    // Test a rejected item flag below rather than inventing a serial rule.
    fixture.Put(fixture.items, 24 + 0x10, std::uint32_t{0});
    fixture.Put(fixture.items, 48 + 0x10, std::uint32_t{0});
    Check(UC::Request(image, 1000, 0x16, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && upgradeCalls == 1,
          "raw root/provider membership with unallocated weak serials remains valid for the actual paired request");
    for (unsigned mode = 0; mode < 7; ++mode)
    {
        reset();
        if (mode == 0)
            fixture.Slot(1001, 0);
        if (mode == 1)
            fixture.Put(fixture.provider, SP::ProviderCountOffset, std::int32_t{1001});
        if (mode == 2)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 3)
            fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
        if (mode == 4)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (mode == 5)
            fixture.Put(fixture.items, 48 + NativeStartupProvidersProfile::itemFlagsOffset, NativeStartupProvidersProfile::itemRejectMask);
        if (mode == 6)
            PoolTestHost::ownerResult = ClockResult::Expired;
        Check(UC::Request(image, 1000, 0x16, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && upgradeCalls == 0,
              "both actual native models,provider bounds,registry,native GameThread,no-GC,playing state and callback owner precede the first effect");
    }
    reset();
    value = UpgradeSentinel();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&]() { foreign = reader.Read(1000, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && UpgradeUntouched(value), "producer thread cannot manufacture the actual initialized native GameThread");
    for (upgradeChange = 0; upgradeChange < 3; ++upgradeChange)
    {
        reset();
        fixture.Stream(1000, 0x40, 1);
        fixture.Stream(1001, 0x40, 1);
        // Pair changes alter the second array. Trigger after those old bytes
        // were captured; changing an unread field can form a valid snapshot.
        PoolTestHost::watched = image + (upgradeChange == 0   ? U::LinkCountRva
                                         : upgradeChange == 1 ? U::SecondLinksRva
                                                              : LP::ArrayRva + 1000 * LP::RowStride + LP::FlagsOffset);
        PoolTestHost::after = &ChangeUpgrade;
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) != ClockResult::Ok && UpgradeUntouched(value) && !PoolTestHost::after,
              "between-copy count,pair or selected-state changes are refused without publishing a dependent loaded fact");
    }
    reset();
    fixture.Stream(1000, 0, 1);
    fixture.Stream(1001, 0, 1);
    value = UpgradeSentinel();
    Check(reader.Read(1000, value) == ClockResult::Ok, "measure actual new paired-query copied-byte and owner boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned part = 1; part <= 16; ++part)
    {
        reset();
        fixture.Stream(1000, 0, 1);
        fixture.Stream(1001, 0, 1);
        PoolTestHost::failAt = 1 + (reads - 1) * (part - 1) / 15;
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) != ClockResult::Ok && UpgradeUntouched(value), "distributed actual checked-copy failures preserve all caller output");
    }
    for (unsigned revoke = 1; revoke <= owners; ++revoke)
    {
        reset();
        fixture.Stream(1000, 0, 1);
        fixture.Stream(1001, 0, 1);
        PoolTestHost::revokeAt = revoke;
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) != ClockResult::Ok && UpgradeUntouched(value), "each actual paired-query callback-owner bookend may revoke publication");
    }
    for (const auto& pin : U::Code)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        value = UpgradeSentinel();
        Check(reader.Read(1000, value) != ClockResult::Ok && UpgradeUntouched(value), "each independent genuine upgrade code pin is mandatory before output");
        Check(UC::Request(image, 1000, 6, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && upgradeCalls == 0,
              "changed genuine upgrade code never reaches native entry");
    }
    std::puts("Production native streaming upgrade request/query fixture completed");
    std::printf("Passed %u native streaming-upgrade checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}

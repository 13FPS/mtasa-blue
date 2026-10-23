/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact byte intent on the sole registered-ped callback owner
 ****************************************************************************/
{
    namespace Area = NativePedAreaProfile;
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Area::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& group : Area::Groups)
    {
        const auto entry = fixture.image + group.entryRva;
        const auto branch = static_cast<std::uint32_t>(group.branchRva);
        admit(group.slotRva, &entry, sizeof(entry), PAGE_READONLY);
        admit(group.tableRva + group.index * 4, &branch, sizeof(branch), PAGE_EXECUTE_READ);
    }
    struct AreaCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<AreaCallback*>(opaque)->action(access); }
    } areaCall;
    struct DefaultAreaOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } absentArea;
    std::uint8_t areaOutput = 0xe7;
    Check(absentArea.ReadPedAreaCode({}, areaOutput) == ClockResult::NotInitialized && areaOutput == 0xe7 &&
              absentArea.SetPedAreaCode({}, 255) == ClockResult::NotInitialized,
          "older owners refuse area intent without substituting a current player or fabricated zero");
    ped_area_fixture::Game areaGame;
    const auto             resetArea = [&]
    {
        ClockPedAreaOwnerTest::stores = 0;
        ClockPedAreaOwnerTest::cell = 0;
        ClockPedAreaOwnerTest::loseToken = ClockPedAreaOwnerTest::failAfter = false;
        pool.objects[Area::AreaOffset - 1] = 0xa5;
        pool.objects[Area::AreaOffset] = 0x7f;
        pool.objects[Area::AreaOffset + 1] = 0xc3;
    };
    resetArea();
    {
        NativeClockService      areaService;
        NativePedReferenceToken areaToken;
        ClockResult             areaResult = ClockResult::NotInitialized;
        areaCall.action = [&](NativeGameTimingAccess& access) { areaResult = access.AcquirePedReference(1, areaToken); };
        BeginOwned(areaService, fixture, false, true, true, &AreaCallback::Invoke, &areaCall);
        fixture.invokeTimer();
        Check(areaResult == ClockResult::Ok && areaToken.version && runtime.pedReferences.IsRetained(), "area uses the sole actual registered-ped controller");
        Check(runtime.access.ReadPedAreaCode(areaToken, areaOutput) != ClockResult::Ok && areaOutput == 0xe7 &&
                  runtime.access.SetPedAreaCode(areaToken, 0) != ClockResult::Ok && !ClockPedAreaOwnerTest::stores,
              "retained token cannot read or write area between callbacks");
        std::thread areaForeign([&] { areaResult = runtime.access.SetPedAreaCode(areaToken, 1); });
        areaForeign.join();
        Check(areaResult != ClockResult::Ok && !ClockPedAreaOwnerTest::stores && !runtime.stopping.load(),
              "foreign area request cannot stop the valid runtime");
        const auto invokeArea = [&]
        {
            Check(areaService.Renew(runtime.generation.load()), "renew actual area callback");
            fixture.invokeTimer();
        };
        areaCall.action = [&](NativeGameTimingAccess& access)
        {
            auto stale = areaToken;
            ++stale.version;
            Check(access.SetPedAreaCode(stale, 3) != ClockResult::Ok && !runtime.stopping.load(), "stale initial token refuses without byte entry or Stop");
        };
        invokeArea();
        for (const auto target : {std::uint8_t{0}, std::uint8_t{7}, std::uint8_t{127}, std::uint8_t{128}, std::uint8_t{255}, std::uint8_t{255}})
        {
            const auto before = ClockPedAreaOwnerTest::stores;
            areaCall.action = [&](NativeGameTimingAccess& access)
            {
                ped_area_fixture::PedAreaFixture ped(areaGame, access, access, access, access, access, access, access, access, access, access, access, access,
                                                     access, access, access, access, access, access, areaToken, 0x12345678);
                CPed&                            sdk = ped;
                try
                {
                    sdk.SetAreaCode(target);
                    areaOutput = sdk.GetAreaCode();
                    areaResult = ClockResult::Ok;
                }
                catch (const std::exception& error)
                {
                    Check(false, error.what());
                }
            };
            invokeArea();
            Check(areaResult == ClockResult::Ok && areaOutput == target && ClockPedAreaOwnerTest::stores == before + 1 &&
                      pool.objects[Area::AreaOffset] == target && pool.objects[Area::AreaOffset - 1] == 0xa5 && pool.objects[Area::AreaOffset + 1] == 0xc3,
                  "actual area SDK forwards raw byte including same-value CAS and preserves both neighbors without interior effects");
        }
        retire(areaService);
        Check(pool.objects[Area::AreaOffset] == 255, "End retires the borrowed reference and keeps accepted area byte");
    }
    for (const bool loseAreaToken : {false, true})
    {
        resetArea();
        NativeClockService      areaService;
        NativePedReferenceToken areaToken;
        ClockResult             areaResult = ClockResult::NotInitialized;
        areaCall.action = [&](NativeGameTimingAccess& access) { areaResult = access.AcquirePedReference(1, areaToken); };
        BeginOwned(areaService, fixture, false, true, true, &AreaCallback::Invoke, &areaCall);
        fixture.invokeTimer();
        Check(areaResult == ClockResult::Ok, "fresh area owner acquires one genuine registered relation");
        const auto node = ClockPedReferenceOwnerTest::node;
        const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
        ClockPedAreaOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedAreaOwnerTest::loseToken = loseAreaToken;
        ClockPedAreaOwnerTest::failAfter = !loseAreaToken;
        areaCall.action = [&](NativeGameTimingAccess& access) { areaResult = access.SetPedAreaCode(areaToken, 0xee); };
        Check(areaService.Renew(runtime.generation.load()), "renew before persistent area acknowledgement loss");
        fixture.invokeTimer();
        Check(areaResult != ClockResult::Ok && runtime.stopping.load() && ClockPedAreaOwnerTest::stores == 1 && pool.objects[Area::AreaOffset] == 0xee,
              "accepted token loss or uncertain CAS acknowledgement Stops after one byte attempt and preserves its effect");
        if (loseAreaToken)
        {
            Check(!areaService.End() && runtime.pedReferences.IsRetained(), "zero weak word cannot free a cell still named by the native bank");
            fixture.invokeTimer();
            Check(ClockPedAreaOwnerTest::stores == 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && runtime.pedReferences.IsRetained(),
                  "stopping callbacks retain native-visible cell debt without retrying area CAS or unsafe unlink");
            // Supply only the established native address-retirement postcondition.
            // This is test-owned bank progress, not new production cleanup logic.
            ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
            const auto free = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(node, free);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
            ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
            fixture.invokeTimer();
        }
        retire(areaService);
        Check(ClockPedAreaOwnerTest::stores == 1 && pool.objects[Area::AreaOffset] == 0xee,
              "area retirement never repeats or restores persistent native intent");
    }
    ClockPedAreaOwnerTest::cell = 0;
    ClockPedAreaOwnerTest::loseToken = ClockPedAreaOwnerTest::failAfter = false;
    std::puts("Production native registered area callback owner fixture completed");
    std::puts("Production actual ped area SDK callback fixture completed");
}

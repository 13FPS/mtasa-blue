/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual registered-ped XYZ intent and SDK teleport completion
 ****************************************************************************/
{
    namespace T = NativePedTransformProfile;
    namespace P = NativePedTeleportProfile;
    namespace M = NativePedMetadataProfile;
    for (const auto& pin : T::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : M::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : P::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : P::Names)
        admit(pin.rva, pin.bytes, pin.size, PAGE_READONLY);
    for (const auto& pin : NativeUObjectMembershipProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeStartupWorldProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    for (const auto& slot : P::Slots)
    {
        const auto method = fixture.image + slot.targetRva;
        admit(slot.rva, &method, sizeof(method), PAGE_READONLY);
    }
    // Retain the common reference fixture's exact destructor slots. Transform
    // accepts those executable methods; replacing them would invalidate the
    // same native reference controller that owns this pose callback.
    admit(T::ScriptVelocityInScaleRva, &T::ScriptVelocityInScaleBits, 4, PAGE_READONLY);
    admit(T::ScriptVelocityOutScaleRva, &T::ScriptVelocityOutScaleBits, 4, PAGE_READONLY);
    const auto getter = fixture.image + NativeStartupWorldProfile::getterRva;
    admit(NativeStartupWorldProfile::ownerGetWorldSlotRva, &getter, sizeof(getter), PAGE_READONLY);
    for (const auto rva :
         {P::IntelligenceRootRva, P::PointerRootRva, P::EntryRootRva, P::ActorClassCacheRva, P::ComponentClassCacheRva,
          NativeStartupWorldProfile::objectChunksRva, NativeUiClassMapProfile::uiTransitionClassCacheRva, NativeStartupOwnerProfile::gameModeClassCacheRva,
          NativeStartupWorldProfile::levelClassCacheRva, NativeStartupProvidersProfile::playerControllerClassCacheRva,
          NativeStartupProvidersProfile::viewportClassCacheRva, NativeStartupProvidersProfile::managerClassCacheRva, NativeStartupProvidersProfile::engineRva})
        admit(rva, &zero, sizeof(zero), PAGE_READWRITE);
    const std::uint32_t count = 0;
    admit(NativeStartupWorldProfile::objectCountRva, &count, sizeof(count), PAGE_READWRITE);
    ClockTeleportPool intelligence(fixture, P::IntelligenceRootRva, P::IntelligenceCapacity, P::IntelligenceStride);
    ClockTeleportPool pointers(fixture, P::PointerRootRva, P::PointerCapacity, P::PointerStride);
    ClockTeleportPool entries(fixture, P::EntryRootRva, P::EntryCapacity, P::EntryStride);
    intelligence.Publish();
    pointers.Publish();
    entries.Publish();
    intelligence.flags[0] = 1;
    const auto pedAddress = reinterpret_cast<std::uintptr_t>(pool.objects);
    const auto intelligenceAddress = reinterpret_cast<std::uintptr_t>(intelligence.objects);
    std::memcpy(intelligence.objects, &pedAddress, sizeof(pedAddress));
    std::memcpy(intelligence.objects + P::IntelligenceOwnerOffset, &pedAddress, sizeof(pedAddress));
    alignas(16) std::array<unsigned char, 0x60> matrix{};
    const auto                                  matrixAddress = reinterpret_cast<std::uintptr_t>(matrix.data());
    std::memcpy(matrix.data() + T::MatrixOwnerOffset, &pedAddress, sizeof(pedAddress));
    const auto reset = [&](bool matrixBacked)
    {
        const auto selectedMatrix = matrixBacked ? matrixAddress : std::uintptr_t{0};
        std::memcpy(pool.objects + T::MatrixPointerOffset, &selectedMatrix, sizeof(selectedMatrix));
        std::memcpy(pool.objects + P::IntelligenceOffset, &intelligenceAddress, sizeof(intelligenceAddress));
        const std::uint16_t model = 17;
        std::memcpy(pool.objects + M::ModelOffset, &model, sizeof(model));
        store(P::PhysicalFlagsOffset, 0);
        store(P::SelectorOffset, 0);
        for (const auto offset : {P::ReferenceOffset, P::WorldEntryOffset, P::MovingNodeOffset, P::ActorOffset})
            std::memcpy(pool.objects + offset, &zero, sizeof(zero));
        const CVector initial(1.0f, 2.0f, 3.0f);
        std::memcpy(pool.objects + T::InlinePositionOffset, &initial, sizeof(initial));
        std::memcpy(matrix.data() + T::MatrixPositionOffset, &initial, sizeof(initial));
        ClockPedPhysicalScalarsOwnerTest::stores = ClockPedPhysicalScalarsOwnerTest::failAt = ClockPedPhysicalScalarsOwnerTest::loseAt = 0;
        ClockPedPhysicalScalarsOwnerTest::cell = 0;
        ClockPedPhysicalScalarsOwnerTest::loseToken = false;
        ClockPedTeleportOwnerTest::calls = 0;
        ClockPedTeleportOwnerTest::result = ClockResult::Ok;
        ClockPedTeleportOwnerTest::effect = true;
    };
    const auto position = [&](bool matrixBacked)
    {
        CVector current;
        std::memcpy(&current, matrixBacked ? matrix.data() + T::MatrixPositionOffset : pool.objects + T::InlinePositionOffset, sizeof(current));
        return current;
    };
    const auto same = [](const CVector& a, const CVector& b) { return std::memcmp(&a, &b, sizeof(a)) == 0; };
    struct PoseCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<PoseCallback*>(opaque)->action(access); }
    } call;
    struct DefaultPoseOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } absent;
    const CVector target(71.25f, -82.5f, 93.75f);
    Check(absent.SetReferencedPedPosition({}, target) == ClockResult::NotInitialized &&
              absent.CompleteReferencedPedTeleport({}, target) == ClockResult::NotInitialized &&
              runtime.access.SetReferencedPedPosition({}, target) == ClockResult::NotInitialized,
          "timing-only and unbound owners neither write XYZ nor invent a completion capability");
    ClockPedTeleportOwnerTest::image = fixture.image;
    {
        reset(false);
        NativeClockService      service;
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        call.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &PoseCallback::Invoke, &call);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok, "pose uses the sole real registered reference controller");
        Check(runtime.access.SetReferencedPedPosition(token, target) != ClockResult::Ok && !ClockPedPhysicalScalarsOwnerTest::stores,
              "a producer cannot turn a retained token into between-callback placement permission");
        std::thread foreign([&] { result = runtime.access.CompleteReferencedPedTeleport(token, target); });
        foreign.join();
        Check(result != ClockResult::Ok && !runtime.stopping.load(), "foreign completion cannot stop the unrelated admitted runtime");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the actual pose callback owner");
            fixture.invokeTimer();
        };
        call.action = [&](NativeGameTimingAccess& access)
        {
            auto stale = token;
            ++stale.version;
            Check(access.SetReferencedPedPosition(stale, target) != ClockResult::Ok && access.CompleteReferencedPedTeleport(stale, target) != ClockResult::Ok &&
                      !runtime.stopping.load(),
                  "stale tokens refuse before entry without stopping a different admitted intent");
            NativePedPoseMutation active;
            Check(static_cast<bool>(active) && access.SetReferencedPedPosition(token, target) == ClockResult::Busy &&
                      access.TeleportPed(1, target) == ClockResult::Busy && !runtime.stopping.load(),
                  "raw placement and teleport share one adapter mutation boundary and release it by RAII");
        };
        invoke();
        call.action = [&](NativeGameTimingAccess& access)
        {
            ped_pose_fixture::PedPoseFixture ped(access, access, access, access, access, access, access, access, access, access, access, access, access, access,
                                                 token, 0x12345678);
            CEntity&                         entity = ped;
            entity.SetPosition(target.fX, target.fY, target.fZ);
            Check(ped.GetType() == 0x12345678 && !ped.repairCalls, "actual pose dispatch retains CPU wrapper bookkeeping and needs no fake repair");
        };
        invoke();
        Check(same(position(false), target) && ClockPedPhysicalScalarsOwnerTest::stores == 3 && !runtime.stopping.load(),
              "actual CEntity scalar SetPosition reaches three qualified XYZ commits under the issued token");
        const auto accepted = ClockPedPhysicalScalarsOwnerTest::stores;
        call.action = [&](NativeGameTimingAccess& access)
        {
            const std::uint16_t train = 537;
            std::memcpy(pool.objects + M::ModelOffset, &train, sizeof(train));
            result = access.SetReferencedPedPosition(token, CVector(4, 5, 6));
            const std::uint16_t ordinary = 17;
            std::memcpy(pool.objects + M::ModelOffset, &ordinary, sizeof(ordinary));
        };
        invoke();
        Check(result == ClockResult::UnsupportedBuild && ClockPedPhysicalScalarsOwnerTest::stores == accepted && !runtime.stopping.load() &&
                  same(position(false), target),
              "unsupported rail side effects refuse before writes without retiring a usable owner");
        reset(true);
        const CVector destination(-14.25f, 25.5f, 36.75f);
        call.action = [&](NativeGameTimingAccess& access)
        {
            WithPedTeleportActor(fixture, pool,
                                 [&]
                                 {
                                     ped_pose_fixture::PedPoseFixture ped(access, access, access, access, access, access, access, access, access, access,
                                                                          access, access, access, access, token, 2);
                                     CEntity&                         entity = ped;
                                     entity.Teleport(destination.fX, destination.fY, destination.fZ);
                                 });
        };
        invoke();
        Check(same(position(true), destination) && ClockPedPhysicalScalarsOwnerTest::stores == 3 && ClockPedTeleportOwnerTest::calls == 1 &&
                  ClockPedTeleportOwnerTest::receiver == pedAddress && !runtime.stopping.load(),
              "matrix-backed actual SDK Teleport commits placement before one genuine native receiver call on the same owner");
        retire(service);
        Check(same(position(true), destination), "normal retirement preserves acknowledged placement and teleport effects");
    }
    std::puts("Production actual ped pose SDK callback fixture completed");
    {
        reset(true);
        NativeClockService      service;
        NativePedReferenceToken token;
        call.action = [&](NativeGameTimingAccess& access) { Check(access.AcquirePedReference(1, token) == ClockResult::Ok, "completion token admission"); };
        BeginOwned(service, fixture, false, true, true, &PoseCallback::Invoke, &call);
        fixture.invokeTimer();
        // SetPosition does not require an intelligence object. A later raw
        // teleport refusal must still stop this already-committed SDK intent.
        std::memcpy(pool.objects + P::IntelligenceOffset, &zero, sizeof(zero));
        bool refused = false;
        call.action = [&](NativeGameTimingAccess& access)
        {
            ped_pose_fixture::PedPoseFixture ped(access, access, access, access, access, access, access, access, access, access, access, access, access, access,
                                                 token, 2);
            try
            {
                ped.Teleport(target.fX, target.fY, target.fZ);
            }
            catch (const std::runtime_error&)
            {
                refused = true;
            }
        };
        Check(service.Renew(runtime.generation.load()), "renew before a post-placement pre-native binding refusal");
        fixture.invokeTimer();
        Check(refused && runtime.stopping.load() && same(position(true), target) && ClockPedPhysicalScalarsOwnerTest::stores == 3 &&
                  ClockPedTeleportOwnerTest::calls == 0,
              "post-placement binding refusal stops the same completion intent even though the native teleport call was never entered");
        fixture.invokeTimer();
        Check(ClockPedPhysicalScalarsOwnerTest::stores == 3 && !ClockPedTeleportOwnerTest::calls,
              "stopping completion neither retries the placement prefix nor invokes a missing native receiver");
        retire(service);
    }
    {
        reset(true);
        NativeClockService      service;
        NativePedReferenceToken token;
        call.action = [&](NativeGameTimingAccess& access) { Check(access.AcquirePedReference(1, token) == ClockResult::Ok, "native-failure token admission"); };
        BeginOwned(service, fixture, false, true, true, &PoseCallback::Invoke, &call);
        fixture.invokeTimer();
        ClockPedTeleportOwnerTest::result = ClockResult::OriginalException;
        bool refused = false;
        call.action = [&](NativeGameTimingAccess& access)
        {
            WithPedTeleportActor(fixture, pool,
                                 [&]
                                 {
                                     ped_pose_fixture::PedPoseFixture ped(access, access, access, access, access, access, access, access, access, access,
                                                                          access, access, access, access, token, 2);
                                     try
                                     {
                                         ped.Teleport(target.fX, target.fY, target.fZ);
                                     }
                                     catch (const std::runtime_error&)
                                     {
                                         refused = true;
                                     }
                                 });
        };
        Check(service.Renew(runtime.generation.load()), "renew before one entered native teleport exception");
        fixture.invokeTimer();
        Check(refused && runtime.stopping.load() && same(position(true), target) && ClockPedTeleportOwnerTest::calls == 1,
              "an entered native failure preserves the accepted position and stops without destructive replay");
        fixture.invokeTimer();
        Check(ClockPedTeleportOwnerTest::calls == 1 && ClockPedPhysicalScalarsOwnerTest::stores == 3, "retirement never repeats the entered teleport");
        retire(service);
    }
    std::puts("Production native ped teleport completion intent fixture completed");
    {
        reset(false);
        NativeClockService      service;
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        call.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &PoseCallback::Invoke, &call);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok, "partial-write token admission");
        ClockPedPhysicalScalarsOwnerTest::failAt = 2;
        call.action = [&](NativeGameTimingAccess& access) { result = access.SetReferencedPedPosition(token, target); };
        Check(service.Renew(runtime.generation.load()), "renew before an entered second-component store failure");
        fixture.invokeTimer();
        const CVector partial(target.fX, 2, 3);
        Check(result != ClockResult::Ok && runtime.stopping.load() && same(position(false), partial) && ClockPedPhysicalScalarsOwnerTest::stores == 2,
              "a failed Y store preserves accepted X, untouched Y/Z and stops the actual owner");
        fixture.invokeTimer();
        Check(ClockPedPhysicalScalarsOwnerTest::stores == 2 && same(position(false), partial), "no failed component or untouched suffix is replayed");
        retire(service);
    }
    {
        reset(false);
        NativeClockService      service;
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        call.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &PoseCallback::Invoke, &call);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok, "post-position reference-loss token admission");
        const auto node = ClockPedReferenceOwnerTest::node;
        const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
        ClockPedPhysicalScalarsOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedPhysicalScalarsOwnerTest::loseAt = 3;
        ClockPedPhysicalScalarsOwnerTest::loseToken = true;
        call.action = [&](NativeGameTimingAccess& access) { result = access.SetReferencedPedPosition(token, target); };
        Check(service.Renew(runtime.generation.load()), "renew before token loss after the final acknowledged component");
        fixture.invokeTimer();
        Check(result != ClockResult::Ok && runtime.stopping.load() && same(position(false), target) && ClockPedPhysicalScalarsOwnerTest::stores == 3 &&
                  runtime.pedReferences.IsRetained() && !service.End(),
              "post-write token loss retains original native-visible cell debt and preserves all accepted position bits");
        fixture.invokeTimer();
        Check(ClockPedPhysicalScalarsOwnerTest::stores == 3 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
              "stopping callbacks neither replay XYZ nor retry uncertain native reference cleanup");
        // Only the existing full-bank node-address absence postcondition may
        // discharge this debt; the accepted position is not restoration data.
        ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
        const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(node, head);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
        ClockPedReferenceOwnerTest::Put(pedAddress + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(service);
        Check(same(position(false), target) && ClockPedPhysicalScalarsOwnerTest::stores == 3 && ClockPedReferenceOwnerTest::unregisters == unlinks,
              "proved reference retirement preserves persistent placement with no mutation or native unlink replay");
    }
    ClockPedPhysicalScalarsOwnerTest::failAt = ClockPedPhysicalScalarsOwnerTest::loseAt = 0;
    ClockPedTeleportOwnerTest::image = 0;
    std::puts("Production native ped placement callback owner fixture completed");
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete native ped orientation in the sole registered callback
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
    namespace O = NativePedOrientationProfile;
    for (const auto& pin : O::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    struct OrientationCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<OrientationCallback*>(opaque)->action(access); }
    } orientationCall;
    struct DefaultOrientationOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } orientationAbsent;
    ped_orientation_fixture::Game orientationGame;
    const CVector                 orientationTarget(0.25f, -0.75f, 2.5f);
    Check(orientationAbsent.SetPedOrientation({}, orientationTarget) == ClockResult::NotInitialized &&
              runtime.access.SetPedOrientation({}, orientationTarget) == ClockResult::NotInitialized,
          "default and unbound native owners grant no orientation phase or fake current-player substitute");
    const auto resetOrientation = [&](bool linked)
    {
        reset(linked);
        ClockPedOrientationOwnerTest::Reset();
    };
    {
        resetOrientation(true);
        NativeClockService      orientationService;
        NativePedReferenceToken orientationToken;
        ClockResult             orientationResult = ClockResult::NotInitialized;
        orientationCall.action = [&](NativeGameTimingAccess& access) { orientationResult = access.AcquirePedReference(1, orientationToken); };
        BeginOwned(orientationService, fixture, false, true, true, &OrientationCallback::Invoke, &orientationCall);
        fixture.invokeTimer();
        Check(orientationResult == ClockResult::Ok, "orientation borrows the existing sole native reference controller");
        Check(runtime.access.SetPedOrientation(orientationToken, orientationTarget) != ClockResult::Ok && !ClockPedOrientationOwnerTest::calls,
              "a retained token grants no between-callback native orientation permission");
        std::thread orientationForeign([&] { orientationResult = runtime.access.SetPedOrientation(orientationToken, orientationTarget); });
        orientationForeign.join();
        Check(orientationResult != ClockResult::Ok && !ClockPedOrientationOwnerTest::calls && !runtime.stopping.load(),
              "foreign orientation admission cannot enter effects or stop another runtime");
        const auto invokeOrientation = [&]
        {
            Check(orientationService.Renew(runtime.generation.load()), "renew the same actual orientation callback");
            fixture.invokeTimer();
        };
        orientationCall.action = [&](NativeGameTimingAccess& access)
        {
            auto staleOrientationToken = orientationToken;
            ++staleOrientationToken.version;
            Check(access.SetPedOrientation(staleOrientationToken, orientationTarget) != ClockResult::Ok && !runtime.stopping.load(),
                  "stale initial token does not stop the active orientation owner");
            NativePedPoseMutation occupiedPose;
            Check(occupiedPose && access.SetPedOrientation(orientationToken, orientationTarget) == ClockResult::Busy &&
                      access.TeleportPed(1, orientationTarget) == ClockResult::Busy &&
                      access.SetReferencedPedPosition(orientationToken, orientationTarget) == ClockResult::Busy && !ClockPedOrientationOwnerTest::calls,
                  "orientation, Teleport and placement retain one existing adapter pose gate without native effects");
        };
        invokeOrientation();
        for (const bool matrixBacked : {false, true})
        {
            resetOrientation(matrixBacked);
            orientationCall.action = [&](NativeGameTimingAccess& access)
            {
                WithPedTeleportActor(fixture, pool,
                                     [&]
                                     {
                                         ClockPedOrientationOwnerTest::afterRemove = [&]
                                         {
                                             Check(access.SetPedOrientation(orientationToken, orientationTarget) == ClockResult::Busy &&
                                                       access.TeleportPed(1, orientationTarget) == ClockResult::Busy &&
                                                       access.SetReferencedPedPosition(orientationToken, orientationTarget) == ClockResult::Busy,
                                                   "synchronous native callbacks cannot replay any pose adapter operation during an admitted phase");
                                         };
                                         ped_orientation_fixture::PedOrientationFixture orientationPed(
                                             orientationGame, access, access, access, access, access, access, access, access, access, access, access, access,
                                             access, access, access, access, access, orientationToken, 0x12345678);
                                         CEntity& orientationEntity = orientationPed;
                                         try
                                         {
                                             orientationEntity.SetOrientation(orientationTarget.fX, orientationTarget.fY, orientationTarget.fZ);
                                         }
                                         catch (const std::exception& error)
                                         {
                                             Check(false, error.what());
                                         }
                                         Check(orientationPed.GetType() == 0x12345678 && !orientationPed.GetCustomMoveAnim() && !orientationPed.repairCalls,
                                               "actual SDK orientation preserves wrapper bookkeeping and does not invent last-good physics repair");
                                     });
            };
            invokeOrientation();
            Check(ClockPedOrientationOwnerTest::calls == 3 && !runtime.stopping.load(),
                  "actual CEntity orientation acknowledges each native phase exactly once");
            for (std::size_t phase = 0; phase != 3; ++phase)
                Check(static_cast<std::size_t>(ClockPedOrientationOwnerTest::phases[phase]) == phase &&
                          ClockPedOrientationOwnerTest::entries[phase] == fixture.image + (phase == 0   ? O::RemoveRva
                                                                                           : phase == 1 ? O::RotateRva
                                                                                                        : O::AddRva) &&
                          ClockPedOrientationOwnerTest::receivers[phase] == pedAddress &&
                          !std::memcmp(&ClockPedOrientationOwnerTest::arguments[phase], &orientationTarget, sizeof(orientationTarget)),
                      "genuine ordered Remove/Euler/Add targets retain the same ped and raw SDK radians");
            if (matrixBacked)
                Check(!std::memcmp(matrix.data(), &orientationTarget, sizeof(orientationTarget)), "matrix-native accepted-prefix marker persists");
            else
                Check(raw(O::InlineHeadingOffset) == floatBits(orientationTarget.fZ), "genuine absent-matrix route preserves its native inline-heading branch");
        }
        ClockPedOrientationOwnerTest::Reset();
        std::memcpy(pool.objects + P::IntelligenceOffset, &zero, sizeof(zero));
        orientationCall.action = [&](NativeGameTimingAccess& access) { orientationResult = access.SetPedOrientation(orientationToken, orientationTarget); };
        invokeOrientation();
        Check(orientationResult != ClockResult::Ok && !ClockPedOrientationOwnerTest::calls && !runtime.stopping.load(),
              "missing typed native prerequisites refuse before world removal and keep unrelated admitted services usable");
        std::memcpy(pool.objects + P::IntelligenceOffset, &intelligenceAddress, sizeof(intelligenceAddress));
        orientationCall.action = [&](NativeGameTimingAccess& access)
        { WithPedTeleportActor(fixture, pool, [&] { orientationResult = access.SetPedOrientation(orientationToken, orientationTarget); }); };
        invokeOrientation();
        Check(orientationResult == ClockResult::Ok && ClockPedOrientationOwnerTest::calls == 3 && !runtime.stopping.load(),
              "refusal and RAII pose-gate release allow a later genuine orientation operation");
        retire(orientationService);
    }
    std::puts("Production actual ped three-axis orientation SDK callback fixture completed");
    for (unsigned failPhase = 1; failPhase <= 3; ++failPhase)
    {
        resetOrientation(true);
        NativeClockService      orientationService;
        NativePedReferenceToken orientationToken;
        ClockResult             orientationResult = ClockResult::NotInitialized;
        orientationCall.action = [&](NativeGameTimingAccess& access) { orientationResult = access.AcquirePedReference(1, orientationToken); };
        BeginOwned(orientationService, fixture, false, true, true, &OrientationCallback::Invoke, &orientationCall);
        fixture.invokeTimer();
        Check(orientationResult == ClockResult::Ok, "each entered orientation-prefix case has a fresh admitted owner");
        ClockPedOrientationOwnerTest::failAt = failPhase;
        orientationCall.action = [&](NativeGameTimingAccess& access)
        { WithPedTeleportActor(fixture, pool, [&] { orientationResult = access.SetPedOrientation(orientationToken, orientationTarget); }); };
        Check(orientationService.Renew(runtime.generation.load()), "renew before one uncertain native orientation phase");
        fixture.invokeTimer();
        Check(orientationResult == ClockResult::OriginalException && runtime.stopping.load() && ClockPedOrientationOwnerTest::calls == failPhase,
              "entered native failure retires only its same original owner and preserves the exact phase prefix without Add compensation");
        if (failPhase >= 2)
            Check(!std::memcmp(matrix.data(), &orientationTarget, sizeof(orientationTarget)),
                  "possibly accepted native rotation remains after lost acknowledgement");
        fixture.invokeTimer();
        Check(ClockPedOrientationOwnerTest::calls == failPhase, "stopping orientation owner never retries an entered phase or synthesizes Add compensation");
        retire(orientationService);
    }
    {
        resetOrientation(true);
        NativeClockService      orientationService;
        NativePedReferenceToken orientationToken;
        ClockResult             orientationResult = ClockResult::NotInitialized;
        orientationCall.action = [&](NativeGameTimingAccess& access) { orientationResult = access.AcquirePedReference(1, orientationToken); };
        BeginOwned(orientationService, fixture, false, true, true, &OrientationCallback::Invoke, &orientationCall);
        fixture.invokeTimer();
        Check(orientationResult == ClockResult::Ok, "post-orientation registered-cell loss token admission");
        const auto orientationNode = ClockPedReferenceOwnerTest::node;
        const auto orientationUnlinks = ClockPedReferenceOwnerTest::unregisters;
        ClockPedOrientationOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedOrientationOwnerTest::loseOnAdd = true;
        orientationCall.action = [&](NativeGameTimingAccess& access)
        { WithPedTeleportActor(fixture, pool, [&] { orientationResult = access.SetPedOrientation(orientationToken, orientationTarget); }); };
        Check(orientationService.Renew(runtime.generation.load()), "renew before completed native orientation loses its weak acknowledgement");
        fixture.invokeTimer();
        Check(orientationResult != ClockResult::Ok && runtime.stopping.load() && ClockPedOrientationOwnerTest::calls == 3 &&
                  !std::memcmp(matrix.data(), &orientationTarget, sizeof(orientationTarget)) && runtime.pedReferences.IsRetained() && !orientationService.End(),
              "post-native token loss keeps accepted orientation and original native-visible reference-cell debt");
        fixture.invokeTimer();
        Check(ClockPedOrientationOwnerTest::calls == 3 && ClockPedReferenceOwnerTest::unregisters == orientationUnlinks && !orientationService.End(),
              "orientation debt retirement neither replays accepted phases nor repeats uncertain native cleanup");
        ClockPedReferenceOwnerTest::Put(orientationNode + 8, std::uintptr_t{0});
        const auto orientationHead = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(orientationNode, orientationHead);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, orientationNode);
        ClockPedReferenceOwnerTest::Put(pedAddress + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(orientationService);
        Check(ClockPedOrientationOwnerTest::calls == 3 && !std::memcmp(matrix.data(), &orientationTarget, sizeof(orientationTarget)),
              "only proved native bank-wide address absence finishes retirement; orientation is never restoration data");
    }
    ClockPedOrientationOwnerTest::Reset();
    ClockPedPhysicalScalarsOwnerTest::cell = 0;
    ClockPedPhysicalScalarsOwnerTest::loseToken = false;
    std::puts("Production native ped three-phase orientation callback owner fixture completed");
}

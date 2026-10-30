/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual camera SDK observations and original Restore in one owner
 ****************************************************************************/
namespace CameraBasicsOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace R = NativePedReferenceProfile;
    namespace C = NativeCameraRestoreProfile;
    struct Node
    {
        std::uintptr_t next = 0, cell = 0;
    };
    static_assert(sizeof(Node) == R::ReferenceBytes);
    std::uintptr_t mutateAddress = 0;
    unsigned       ownerChecks = 0, mutateOn = 0;
    ClockResult    MutatingOwner() noexcept
    {
        const auto result = CheckTimingOwner();
        if (result == ClockResult::Ok && ++ownerChecks == mutateOn)
            *reinterpret_cast<std::uint32_t*>(mutateAddress) ^= 1;
        return result;
    }

    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* data, std::size_t bytes, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, data, bytes, protection); };
        const auto admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativePedPoolProfile::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
        for (const auto& pin : NativePlayerInfoProfile::Code)
            apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativePlayerInfoExtraProfile::Code)
            apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& group : P::Groups)
        {
            writes.Global(group.slotRva, fixture.image + group.entryRva, PAGE_READONLY);
            writes.Global(group.tableSlotRva, static_cast<std::uint32_t>(group.branchRva), PAGE_EXECUTE_READ);
        }
        writes.Global(P::CrtPointerRva, fixture.image + P::CrtWrapperRva, PAGE_READONLY);
        apply(C::EntryRva, C::Restore.data(), C::Restore.size(), PAGE_EXECUTE_READ);
        fixture.Seed();
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        ClockTeleportPool peds(fixture, NativePedPoolProfile::RootRva, NativePedPoolProfile::NativeCapacity, NativePedPoolProfile::ObjectStride);
        peds.Publish();
        peds.flags[0] = 1;
        const auto ped = reinterpret_cast<std::uintptr_t>(peds.objects);
        const auto putPed = [&](std::size_t offset, const auto& value) { std::memcpy(peds.objects + offset, &value, sizeof(value)); };
        putPed(0, fixture.image + NativePlayerInfoProfile::PlayerVtableRva);
        putPed(0x5f0, fixture.image + NativePlayerInfoProfile::BankRva + 8);
        putPed(0x7e0, std::int32_t{0});
        putPed(0x6a, static_cast<unsigned char>(3));
        putPed(0x634, std::uint32_t{0});
        writes.Global(NativePlayerInfoProfile::BankRva, ped);
        writes.Global(NativePlayerInfoProfile::PlayerIndexRva, static_cast<unsigned char>(0));
        writes.Global(NativePlayerInfoProfile::PlayerVtableRva + 0x50, fixture.image + NativePlayerInfoProfile::PlayerControlRva, PAGE_READONLY);
        // The reference profile supplies the exact first virtual target;
        // preserve it while adding the independently qualified control slot.
        const auto oldReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = ClockCameraRestoreOwnerTest::image = fixture.image;
        ClockCameraRestoreOwnerTest::calls = 0;
        ClockCameraRestoreOwnerTest::scenario = 0;
        const auto nodeAddress = [&](std::size_t index) { return fixture.image + R::ReferencesRva + index * sizeof(Node); };
        const auto seed = [&](std::uint32_t mode, bool primary, bool secondary)
        {
            std::array<unsigned char, C::CameraBytes> camera;
            camera.fill(0x5a);
            const auto put = [&](std::size_t offset, const auto& value) { std::memcpy(camera.data() + offset, &value, sizeof(value)); };
            put(0, fixture.image + P::CameraVtableRva);
            put(0x33, std::uint8_t{0});
            put(0x7f8, primary ? ped : std::uintptr_t{0});
            put(0x800, secondary ? ped : std::uintptr_t{0});
            apply(C::CameraRva, camera.data(), camera.size(), PAGE_READWRITE);
            std::array<Node, R::ReferenceCount> bank{};
            std::size_t                         used = 0;
            if (primary)
                bank[used++].cell = fixture.image + C::CameraRva + 0x7f8;
            if (secondary)
                bank[used++].cell = fixture.image + C::CameraRva + 0x800;
            for (std::size_t i = 0; i != used; ++i)
                bank[i].next = i + 1 < used ? nodeAddress(i + 1) : 0;
            for (std::size_t i = used; i != bank.size(); ++i)
                bank[i].next = i + 1 < bank.size() ? nodeAddress(i + 1) : 0;
            apply(R::ReferencesRva, bank.data(), sizeof(bank), PAGE_READWRITE);
            writes.Global(R::FreeHeadRva, nodeAddress(used));
            putPed(R::EntityHeadOffset, used ? nodeAddress(0) : std::uintptr_t{0});
            putPed(0x5fc, mode);
        };
        seed(0, false, false);

        CameraBasicsObserver camera(runtime.access);
        CCamera&             sdk = camera;
        CamFovObserver       row0(0, runtime.access), row1(1, runtime.access), row2(2, runtime.access), invalidRow(3, runtime.access);
        std::array<CCam*, 3> rows{&row0, &row1, &row2};
        std::uint8_t         active = 0xa5;
        std::uint32_t        fov = 0xdeadbeef;
        Check(runtime.access.ReadCameraActiveIndex(active) != ClockResult::Ok && active == 0xa5 && runtime.access.ReadCameraFov(0, fov) != ClockResult::Ok &&
                  fov == 0xdeadbeef && runtime.access.RestoreCamera() != ClockResult::Ok && !ClockCameraRestoreOwnerTest::calls,
              "unbound actual camera owner preserves outputs and enters no original Restore");
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockServiceRequestV1             request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.modulePinned,
              "actual camera post-frame Begin retains the real wrong-executable rejection");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const auto began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && state.hooked && state.modulePinned && runtime.frameHooked.load() && !runtime.timerHooked.load(),
              "public camera Begin borrows one genuine post-original frame owner without a timer substitute");
        const auto pulse = [&]
        {
            Check(service.Renew(state.generation), "renew only this camera native callback owner");
            StaticAssociationInitializationOwnerFixture::ProducerPulse producer(service, state.generation);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
            producer.Stop();
            Check(!producer.unexpectedFailures.load(), "camera producer preserves the actual post-frame handoff");
        };
        ClockResult result = ClockResult::NotInitialized;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(CheckApplicationTimingOwner() == ClockResult::Ok && timingCallbackGeneration == state.generation && !timingTimerScope,
                  "camera calls execute only in the actual admitted post-original scope");
            result = access.ReadCameraActiveIndex(active);
        };

        if (!scenario)
        {
            std::thread foreign([&] { result = runtime.access.ReadCameraFov(0, fov); });
            foreign.join();
            Check(result != ClockResult::Ok && fov == 0xdeadbeef && !runtime.stopping.load(),
                  "a foreign thread cannot borrow the retained camera owner or modify caller bits");
            for (const std::uint8_t value : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{255}})
            {
                fixture.Put(P::ActiveRva, value);
                callback.action = [&](NativeGameTimingAccess&) { active = sdk.GetActiveCam(); };
                pulse();
                Check(active == value && !runtime.stopping.load(), "actual camera SDK returns every native selector BYTE without clamping");
            }
            const std::array<std::uint32_t, 8> bits{0, 0x80000000u, 0x3f800001u, 0xc1200000u, 0x42c80000u, 0x7fc12345u, 0x7f800000u, 0xff800000u};
            for (std::size_t row = 0; row != rows.size(); ++row)
                for (const auto pattern : bits)
                {
                    fixture.Put(P::CameraRva + P::ArrayOffset + row * P::RowBytes + P::FovOffset, pattern);
                    float observed = 123;
                    callback.action = [&](NativeGameTimingAccess& access)
                    {
                        result = access.ReadCameraFov(static_cast<std::uint32_t>(row), fov);
                        observed = rows[row]->GetFOV();
                    };
                    pulse();
                    const bool finite = (pattern & 0x7f800000u) != 0x7f800000u;
                    Check(result == ClockResult::Ok && fov == pattern &&
                              WorldGroundOwnerFixture::Bits(observed) == (finite ? pattern : WorldGroundOwnerFixture::Bits(70.0f)),
                          "actual logical-row Cam SDK preserves finite FOV bits and applies existing nonfinite70 policy after raw observation");
                }
            callback.action = [&](NativeGameTimingAccess& access)
            {
                fov = 0xdeadbeef;
                bool threw = false;
                try
                {
                    invalidRow.GetFOV();
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                Check(threw && access.ReadCameraFov(3, fov) == ClockResult::InvalidArgument &&
                          access.ReadCameraFov(0xffffffffu, fov) == ClockResult::InvalidArgument && fov == 0xdeadbeef,
                      "logical rows outside three constructed objects refuse and preserve output instead of wrapping the active BYTE");
                active = 0xa5;
                fov = 0xdeadbeef;
                Check(NativeCameraDataMemory::ReadActive(fixture.image, nullptr, active) == ClockResult::NotInitialized &&
                          NativeCameraDataMemory::ReadFov(fixture.image, 0, nullptr, fov) == ClockResult::NotInitialized && active == 0xa5 && fov == 0xdeadbeef,
                      "missing native owner cannot publish scalar fallback values");
            };
            pulse();
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the existing shared gate before camera scalar observation");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadCameraFov(0, fov); };
            pulse();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && fov == 0xdeadbeef && !runtime.stopping.load(),
                  "camera scalar shares the existing gate and preserves refused bits");
            callback.action = [&](NativeGameTimingAccess& access)
            {
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                result = access.ReadCameraFov(0, fov);
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                Check(result == ClockResult::Busy && fov == 0xdeadbeef, "real native GC excludes camera observations without touching caller bits");
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
                result = access.ReadCameraFov(0, fov);
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
                Check(result != ClockResult::Ok && fov == 0xdeadbeef, "a retained callback alone cannot manufacture native playing state");
                const auto oldType = fixture.Get<std::uintptr_t>(P::CameraRva);
                fixture.Put(P::CameraRva, std::uintptr_t{0});
                result = access.ReadCameraFov(0, fov);
                fixture.Put(P::CameraRva, oldType);
                Check(result == ClockResult::BindingChanged && fov == 0xdeadbeef, "constructed camera type must remain current around every data observation");
                const auto original = P::NamedFov[0];
                const auto changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(0x10c36c5, &changed, 1);
                result = access.ReadCameraFov(0, fov);
                fixture.Patch(0x10c36c5, &original, 1);
                Check(result == ClockResult::UnsupportedBuild && fov == 0xdeadbeef, "modified named FOV code cannot retain old scalar authority");
                mutateAddress = fixture.image + P::CameraRva + P::ArrayOffset + P::FovOffset;
                ownerChecks = 0;
                mutateOn = 2;
                result = NativeCameraDataMemory::ReadFov(fixture.image, 0, &MutatingOwner, fov);
                Check(result == ClockResult::BindingChanged && fov == 0xdeadbeef,
                      "a changing native FOV between owner checks cannot publish a mixed observation");
                mutateAddress = graph.World() + NativeStreamingLoadAllProfile::WorldModeOffset;
                ownerChecks = 0;
                mutateOn = 3;
                result = NativeCameraDataMemory::ReadFov(fixture.image, 0, &MutatingOwner, fov);
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), NativeStreamingLoadAllProfile::WorldModeOffset,
                                                          std::uint32_t{0});
                mutateOn = 0;
                Check(result == ClockResult::BindingChanged && fov == 0xdeadbeef, "final supplied owner changes require a new matching typed-world capture");
                const auto serial = frameCallbackSerial;
                --frameCallbackSerial;
                result = access.RestoreCamera();
                frameCallbackSerial = serial;
                Check(result != ClockResult::Ok && !ClockCameraRestoreOwnerTest::calls, "Restore cannot borrow a previous camera frame serial");
                fixture.Put(R::FreeHeadRva, std::uintptr_t{0});
                result = access.RestoreCamera();
                fixture.Put(R::FreeHeadRva, nodeAddress(0));
                Check(result == ClockResult::BindingChanged && !ClockCameraRestoreOwnerTest::calls && !runtime.stopping.load(),
                      "an exhausted native reference bank refuses before the original without invoking its global prune fallback");
                putPed(0x634, std::uint32_t{0x100});
                result = access.RestoreCamera();
                putPed(0x634, std::uint32_t{0});
                Check(result == ClockResult::BindingChanged && !ClockCameraRestoreOwnerTest::calls,
                      "unqualified vehicle targets confer no Restore mutation authority");
            };
            pulse();
            Check(!runtime.stopping.load() && !runtime.cameraRestore.IsRetained(), "all before-entry scalar/Restore refusals leave the same owner usable");
            std::puts("Production camera basics current owner refusals fixture completed");
            for (const auto mode : {0u, 0x35u, 0x39u, 0x3bu, 0x3du})
            {
                seed(mode, true, true);
                const auto before = ClockCameraRestoreOwnerTest::calls;
                callback.action = [&](NativeGameTimingAccess&) { sdk.Restore(); };
                pulse();
                Check(
                    ClockCameraRestoreOwnerTest::calls == before + 1 && runtime.cameraRestoreReceipt.effectsVerified && !runtime.cameraRestoreReceipt.debt &&
                        !runtime.cameraRestore.IsRetained() && fixture.Get<std::uintptr_t>(C::CameraRva + 0x7f8) == ped &&
                        fixture.Get<std::uintptr_t>(C::CameraRva + 0x800) == 0 &&
                        fixture.Get<std::uint16_t>(C::CameraRva + 0xaf0) == (mode == 0x35 || mode == 0x39 || mode == 0x3b ? 0x12 : 4) &&
                        fixture.Get<std::uint16_t>(C::CameraRva + 0xaf4) == 1 && fixture.Get<std::uint32_t>(C::CameraRva + 0xaec) == 30 &&
                        fixture.Get<unsigned char>(C::CameraRva + 0x55) == 0x5a,
                    "actual camera Restore executes the whole original once and verifies genuine unregister/register effects and unrelated-byte preservation");
                std::uintptr_t head = 0;
                std::memcpy(&head, peds.objects + R::EntityHeadOffset, sizeof(head));
                Check(head && fixture.Get<Node>(head - fixture.image).cell == fixture.image + C::CameraRva + 0x7f8 &&
                          fixture.Get<Node>(head - fixture.image).next == 0,
                      "native reference helpers leave exactly the primary static camera cell in the actual player chain");
            }
            fixture.Put(C::CameraRva + 0x33, std::uint8_t{1});
            const auto cameraBefore = fixture.Get<std::array<unsigned char, C::CameraBytes>>(C::CameraRva);
            const auto bankBefore = fixture.Get<std::array<Node, R::ReferenceCount>>(R::ReferencesRva);
            callback.action = [&](NativeGameTimingAccess&) { sdk.Restore(); };
            pulse();
            const auto bankAfter = fixture.Get<std::array<Node, R::ReferenceCount>>(R::ReferencesRva);
            Check(runtime.cameraRestoreReceipt.noEffect && runtime.cameraRestoreReceipt.effectsVerified &&
                      fixture.Get<std::array<unsigned char, C::CameraBytes>>(C::CameraRva) == cameraBefore &&
                      !std::memcmp(bankBefore.data(), bankAfter.data(), sizeof(bankBefore)),
                  "the original source no-effect branch returns without rewriting any camera or native reference byte");
            Check(service.End() && !service.IsRetained() && !runtime.frameHooked.load() && !runtime.cameraRestore.IsRetained(),
                  "acknowledged Restore retires the actual callback owner without cleanup, rollback or another native invocation");
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
            ClockCameraRestoreOwnerTest::image = 0;
            std::puts("Production camera restore whole original reference effects fixture completed");
            std::puts("Production camera basics actual SDK owner fixture completed");
            return;
        }

        ClockCameraRestoreOwnerTest::scenario = scenario;
        bool threw = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                sdk.Restore();
            }
            catch (const std::exception&)
            {
                threw = true;
            }
        };
        pulse();
        const auto calls = ClockCameraRestoreOwnerTest::calls;
        Check(threw && calls == 1 && runtime.stopping.load() && runtime.cameraRestoreReceipt.entered && runtime.cameraRestoreReceipt.debt &&
                  runtime.cameraRestore.RecoveryPending() && service.IsRetained() && runtime.frameHooked.load(),
              "an entered unknown exception, lost owner or camera mismatch preserves the real Restore journal and stopped module owner");
        Check(runtime.cameraRestore.Restore(fixture.image, runtime.playerInfo, &CheckTimingOwner, &CheckApplicationTimingOwner) == ClockResult::RestoreFailure,
              "the component refuses to repeat an uncertain original even when another caller requests Restore");
        ClockCameraRestoreOwnerTest::scenario = 0;
        Check(!service.End() && service.IsRetained(), "producer End cannot erase native camera reference debt");
        fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
        Check(!service.End() && service.IsRetained() && runtime.cameraRestore.RecoveryPending() && ClockCameraRestoreOwnerTest::calls == calls,
              "stopped natural frames and repeated End cannot replay, reverse or discard uncertain camera effects");
        const char* names[] = {"", "entered-before", "entered-after", "owner-loss", "effect-mismatch"};
        std::printf("Production camera restore %s retained debt fixture completed\n", names[scenario]);
        // This process now owns the unresolved native journal. Preserve every
        // fixture byte until process retirement instead of manufacturing ACK.
    }
}

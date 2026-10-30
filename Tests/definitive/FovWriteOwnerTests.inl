/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CCam FOV setter with native row CAS and retained write debt
 ****************************************************************************/
namespace CameraFovWriteOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    struct ObservedProvider final : NativeCameraSphereVisibleAccess
    {
        unsigned    writes = 0;
        ClockResult WriteCameraFov(std::uint32_t row, std::uint32_t bits) noexcept override
        {
            ++writes;
            return runtime.access.WriteCameraFov(row, bits);
        }
        ClockResult ReadCameraFov(std::uint32_t row, std::uint32_t& bits) noexcept override { return runtime.access.ReadCameraFov(row, bits); }
    };
    float FromBits(std::uint32_t bits)
    {
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* value, std::size_t bytes, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, value, bytes, protection); };
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
        for (const auto& pin : P::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& group : P::Groups)
        {
            writes.Global(group.slotRva, fixture.image + group.entryRva, PAGE_READONLY);
            writes.Global(group.tableSlotRva, static_cast<std::uint32_t>(group.branchRva), PAGE_EXECUTE_READ);
        }
        writes.Global(P::CrtPointerRva, fixture.image + P::CrtWrapperRva, PAGE_READONLY);
        fixture.Seed();
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        // A scalar FOV setter owns no player/target/reference or matrix path.
        // Keep the native ped bank absent while the real world owner is live.
        writes.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
        std::array<unsigned char, P::CameraBytes> camera;
        camera.fill(0x5a);
        const auto vtable = fixture.image + P::CameraVtableRva;
        std::memcpy(camera.data(), &vtable, sizeof(vtable));
        camera[0x5b] = 255;
        apply(P::CameraRva, camera.data(), camera.size(), PAGE_READWRITE);
        const auto rowRva = [](std::uint32_t row) { return P::CameraRva + P::ArrayOffset + row * P::RowBytes + P::FovOffset; };
        for (std::uint32_t row = 0; row != 3; ++row)
            fixture.Put(rowRva(row), std::uint32_t{0x7fc12345});
        const auto oldReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = ClockCameraFovWriteOwnerTest::image = fixture.image;
        ClockCameraFovWriteOwnerTest::worldMode = graph.World() + NativeStreamingLoadAllProfile::WorldModeOffset;
        ClockCameraFovWriteOwnerTest::calls = ClockCameraFovWriteOwnerTest::scenario = 0;
        ObservedProvider                   provider;
        CamFovObserver                     row0(0, provider), row1(1, provider), row2(2, provider), invalidRow(0xffffffffu, provider);
        std::array<CCam*, 3>               rows{&row0, &row1, &row2};
        const std::array<std::uint32_t, 9> invalid{0, 0x80000000u, 0x80000001u, 0xbf800000u, 0x43340000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u};
        const auto                         beforeInvalid = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
        for (const auto bits : invalid)
            for (CCam* cam : {static_cast<CCam*>(&row0), static_cast<CCam*>(&invalidRow)})
                cam->SetFOV(FromBits(bits));
        Check(!provider.writes && !ClockCameraFovWriteOwnerTest::calls && beforeInvalid == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva),
              "invalid actual CCam FOV values are no-ops before provider or row admission, even outside an owner");
        bool threw = false;
        try
        {
            row0.SetFOV(90.0f);
        }
        catch (const std::exception&)
        {
            threw = true;
        }
        Check(threw && provider.writes == 1 && !ClockCameraFovWriteOwnerTest::calls &&
                  beforeInvalid == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva),
              "valid actual SDK FOV outside the retained callback throws without changing native bytes");
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockServiceRequestV1             request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.modulePinned,
              "FOV writer preserves public wrong-executable admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && state.hooked && state.modulePinned && runtime.frameHooked.load() && !runtime.timerHooked.load(),
              "FOV writes borrow the actual public post-original application-frame owner");
        const auto pulse = [&]
        {
            Check(service.Renew(state.generation), "renew the same FOV write owner");
            StaticAssociationInitializationOwnerFixture::ProducerPulse producer(service, state.generation);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
            producer.Stop();
            Check(!producer.unexpectedFailures.load(), "independent FOV producer preserves native callback authority");
        };
        ClockResult result = ClockResult::NotInitialized;
        if (!scenario)
        {
            std::thread foreign([&] { result = runtime.access.WriteCameraFov(0, 0x42b40000u); });
            foreign.join();
            Check(result != ClockResult::Ok && !ClockCameraFovWriteOwnerTest::calls && !runtime.stopping.load(),
                  "a foreign thread cannot inherit FOV mutation authority");
            callback.action = [&](NativeGameTimingAccess& access)
            {
                const auto refusal = [&](const char* why)
                {
                    const auto before = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
                    result = access.WriteCameraFov(0, 0x42b40000u);
                    Check(result != ClockResult::Ok && !ClockCameraFovWriteOwnerTest::calls && !runtime.cameraFovWrite.IsRetained() &&
                              !runtime.stopping.load() && before == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva),
                          why);
                };
                const auto serial = frameCallbackSerial;
                --frameCallbackSerial;
                refusal("a previous frame serial grants no current FOV write phase");
                frameCallbackSerial = serial;
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                refusal("native GC excludes FOV writes before their atomic effect");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
                refusal("retained callback intent cannot substitute for native playing9");
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
                fixture.Put(P::CameraRva, std::uintptr_t{0});
                refusal("current constructed camera type is required independently of writable image pages");
                fixture.Put(P::CameraRva, vtable);
                const auto original = P::NamedFov[0];
                const auto changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(0x10c36c5, &changed, 1);
                refusal("exact named source FOV consumer code must remain current before mutation");
                fixture.Patch(0x10c36c5, &original, 1);
                const auto index = std::int32_t{123};
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, index);
                refusal("a stale typed-world registry index cannot authorize FOV writes");
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{3});
                result = access.WriteCameraFov(3, 0x42b40000u);
                Check(result == ClockResult::InvalidArgument && !ClockCameraFovWriteOwnerTest::calls && !runtime.stopping.load(),
                      "logical FOV rows outside the genuine three-object array refuse before native entry");
                const auto calls = provider.writes;
                for (const auto bits : invalid)
                    invalidRow.SetFOV(FromBits(bits));
                Check(provider.writes == calls && !ClockCameraFovWriteOwnerTest::calls,
                      "invalid SDK FOV values still avoid provider calls inside a valid native owner");
            };
            pulse();
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the shared camera/native operation gate before FOV write");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.WriteCameraFov(0, 0x42b40000u); };
            pulse();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && !ClockCameraFovWriteOwnerTest::calls && !runtime.stopping.load(),
                  "FOV write cannot bypass an existing native operation gate");
            std::puts("Production camera FOV write current owner refusals fixture completed");
            const std::array<std::uint32_t, 7> patterns{1, 0x007fffffu, 0x00800000u, 0x3f800001u, 0x428c0000u, 0x42b40000u, 0x4333ffffu};
            for (std::uint32_t row = 0; row != 3; ++row)
                for (const auto replacement : patterns)
                {
                    fixture.Put(P::ActiveRva, row == 1 ? std::uint8_t{255} : static_cast<std::uint8_t>((row + 1) % 3));
                    auto       expected = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
                    const auto offset = P::ArrayOffset + row * P::RowBytes + P::FovOffset;
                    std::memcpy(expected.data() + offset, &replacement, sizeof(replacement));
                    const auto calls = ClockCameraFovWriteOwnerTest::calls;
                    threw = false;
                    float observed = 0;
                    callback.action = [&](NativeGameTimingAccess&)
                    {
                        Check(CheckApplicationTimingOwner() == ClockResult::Ok && !timingTimerScope,
                              "actual CCam setter retains this natural post-original frame phase");
                        try
                        {
                            rows[row]->SetFOV(FromBits(replacement));
                            observed = rows[row]->GetFOV();
                        }
                        catch (const std::exception& error)
                        {
                            threw = true;
                            std::fprintf(stderr, "FOV SDK refused: %s\n", error.what());
                        }
                    };
                    pulse();
                    Check(!threw && ClockCameraFovWriteOwnerTest::calls == calls + 1 && runtime.cameraFovWriteReceipt.effectsVerified &&
                              runtime.cameraFovWriteReceipt.logicalRow == row && runtime.cameraFovWriteReceipt.replacement == replacement &&
                              WorldGroundOwnerFixture::Bits(observed) == replacement && !runtime.cameraFovWrite.IsRetained() &&
                              expected == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva) && !runtime.stopping.load(),
                          "actual SDK writes only its logical native FOV DWORD, preserving valid subnormal/boundary bits and every other camera byte "
                          "independently of active selector");
                }
            Check(!runtime.pedReferences.IsRetained() && fixture.Get<std::uintptr_t>(NativePedPoolProfile::RootRva) == 0,
                  "FOV scalar writes create no ped, matrix or permanent camera reference dependency");
            Check(service.End() && !service.IsRetained() && !runtime.cameraFovWrite.IsRetained() && !runtime.frameHooked.load(),
                  "acknowledged FOV writes retire their real owner without restoration or replay");
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
            ClockCameraFovWriteOwnerTest::image = 0;
            std::puts("Production camera FOV write original atomic fixture completed");
            std::puts("Production camera FOV write actual SDK owner fixture completed");
            return;
        }

        fixture.Put(rowRva(1), std::uint32_t{0x428c0000});
        ClockCameraFovWriteOwnerTest::scenario = scenario;
        threw = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                row1.SetFOV(90.0f);
            }
            catch (const std::exception&)
            {
                threw = true;
            }
        };
        pulse();
        const auto& receipt = runtime.cameraFovWriteReceipt;
        if (scenario == 1)
        {
            Check(threw && receipt.result == ClockResult::Busy && receipt.entered && receipt.returned && !receipt.accepted && receipt.knownNoEffect &&
                      !receipt.debt && !runtime.cameraFovWrite.IsRetained() && !runtime.stopping.load() && ClockCameraFovWriteOwnerTest::calls == 1 &&
                      fixture.Get<std::uint32_t>(rowRva(1)) == 0x42800000u,
                  "a known native CAS mismatch preserves the competing FOV without Stop, debt or automatic repair");
            ClockCameraFovWriteOwnerTest::scenario = 0;
            threw = false;
            pulse();
            Check(!threw && ClockCameraFovWriteOwnerTest::calls == 2 && fixture.Get<std::uint32_t>(rowRva(1)) == 0x42b40000u,
                  "a new explicit SDK request can use the same healthy owner after a known no-effect conflict");
            Check(service.End() && !service.IsRetained(), "known no-effect conflict leaves no retirement debt");
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
            std::puts("Production camera FOV write known rejected atomic fixture completed");
            return;
        }
        Check(threw && receipt.entered && receipt.debt && runtime.cameraFovWrite.RecoveryPending() && runtime.stopping.load() && service.IsRetained() &&
                  runtime.frameHooked.load() && ClockCameraFovWriteOwnerTest::calls == 1,
              "accepted or unknown FOV atomic uncertainty retains the exact native journal and stopped module owner");
        if (scenario == 3 || scenario == 4 || scenario == 6 || scenario == 7)
            Check(fixture.Get<std::uint32_t>(rowRva(1)) == 0x42b40000u, "accepted FOV bits survive a lost post-write acknowledgement without rollback");
        if (scenario == 2)
            Check(fixture.Get<std::uint32_t>(rowRva(1)) == 0x428c0000u,
                  "an unknown pre-store failure retains debt without fabricating a successful FOV effect");
        if (scenario == 5)
            Check(fixture.Get<std::uint32_t>(rowRva(1)) == 0x42800000u, "a later foreign FOV value survives failed acknowledgement without repair");
        const auto calls = ClockCameraFovWriteOwnerTest::calls;
        Check(runtime.cameraFovWrite.Write(fixture.image, 1, 0x428c0000u, &CheckTimingOwner, &CheckApplicationTimingOwner) == ClockResult::RestoreFailure,
              "retained FOV write debt refuses to replay or repair a previous entered operation");
        ClockCameraFovWriteOwnerTest::scenario = 0;
        Check(!service.End() && service.IsRetained(), "producer End cannot erase uncertain native FOV effects");
        fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
        Check(!service.End() && service.IsRetained() && runtime.cameraFovWrite.RecoveryPending() && ClockCameraFovWriteOwnerTest::calls == calls,
              "stopped natural frames preserve the FOV journal without native retries or cached restoration");
        const char* names[] = {"", "", "before", "after", "owner-loss", "value-mismatch", "world-drift", "code-drift"};
        std::printf("Production camera FOV write %s retained debt fixture completed\n", names[scenario]);
    }
}

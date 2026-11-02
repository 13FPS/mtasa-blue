/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CCam mode and ordered direction pair ownership and debt
 ****************************************************************************/
namespace CameraDirectionOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace Q = NativeCameraDirectionProfile;
    constexpr std::size_t HorizontalOffset = 0x10, VerticalOffset = 0, ModeOffset = 0x28;
    std::uintptr_t        RowRva(std::uint32_t row)
    {
        return P::CameraRva + P::ArrayOffset + row * P::RowBytes;
    }
    float FromBits(std::uint32_t bits)
    {
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::uint32_t Bits(float value)
    {
        return WorldGroundOwnerFixture::Bits(value);
    }
    float SourceWrap(float value)
    {
        // This is the existing SA policy oracle. The owner fixture compares
        // its SDK result and native writes; the native controller never wraps.
        constexpr float pi = 3.14159265358979323846f, tau = 6.28318530717958647692f;
        value -= tau * std::floor((value + pi) / tau);
        if (value <= -pi)
            value += tau;
        else if (value > pi)
            value -= tau;
        return value;
    }
    std::uint32_t OutputBits(std::uint32_t original)
    {
        const auto value = FromBits(original);
        return Bits(std::isfinite(value) ? SourceWrap(value) : 0.0f);
    }
    struct ObservedProvider final : NativeCameraSphereVisibleAccess
    {
        unsigned       modes = 0, reads = 0, repairs = 0, writes = 0, driftBeforeRepair = 0;
        std::uintptr_t image = 0;
        ClockResult    lastRepair = ClockResult::NotInitialized;
        ClockResult    ReadCameraMode(std::uint32_t row, std::uint16_t& output) noexcept override
        {
            ++modes;
            return runtime.access.ReadCameraMode(row, output);
        }
        ClockResult ReadCameraDirection(std::uint32_t row, std::uint32_t& horizontal, std::uint32_t& vertical) noexcept override
        {
            ++reads;
            return runtime.access.ReadCameraDirection(row, horizontal, vertical);
        }
        ClockResult ConditionalRepairCameraDirection(std::uint32_t row, std::uint32_t expectedH, std::uint32_t expectedV, std::uint32_t horizontal,
                                                     std::uint32_t vertical) noexcept override
        {
            ++repairs;
            // Drift belongs between the real raw read and conditional repair,
            // so the fixture cannot accidentally authorize a newer pair.
            if (driftBeforeRepair == 1 || driftBeforeRepair == 2)
                *reinterpret_cast<std::uint32_t*>(image + RowRva(row) + (driftBeforeRepair == 1 ? HorizontalOffset : VerticalOffset)) = 0x3f000000u;
            if (driftBeforeRepair == 3)
                *reinterpret_cast<LONG*>(image + NativeBorrowedGcProfile::markerRva) = 1;
            lastRepair = runtime.access.ConditionalRepairCameraDirection(row, expectedH, expectedV, horizontal, vertical);
            return lastRepair;
        }
        ClockResult WriteCameraDirection(std::uint32_t row, std::uint32_t horizontal, std::uint32_t vertical) noexcept override
        {
            ++writes;
            return runtime.access.WriteCameraDirection(row, horizontal, vertical);
        }
    };
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
        for (const auto& pin : Q::Pins)
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
        // Direction/mode own no target entity, native ped bank, or vector borrow.
        writes.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
        std::array<unsigned char, P::CameraBytes> camera;
        camera.fill(0x5a);
        const auto vtable = fixture.image + P::CameraVtableRva;
        std::memcpy(camera.data(), &vtable, sizeof(vtable));
        camera[0x5b] = 255;
        apply(P::CameraRva, camera.data(), camera.size(), PAGE_READWRITE);
        const auto putPair = [&](std::uint32_t row, std::uint32_t horizontal, std::uint32_t vertical)
        {
            fixture.Put(RowRva(row) + HorizontalOffset, horizontal);
            fixture.Put(RowRva(row) + VerticalOffset, vertical);
        };
        for (std::uint32_t row = 0; row != 3; ++row)
            putPair(row, Bits(10.0f), Bits(-8.0f));
        const auto oldReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = ClockCameraDirectionOwnerTest::image = fixture.image;
        ClockCameraDirectionOwnerTest::worldMode = graph.World() + NativeStreamingLoadAllProfile::WorldModeOffset;
        ClockCameraDirectionOwnerTest::sourceRva = Q::Pins[0].rva;
        ClockCameraDirectionOwnerTest::calls = ClockCameraDirectionOwnerTest::step = ClockCameraDirectionOwnerTest::scenario = 0;
        const auto prepareExchange = [&](std::uint32_t row, std::uint32_t beforeH, std::uint32_t beforeV, std::uint32_t targetH, std::uint32_t targetV)
        {
            ClockCameraDirectionOwnerTest::targetH = fixture.image + RowRva(row) + HorizontalOffset;
            ClockCameraDirectionOwnerTest::targetV = fixture.image + RowRva(row) + VerticalOffset;
            ClockCameraDirectionOwnerTest::beforeH = beforeH;
            ClockCameraDirectionOwnerTest::beforeV = beforeV;
            ClockCameraDirectionOwnerTest::replacementH = targetH;
            ClockCameraDirectionOwnerTest::replacementV = targetV;
            ClockCameraDirectionOwnerTest::step = 0;
            ClockCameraDirectionOwnerTest::ordered = false;
        };
        ObservedProvider provider;
        provider.image = fixture.image;
        CamFovObserver                     row0(0, provider), row1(1, provider), row2(2, provider), invalidRow(0xffffffffu, provider);
        std::array<CCam*, 3>               rows{&row0, &row1, &row2};
        const std::array<std::uint32_t, 4> invalid{0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u};
        const auto                         beforeInvalid = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
        for (const auto bits : invalid)
            for (CCam* cam : {static_cast<CCam*>(&row0), static_cast<CCam*>(&invalidRow)})
            {
                cam->SetDirection(FromBits(bits), 1.0f);
                cam->SetDirection(1.0f, FromBits(bits));
            }
        Check(
            !provider.writes && !ClockCameraDirectionOwnerTest::calls && beforeInvalid == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva),
            "invalid actual CCam direction input is a no-op before provider and row admission outside an owner");
        float horizontal = 123.25f, vertical = -456.5f;
        bool  threw = false;
        try
        {
            row0.GetDirection(horizontal, vertical);
        }
        catch (const std::exception&)
        {
            threw = true;
        }
        Check(threw && horizontal == 123.25f && vertical == -456.5f && !ClockCameraDirectionOwnerTest::calls,
              "refused actual direction getter preserves both output references outside an owner");
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockServiceRequestV1             request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.modulePinned,
              "direction preserves public wrong-executable admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && state.hooked && state.modulePinned && runtime.frameHooked.load() && !runtime.timerHooked.load(),
              "actual camera direction uses the public retained post-original application frame");
        const auto pulse = [&]
        {
            Check(service.Renew(state.generation), "renew this same direction owner");
            StaticAssociationInitializationOwnerFixture::ProducerPulse producer(service, state.generation);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
            producer.Stop();
            Check(!producer.unexpectedFailures.load(), "independent direction producer preserves callback authority");
        };
        ClockResult result = ClockResult::NotInitialized;
        if (!scenario)
        {
            std::thread foreign([&] { result = runtime.access.WriteCameraDirection(0, Bits(1.0f), Bits(2.0f)); });
            foreign.join();
            Check(result != ClockResult::Ok && !ClockCameraDirectionOwnerTest::calls && !runtime.stopping.load(),
                  "foreign thread cannot inherit direction mutation authority");
            callback.action = [&](NativeGameTimingAccess& access)
            {
                const auto refusal = [&](const char* why)
                {
                    const auto before = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
                    result = access.WriteCameraDirection(0, Bits(1.0f), Bits(2.0f));
                    std::uint16_t rawMode = 0x1234;
                    std::uint32_t rawH = 0x12345678, rawV = 0x87654321;
                    const auto    modeResult = access.ReadCameraMode(0, rawMode);
                    const auto    pairResult = access.ReadCameraDirection(0, rawH, rawV);
                    Check(result != ClockResult::Ok && !ClockCameraDirectionOwnerTest::calls && !runtime.cameraDirection.IsRetained() &&
                              modeResult != ClockResult::Ok && pairResult != ClockResult::Ok && rawMode == 0x1234 && rawH == 0x12345678 && rawV == 0x87654321 &&
                              !runtime.stopping.load() && before == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva),
                          why);
                };
                const auto serial = frameCallbackSerial;
                --frameCallbackSerial;
                refusal("previous frame serial grants no current direction write phase");
                frameCallbackSerial = serial;
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                refusal("GC excludes ordered direction effects before their first CAS");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
                refusal("playing9 is independently required for direction writes");
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
                fixture.Put(P::CameraRva, std::uintptr_t{0});
                refusal("constructed camera type is required independently of writable image pages");
                fixture.Put(P::CameraRva, vtable);
                const auto original = Q::Pins[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(Q::Pins[0].rva, &changed, 1);
                refusal("exact direction source code is checked before native entry");
                fixture.Patch(Q::Pins[0].rva, &original, 1);
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{123});
                refusal("typed-world membership cannot be replaced by a readable stale pointer");
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{3});
                std::uint16_t mode = 0x1234;
                std::uint32_t rawH = 0x12345678, rawV = 0x87654321;
                Check(access.ReadCameraMode(0xffffffffu, mode) == ClockResult::InvalidArgument && mode == 0x1234 &&
                          access.ReadCameraDirection(0x10000u, rawH, rawV) == ClockResult::InvalidArgument && rawH == 0x12345678 && rawV == 0x87654321 &&
                          access.WriteCameraDirection(3, Bits(1.0f), Bits(2.0f)) == ClockResult::InvalidArgument && !ClockCameraDirectionOwnerTest::calls,
                      "full logical index domain refuses without truncation and preserves raw observation outputs");
                const auto calls = provider.writes;
                for (const auto bits : invalid)
                {
                    invalidRow.SetDirection(FromBits(bits), 1.0f);
                    invalidRow.SetDirection(1.0f, FromBits(bits));
                }
                Check(provider.writes == calls && !ClockCameraDirectionOwnerTest::calls,
                      "invalid actual SDK direction values avoid provider calls inside a current owner");
                horizontal = 123.25f;
                vertical = -456.5f;
                threw = false;
                const auto repairCalls = provider.repairs;
                try
                {
                    invalidRow.GetDirection(horizontal, vertical);
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                Check(threw && horizontal == 123.25f && vertical == -456.5f && provider.repairs == repairCalls,
                      "invalid actual getter row preserves outputs and never requests a conditional repair");
            };
            pulse();
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the shared native gate before ordered direction");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.WriteCameraDirection(0, Bits(1.0f), Bits(2.0f)); };
            pulse();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && !ClockCameraDirectionOwnerTest::calls && !runtime.stopping.load(),
                  "direction refuses a shared native operation gate before any effect");
            std::puts("Production camera direction current owner refusals fixture completed");

            const std::array<std::uint16_t, 9> modes{0, 1, 4, 15, 127, 0x7fff, 0x8000, 0x8001, 0xffff};
            for (std::uint32_t row = 0; row != 3; ++row)
                for (const auto word : modes)
                {
                    fixture.Put(P::ActiveRva, row == 1 ? std::uint8_t{255} : static_cast<std::uint8_t>((row + 1) % 3));
                    fixture.Put(RowRva(row) + ModeOffset, word);
                    const auto before = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
                    callback.action = [&](NativeGameTimingAccess&)
                    {
                        const auto   observed = rows[row]->GetMode();
                        std::int16_t signedMode = 0;
                        std::memcpy(&signedMode, &word, sizeof(word));
                        Check(observed == static_cast<unsigned int>(signedMode),
                              "actual SDK mode preserves signed native WORD conversion across the full sign boundary");
                    };
                    pulse();
                    Check(before == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva) && !ClockCameraDirectionOwnerTest::calls,
                          "mode reads its logical row independently of active selector without mutating any camera byte");
                }
            std::puts("Production camera mode raw word fixture completed");

            const std::array<float, 9> finite{0.0f,
                                              -0.0f,
                                              3.14159265358979323846f,
                                              -3.14159265358979323846f,
                                              6.28318530717958647692f,
                                              -8.0f,
                                              10.5f,
                                              1.0e20f,
                                              (std::numeric_limits<float>::max)()};
            for (std::uint32_t row = 0; row != 3; ++row)
                for (const auto originalH : finite)
                {
                    const auto originalV = -8.0f;
                    putPair(row, Bits(originalH), Bits(originalV));
                    const auto before = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
                    const auto reads = provider.reads, repairs = provider.repairs, calls = ClockCameraDirectionOwnerTest::calls;
                    callback.action = [&](NativeGameTimingAccess&)
                    {
                        horizontal = 123.25f;
                        vertical = -456.5f;
                        rows[row]->GetDirection(horizontal, vertical);
                        Check(Bits(horizontal) == Bits(SourceWrap(originalH)) && Bits(vertical) == Bits(SourceWrap(originalV)),
                              "finite actual getter uses the source wrapping policy including signed zero and huge finite inputs");
                        float alias = 99;
                        rows[row]->GetDirection(alias, alias);
                        Check(Bits(alias) == Bits(SourceWrap(originalV)), "SDK horizontal-then-vertical output order preserves alias semantics");
                    };
                    pulse();
                    Check(provider.reads == reads + 2 && provider.repairs == repairs && ClockCameraDirectionOwnerTest::calls == calls &&
                              before == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva),
                          "finite direction getter is raw-read plus SDK wrapping with no native normalization or CAS");
                }

            const std::array<std::array<std::uint32_t, 2>, 7> repairPairs{{{0x7fc12345u, Bits(-8.0f)},
                                                                           {Bits(10.0f), 0x7f800000u},
                                                                           {0xff800000u, Bits(10.0f)},
                                                                           {Bits(-8.0f), 0xffc54321u},
                                                                           {0x7fc12345u, 0xff800000u},
                                                                           {0x7f800000u, 0xffc54321u},
                                                                           {Bits((std::numeric_limits<float>::max)()), 0x7fc12345u}}};
            for (std::uint32_t row = 0; row != 3; ++row)
                for (const auto& pair : repairPairs)
                {
                    putPair(row, pair[0], pair[1]);
                    const auto targetH = OutputBits(pair[0]), targetV = OutputBits(pair[1]);
                    prepareExchange(row, pair[0], pair[1], targetH, targetV);
                    auto expected = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
                    std::memcpy(expected.data() + RowRva(row) - P::CameraRva + HorizontalOffset, &targetH, sizeof(targetH));
                    std::memcpy(expected.data() + RowRva(row) - P::CameraRva + VerticalOffset, &targetV, sizeof(targetV));
                    const auto reads = provider.reads, repairs = provider.repairs, calls = ClockCameraDirectionOwnerTest::calls;
                    callback.action = [&](NativeGameTimingAccess&)
                    {
                        rows[row]->GetDirection(horizontal, vertical);
                        Check(Bits(horizontal) == targetH && Bits(vertical) == targetV, "repair ACK publishes the original precomputed SDK outputs");
                    };
                    pulse();
                    const auto& receipt = runtime.cameraDirectionReceipt;
                    Check(provider.reads == reads + 1 && provider.repairs == repairs + 1 && ClockCameraDirectionOwnerTest::calls == calls + 2 &&
                              ClockCameraDirectionOwnerTest::ordered && receipt.getter && receipt.repair && receipt.effectsVerified &&
                              receipt.expected.horizontal == pair[0] && receipt.expected.vertical == pair[1] && receipt.original.horizontal == pair[0] &&
                              receipt.original.vertical == pair[1] && receipt.horizontal.accepted && receipt.vertical.accepted &&
                              expected == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva) && !runtime.cameraDirection.IsRetained(),
                          "either nonfinite input repairs both H+10 then V+0 through actual CAS, preserving V rate+4 and every unrelated camera byte");
                }
            for (unsigned drift = 1; drift != 4; ++drift)
            {
                putPair(0, 0x7fc12345u, Bits(-8.0f));
                provider.driftBeforeRepair = drift;
                const auto calls = ClockCameraDirectionOwnerTest::calls;
                callback.action = [&](NativeGameTimingAccess&)
                {
                    horizontal = 123.25f;
                    vertical = -456.5f;
                    threw = false;
                    try
                    {
                        row0.GetDirection(horizontal, vertical);
                    }
                    catch (const std::exception&)
                    {
                        threw = true;
                    }
                    fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                    Check(threw && horizontal == 123.25f && vertical == -456.5f,
                          "drift between raw getter read and conditional repair preserves both unassigned SDK outputs");
                };
                pulse();
                Check(ClockCameraDirectionOwnerTest::calls == calls && !runtime.cameraDirection.IsRetained() && !runtime.stopping.load() &&
                          (drift == 3 || provider.lastRepair == ClockResult::BindingChanged),
                      "changed expected pair or lost repair owner refuses before either CAS without debt");
            }
            provider.driftBeforeRepair = 0;
            putPair(0, 0x7fc12345u, Bits(-8.0f));
            prepareExchange(0, 0x7fc12345u, Bits(-8.0f), 0, OutputBits(Bits(-8.0f)));
            callback.action = [&](NativeGameTimingAccess&)
            {
                float alias = 99;
                row0.GetDirection(alias, alias);
                Check(Bits(alias) == OutputBits(Bits(-8.0f)), "successful repair still assigns horizontal then vertical when outputs alias");
            };
            pulse();
            std::puts("Production camera direction conditional repair fixture completed");

            for (std::uint32_t row = 0; row != 3; ++row)
                for (const auto inputH : finite)
                {
                    const auto inputV = -8.0f;
                    const auto originalH = Bits(1.0f), originalV = Bits(2.0f);
                    putPair(row, originalH, originalV);
                    const auto targetH = Bits(SourceWrap(inputH)), targetV = Bits(SourceWrap(inputV));
                    prepareExchange(row, originalH, originalV, targetH, targetV);
                    auto expected = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
                    std::memcpy(expected.data() + RowRva(row) - P::CameraRva + HorizontalOffset, &targetH, sizeof(targetH));
                    std::memcpy(expected.data() + RowRva(row) - P::CameraRva + VerticalOffset, &targetV, sizeof(targetV));
                    const auto calls = ClockCameraDirectionOwnerTest::calls;
                    callback.action = [&](NativeGameTimingAccess&) { rows[row]->SetDirection(inputH, inputV); };
                    pulse();
                    const auto& receipt = runtime.cameraDirectionReceipt;
                    Check(ClockCameraDirectionOwnerTest::calls == calls + 2 && ClockCameraDirectionOwnerTest::ordered && receipt.effectsVerified &&
                              !receipt.getter && !receipt.repair && receipt.replacement.horizontal == targetH && receipt.replacement.vertical == targetV &&
                              expected == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva) && !runtime.stopping.load(),
                          "valid actual SetDirection wraps once in SDK then stores only ordered native H+10/V+0, including huge finite source results");
                }
            Check(!runtime.pedReferences.IsRetained() && fixture.Get<std::uintptr_t>(NativePedPoolProfile::RootRva) == 0,
                  "mode/direction create no entity, native target reference or vector lifetime dependency");
            Check(service.End() && !service.IsRetained() && !runtime.cameraDirection.IsRetained() && !runtime.frameHooked.load(),
                  "acknowledged direction pairs retire without restoring any native camera state");
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
            ClockCameraDirectionOwnerTest::image = 0;
            std::puts("Production camera direction ordered writes fixture completed");
            std::puts("Production camera direction actual SDK owner fixture completed");
            return;
        }

        const bool getter = scenario >= 15;
        const auto originalH = getter ? 0x7fc12345u : Bits(10.0f), originalV = Bits(-8.0f);
        const auto targetH = getter ? OutputBits(originalH) : Bits(SourceWrap(3.5f));
        const auto targetV = getter ? OutputBits(originalV) : Bits(SourceWrap(-7.0f));
        putPair(1, originalH, originalV);
        prepareExchange(1, originalH, originalV, targetH, targetV);
        const auto beforeDebt = fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva);
        ClockCameraDirectionOwnerTest::scenario = scenario;
        horizontal = 123.25f;
        vertical = -456.5f;
        threw = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                if (getter)
                    row1.GetDirection(horizontal, vertical);
                else
                    row1.SetDirection(3.5f, -7.0f);
            }
            catch (const std::exception&)
            {
                threw = true;
            }
        };
        pulse();
        const auto receipt = runtime.cameraDirectionReceipt;
        if (scenario == 1)
        {
            Check(threw && receipt.result == ClockResult::Busy && receipt.horizontal.entered && receipt.horizontal.returned && !receipt.horizontal.accepted &&
                      !receipt.vertical.entered && receipt.knownNoEffect && !receipt.debt && !runtime.cameraDirection.IsRetained() &&
                      !runtime.stopping.load() && ClockCameraDirectionOwnerTest::calls == 1 &&
                      fixture.Get<std::uint32_t>(RowRva(1) + HorizontalOffset) == 0x3f000000u &&
                      fixture.Get<std::uint32_t>(RowRva(1) + VerticalOffset) == originalV,
                  "first returned CAS mismatch preserves the competing horizontal field with known no-effect and no retained pair debt");
            ClockCameraDirectionOwnerTest::scenario = 0;
            prepareExchange(1, 0x3f000000u, originalV, targetH, targetV);
            threw = false;
            pulse();
            Check(
                !threw && ClockCameraDirectionOwnerTest::calls == 3 && runtime.cameraDirectionReceipt.effectsVerified && service.End() && !service.IsRetained(),
                "new explicit SDK request can use the same healthy owner after a known rejected first CAS");
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
            std::puts("Production camera direction known rejected first atomic fixture completed");
            return;
        }
        const bool second = scenario == 2 || scenario == 5 || scenario == 6 || (scenario >= 11 && scenario <= 14) || scenario == 16 || scenario == 17;
        Check(threw && receipt.horizontal.entered && receipt.vertical.entered == second && receipt.debt && !receipt.knownNoEffect &&
                  runtime.cameraDirection.RecoveryPending() && runtime.stopping.load() && service.IsRetained() && runtime.frameHooked.load() &&
                  ClockCameraDirectionOwnerTest::calls == (second ? 2u : 1u) && receipt.original.horizontal == originalH &&
                  receipt.original.vertical == originalV && receipt.replacement.horizontal == targetH && receipt.replacement.vertical == targetV &&
                  receipt.getter == getter && receipt.repair == getter,
              "partial or unknown ordered direction retains the full original/replacement pair journal and stopped module owner");
        Check(receipt.image == fixture.image && receipt.root && receipt.world == graph.World() && receipt.logicalRow == 1 &&
                  receipt.horizontal.target == fixture.image + RowRva(1) + HorizontalOffset &&
                  receipt.vertical.target == fixture.image + RowRva(1) + VerticalOffset && receipt.horizontal.before == originalH &&
                  receipt.vertical.before == originalV && receipt.horizontal.replacement == targetH && receipt.vertical.replacement == targetV,
              "retained pair receipt keeps the exact owner graph, logical row, both targets and ordered DWORD request preimages");
        if (scenario == 3 || scenario == 4 || scenario == 15)
            Check(!receipt.horizontal.returned && !receipt.horizontal.accepted && receipt.horizontal.exceptionCode == EXCEPTION_ACCESS_VIOLATION,
                  "unknown horizontal boundary preserves its exception without claiming a native return or acknowledgement");
        if (scenario == 5 || scenario == 6 || scenario == 16)
            Check(!receipt.vertical.returned && !receipt.vertical.accepted && receipt.vertical.exceptionCode == EXCEPTION_ACCESS_VIOLATION,
                  "unknown vertical boundary preserves its exception after the accepted horizontal prefix");
        if (scenario == 2 || scenario == 17)
            Check(receipt.horizontal.accepted && receipt.vertical.returned && !receipt.vertical.accepted,
                  "known second CAS rejection retains the accepted first effect instead of declaring the pair effect-free");
        if (getter)
            Check(horizontal == 123.25f && vertical == -456.5f, "uncertain getter repair never assigns either caller output");
        const bool beforeH = scenario == 3 || scenario == 15;
        const auto expectedH = beforeH ? originalH : scenario == 8 ? 0x3f000000u : targetH;
        const bool afterV = scenario == 6 || (scenario >= 11 && scenario <= 14) || scenario == 16;
        const auto expectedV = scenario == 2 || scenario == 12 || scenario == 17 ? 0x3f000000u : afterV ? targetV : originalV;
        auto       expectedCamera = beforeDebt;
        std::memcpy(expectedCamera.data() + RowRva(1) - P::CameraRva + HorizontalOffset, &expectedH, sizeof(expectedH));
        std::memcpy(expectedCamera.data() + RowRva(1) - P::CameraRva + VerticalOffset, &expectedV, sizeof(expectedV));
        Check(fixture.Get<std::uint32_t>(RowRva(1) + HorizontalOffset) == expectedH && fixture.Get<std::uint32_t>(RowRva(1) + VerticalOffset) == expectedV &&
                  expectedCamera == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva),
              "accepted direction prefixes and later foreign values survive acknowledgement loss without rollback or invented completion");
        const auto calls = ClockCameraDirectionOwnerTest::calls;
        Check(runtime.cameraDirection.Set(fixture.image, 1, originalH, originalV, &CheckTimingOwner, &CheckApplicationTimingOwner) ==
                      ClockResult::RestoreFailure &&
                  runtime.cameraDirection.Repair(fixture.image, 1, originalH, originalV, targetH, targetV, &CheckTimingOwner, &CheckApplicationTimingOwner) ==
                      ClockResult::RestoreFailure,
              "both setter and conditional repair refuse to replay a retained entered pair operation");
        ClockCameraDirectionOwnerTest::scenario = 0;
        Check(!service.End() && service.IsRetained(), "End cannot erase the uncertain direction pair journal");
        fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
        Check(!service.End() && service.IsRetained() && runtime.cameraDirection.RecoveryPending() && ClockCameraDirectionOwnerTest::calls == calls &&
                  expectedCamera == fixture.Get<std::array<unsigned char, P::CameraBytes>>(P::CameraRva),
              "stopped natural frames retain the pair without replay, normalization or cached restoration");
        const char* names[] = {"success",          "cas-rejected",  "second-rejected",  "h-before",        "h-after",        "v-before",
                               "v-after",          "h-owner-loss",  "h-value-mismatch", "h-world-drift",   "h-code-drift",   "v-owner-loss",
                               "v-value-mismatch", "v-world-drift", "v-code-drift",     "repair-h-before", "repair-v-after", "repair-second-rejected"};
        std::printf("Production camera direction %s retained debt fixture completed\n", names[scenario]);
    }
}

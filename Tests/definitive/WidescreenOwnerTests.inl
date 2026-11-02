/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe actual configured-mode CAS and retained neighboring-state ACKs
 ****************************************************************************/
#if !defined(MTA_DE_CAMERA_WIDESCREEN_OWNER_BODY)
namespace ClockCameraWidescreenOwnerTest
{
    using namespace mta::game_de;
    unsigned       scenario = 0, calls = 0;
    std::uintptr_t target = 0;
    std::uint8_t   before = 0, replacement = 0;
    ClockResult    Exchange(std::uintptr_t cell, std::uint8_t expected, std::uint8_t value, std::uint8_t& observed, bool& accepted,
                            std::uint32_t& exception) noexcept
    {
        ++calls;
        target = cell;
        before = expected;
        replacement = value;
        if (expected > 1 || value > 1)
            std::abort();
        if (scenario == 2)
            *reinterpret_cast<std::uint8_t*>(cell) = value;
        if (scenario == 3)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        const auto previous = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(cell), static_cast<char>(value), static_cast<char>(expected));
        observed = static_cast<std::uint8_t>(previous);
        accepted = observed == expected;
        if (scenario == 4)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        // This independent owned writer changes pending-off only AFTER the
        // accepted mode CAS. Production must retain uncertainty, not repair
        // the neighbor or pretend the complete snapshot was acknowledged.
        if (scenario == 5)
            *reinterpret_cast<std::uint8_t*>(cell - 1) ^= 0xff;
        return ClockResult::Ok;
    }
}
#else
namespace CameraWidescreenOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace Q = NativeCameraHistoryProfile;
    namespace R = NativeCameraRowBorrowProfile;
    namespace S = NativeCameraWidescreenProfile;
    namespace Host = ClockCameraWidescreenOwnerTest;
    using CameraBytes = CameraRowBorrowOwnerFixture::CameraBytes;
    using EnginePump = CameraRowBorrowOwnerFixture::EnginePump;
    using CameraTargetOwnerFixture::Refused;
    static_assert(S::ModeOffset == 0x41 && S::NeighborOffset == 0x40 && S::NeighborBytes == 4);
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* value, std::size_t size, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, value, size, protection); };
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
        // All new source roles lie in the fixture's already RX .head/.text
        // sections, not just pages made executable after PE construction.
        for (const auto& pin : R::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : Q::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& group : P::Groups)
        {
            writes.Global(group.slotRva, fixture.image + group.entryRva, PAGE_READONLY);
            writes.Global(group.tableSlotRva, static_cast<std::uint32_t>(group.branchRva), PAGE_EXECUTE_READ);
        }
        writes.Global(P::CrtPointerRva, fixture.image + P::CrtWrapperRva, PAGE_READONLY);
        for (const auto& pin : S::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        writes.Global(S::GroupSlotRva, fixture.image + S::GroupEntryRva, PAGE_READONLY);
        writes.Global(S::SelectorRva, S::Selector, PAGE_EXECUTE_READ);
        writes.Global(S::CaseSlotRva, static_cast<std::uint32_t>(S::CaseRva), PAGE_EXECUTE_READ);
        fixture.Seed();
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        writes.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
        CameraBytes camera;
        camera.fill(0x5a);
        const auto vtable = fixture.image + P::CameraVtableRva;
        std::memcpy(camera.data(), &vtable, sizeof(vtable));
        camera[0x5b] = 255;
        camera[S::NeighborOffset] = 0x7e;
        camera[S::ModeOffset] = 0;
        camera[0x42] = 0x55;
        camera[0x43] = 0xaa;
        apply(P::CameraRva, camera.data(), camera.size(), PAGE_READWRITE);
        const auto oldReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockServiceRequestV1             request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.modulePinned,
              "row borrow preserves public wrong-executable rejection");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && state.hooked && runtime.frameHooked.load(), "row borrow uses the actual public retained application-frame service");
        // Public Begin checked the unpatched entry before attaching its own
        // detour. Validate the live binding here: requiring pre-hook bytes
        // would reject that acknowledged detour, not an unsupported image.
        Check(CheckBinding(), "owned synthetic live binding is independently valid before camera client access");
        if (runtime.cameraRowBorrows.IssuanceClosed())
            Check(runtime.cameraRowBorrows.Open(CameraRowBorrowIdentity()) == ClockResult::Ok,
                  "synthetic fixture opens only after real CheckBinding using actual runtime generation admission image and producer");
        Check(!runtime.cameraRowBorrows.IssuanceClosed() && !runtime.cameraRowBorrows.IsRetained(), "validated generation starts with open empty row slots");
        const auto generation = state.generation;
        const auto producer = runtime.producerThread;
        const auto pulse = [&](bool stopped = false) { engine.Pulse(service, generation, stopped); };
        const auto finish = [&](const CameraBytes* expected = nullptr)
        {
            if (!service.End())
            {
                pulse(true);
                Check(service.End(), "producer detaches after the actual engine acknowledges final restoration");
            }
            Check(!service.IsRetained() && !runtime.cameraRowBorrows.IsRetained() && !runtime.frameHooked.load(),
                  "drained row clients permit complete hook and generation retirement");
            if (expected)
                Check(*expected == fixture.Get<CameraBytes>(P::CameraRva), "successful End does not roll back native camera vector writes");
            engine.Stop();
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
        };

        Host::scenario = 0;
        Host::calls = 0;
        auto                     registryStorage = std::make_unique<pool_registry_fixture::Pools>(runtime.access);
        auto&                    registry = *registryStorage;
        CameraWidescreenObserver widescreen(registry, runtime.access);
        CCam*                    product = nullptr;
        const auto               originalCamera = fixture.Get<CameraBytes>(P::CameraRva);
        const auto               modeCell = P::CameraRva + S::ModeOffset;
        const auto               pendingCell = P::CameraRva + S::NeighborOffset;
        bool                     output = true;
        Check(runtime.access.ReadCameraWidescreen(output) != ClockResult::Ok && output && Refused([&] { widescreen.GetWidescreen(); }) &&
                  Refused([&] { widescreen.SetWidescreen(true); }) && !Host::calls,
              "actual widescreen SDK and raw observer require a genuine current callback and preserve refused bool output");
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(GetCurrentThreadId() == engine.thread.load() && runtime.producerThread == producer && producer != GetCurrentThreadId(),
                  "configured widescreen uses the genuine application callback and immutable Begin/End producer");
            product = widescreen.GetCam(0);
            Check(product && widescreen.GetCam(0) == product, "new widescreen facade reuses the same stable actual GetCam product and explicit drain contract");
            bool native = true;
            Check(!widescreen.GetWidescreen() && runtime.access.ReadCameraWidescreen(native) == ClockResult::Ok && !native &&
                      fixture.Get<std::uint8_t>(pendingCell) == 0x7e,
                  "actual getter observes configured byte41 as false independently of a nonzero pending-off byte40");
        };
        pulse();
        const auto cameraExpected = [&](std::uint8_t mode, std::uint8_t pending = 0x7e)
        {
            auto expected = originalCamera;
            expected[S::ModeOffset] = mode;
            expected[S::NeighborOffset] = pending;
            return expected;
        };
        const auto orderlyFinish = [&](const CameraBytes& expected)
        {
            widescreen.CloseCamPublication();
            Check(!service.End() && runtime.cameraRowBorrows.IsRetained(),
                  "first End stops issuance while inherited typed Cam product still retains the native module");
            pulse(true);
            Check(widescreen.DrainCamsOnProducer() == ClockResult::Ok && !runtime.cameraRowBorrows.IsRetained(),
                  "same producer explicitly drains inherited Cam product after close quiescence and native Stop");
            finish(&expected);
        };
        if (scenario == 0)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                widescreen.SetWidescreen(true);
                Check(Host::calls == 1 && widescreen.GetWidescreen() && runtime.cameraWidescreenReceipt.effectsVerified &&
                          fixture.Get<CameraBytes>(P::CameraRva) == cameraExpected(1),
                      "actual setter changes only configured bool41 and preserves pending-off40 plus every other native camera byte");
                fixture.Put(modeCell, std::uint8_t{0});
                Check(!widescreen.GetWidescreen(), "actual getter reads a later native mode change rather than caching last setter intent or visible bars");
                widescreen.SetWidescreen(true);
                widescreen.SetWidescreen(false);
                widescreen.SetWidescreen(true);
                widescreen.SetWidescreen(true);
                const auto receipt = runtime.cameraWidescreenReceipt;
                Check(
                    Host::calls == 5 && receipt.entered && receipt.returned && receipt.accepted && receipt.effectsVerified && !receipt.debt &&
                        receipt.before == 1 && receipt.replacement == 1 && receipt.after == 1 && widescreen.GetWidescreen() &&
                        fixture.Get<CameraBytes>(P::CameraRva) == cameraExpected(1) && !runtime.cameraWidescreen.IsRetained(),
                    "each explicit canonical bool request has one verified expected-old byte CAS including a same-value request without SCM pending behavior");
            };
            pulse();
            orderlyFinish(cameraExpected(1));
            std::puts("Production camera widescreen actual SDK owner fixture completed");
            std::puts("Production camera widescreen configured mode fixture completed");
            std::puts("Production camera widescreen pending preservation fixture completed");
            return;
        }
        if (scenario == 1)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto refusal = [&](const char* why)
                {
                    const auto beforeCamera = fixture.Get<CameraBytes>(P::CameraRva);
                    bool       native = true;
                    const auto calls = Host::calls;
                    Check(runtime.access.ReadCameraWidescreen(native) != ClockResult::Ok && native && Refused([&] { widescreen.GetWidescreen(); }) &&
                              Refused([&] { widescreen.SetWidescreen(true); }) && Host::calls == calls && !runtime.cameraWidescreen.RecoveryPending() &&
                              !runtime.stopping.load() && beforeCamera == fixture.Get<CameraBytes>(P::CameraRva),
                          why);
                };
                const auto frame = frameCallbackSerial;
                --frameCallbackSerial;
                refusal("stale application frame refuses mode publication and writes without changing output or camera bytes");
                frameCallbackSerial = frame;
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                refusal("GC excludes configured mode observation and mutation");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
                refusal("configured mode requires current playing state");
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
                const auto vtable = fixture.Get<std::uintptr_t>(P::CameraRva);
                fixture.Put(P::CameraRva, std::uintptr_t{0});
                refusal("constructed native camera storage is required before bool access");
                fixture.Put(P::CameraRva, vtable);
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{123});
                refusal("mode access refuses stale typed-world membership");
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{3});
                const auto original = S::Pins[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(S::Pins[0].rva, &changed, 1);
                refusal("exact separately qualified widescreen command source remains mandatory");
                Check(product->GetFront() == reinterpret_cast<CVector*>(fixture.image + CameraRowBorrowOwnerFixture::RowRva(0) + 0x110),
                      "widescreen source qualification does not strengthen the existing actual row vector methods");
                fixture.Patch(S::Pins[0].rva, &original, 1);
                fixture.Put(modeCell, std::uint8_t{2});
                refusal("noncanonical native mode bool is refused rather than clamped read as true or overwritten");
                fixture.Put(modeCell, std::uint8_t{0});
            };
            pulse();
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the existing shared native gate before mode access");
            callback.action = [&](NativeGameTimingAccess&)
            {
                bool native = true;
                Check(runtime.access.ReadCameraWidescreen(native) == ClockResult::Busy && native && Refused([&] { widescreen.GetWidescreen(); }) &&
                          Refused([&] { widescreen.SetWidescreen(true); }) && !Host::calls,
                      "shared gate contention refuses both actual mode methods without effects");
            };
            pulse();
            NativeStreamingCompletionContext::Leave();
            callback.action = [&](NativeGameTimingAccess&)
            {
                Stop(ClockResult::Stopped);
                bool native = true;
                Check(runtime.access.ReadCameraWidescreen(native) != ClockResult::Ok && native && Refused([&] { widescreen.GetWidescreen(); }) &&
                          Refused([&] { widescreen.SetWidescreen(true); }) && !Host::calls,
                      "Stop closes mode reads and writes without inventing a configured false output");
            };
            pulse();
            orderlyFinish(cameraExpected(0));
            std::puts("Production camera widescreen refusal fixture completed");
            return;
        }
        Host::scenario = scenario;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(Refused([&] { widescreen.SetWidescreen(true); }),
                  "the actual configured mode setter propagates rejected or uncertain native acknowledgement");
            const auto receipt = runtime.cameraWidescreenReceipt;
            Check(Host::calls == 1 && receipt.entered && receipt.before == 0 && receipt.replacement == 1 && receipt.target == fixture.image + modeCell,
                  "one explicit setter request enters only one byte CAS on configured41 without whole-command substitution");
            if (scenario == 2)
            {
                Check(receipt.returned && !receipt.accepted && receipt.knownNoEffect && !receipt.debt && !runtime.stopping.load() &&
                          !runtime.cameraWidescreen.IsRetained() && fixture.Get<CameraBytes>(P::CameraRva) == cameraExpected(1),
                      "known first CAS mismatch preserves the competing mode with no own effect debt no automatic retry and no pending-off change");
                Host::scenario = 0;
                widescreen.SetWidescreen(false);
                Check(Host::calls == 2 && !widescreen.GetWidescreen() && runtime.cameraWidescreenReceipt.effectsVerified &&
                          !runtime.cameraWidescreen.IsRetained(),
                      "a new explicit request can use the healthy owner after a known rejected comparison");
            }
        };
        pulse();
        if (scenario == 2)
        {
            orderlyFinish(cameraExpected(0));
            std::puts("Production camera widescreen known rejected CAS fixture completed");
            return;
        }
        const auto         receipt = runtime.cameraWidescreenReceipt;
        const std::uint8_t expectedMode = scenario == 3 ? 0 : 1, expectedPending = scenario == 5 ? 0x81 : 0x7e;
        const auto         expected = cameraExpected(expectedMode, expectedPending);
        Check(receipt.debt && !receipt.effectsVerified && !receipt.knownNoEffect && runtime.cameraWidescreen.RecoveryPending() && runtime.stopping.load() &&
                  service.IsRetained() && runtime.frameHooked.load() && fixture.Get<CameraBytes>(P::CameraRva) == expected,
              "accepted or unknown mode completion retains its first native journal and exact visible camera/pending prefix without rollback");
        Check(receipt.returned == (scenario == 5) && receipt.accepted == (scenario != 3) &&
                  (scenario == 5 || receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION),
              "mode journal distinguishes unknown before and after CAS from returned accepted but unverified neighbors");
        widescreen.CloseCamPublication();
        Check(!service.End() && runtime.cameraRowBorrows.IsRetained(), "mode uncertainty and inherited Cam hold independently prevent native retirement");
        pulse(true);
        Check(widescreen.DrainCamsOnProducer() == ClockResult::Ok && !runtime.cameraRowBorrows.IsRetained() && !service.End() &&
                  runtime.cameraWidescreen.RecoveryPending(),
              "producer drains typed row products without consuming the independent unresolved widescreen effect journal");
        pulse(true);
        const auto calls = Host::calls;
        Check(runtime.cameraWidescreen.Write(fixture.image, false, &CheckTimingOwner, &CheckApplicationTimingOwner) == ClockResult::RestoreFailure &&
                  runtime.cameraWidescreenReceipt.target == receipt.target && runtime.cameraWidescreenReceipt.before == receipt.before &&
                  runtime.cameraWidescreenReceipt.replacement == receipt.replacement && Host::calls == calls &&
                  fixture.Get<CameraBytes>(P::CameraRva) == expected && !service.End(),
              "new requests stopped frames and repeated End preserve the first mode journal and do not replay restore configured mode or repair pending-off");
        engine.Stop();
        const char* names[] = {"success", "refusals", "cas-rejected", "write-before", "write-after", "accepted-unverified"};
        std::printf("Production camera widescreen %s retained debt fixture completed\n", names[scenario]);
        // Preserve the unresolved native bytes until this fresh process exits;
        // restoring fixture state here would manufacture an acknowledgement.
    }
}
#endif

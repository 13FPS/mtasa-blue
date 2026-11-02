/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual held-row target-history arrays preserve their complete storage
 ****************************************************************************/
namespace CameraHistoryOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace Q = NativeCameraHistoryProfile;
    namespace R = NativeCameraRowBorrowProfile;
    using Token = NativeCameraRowBorrow::Token;
    using CameraBytes = CameraRowBorrowOwnerFixture::CameraBytes;
    using EnginePump = CameraRowBorrowOwnerFixture::EnginePump;
    using CameraRowBorrowOwnerFixture::RowRva;
    constexpr std::size_t HistoryOffset = Q::HistoryOffset, HistoryBytes = Q::ArrayBytes, MetadataOffset = 0x188;
    static_assert(sizeof(CVector) == 12 && Q::VectorCount == 4 && HistoryBytes == 4 * sizeof(CVector) && HistoryOffset + HistoryBytes == MetadataOffset);
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
        const auto generation = state.generation, serial = runtime.admissionSerial.load();
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

        std::array<std::unique_ptr<CamRowObserver>, 3> rows;
        Token                                          raw;
        std::array<std::array<std::uint32_t, 5>, 3>    metadata{};
        for (std::uint32_t row = 0; row != 3; ++row)
        {
            // Deliberately unusual timestamps/count are not a pointer-getter
            // policy. The constructed four-vector extent remains available.
            metadata[row] = {0xdead0000u + row, 0xaabbcc00u + row, 0x11223300u + row, 0x87654300u + row, 0xf00d0000u + row};
            fixture.Put(RowRva(row) + MetadataOffset, metadata[row]);
        }
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(GetCurrentThreadId() == engine.thread.load() && runtime.producerThread == producer && GetCurrentThreadId() != producer,
                  "history acquires the existing row hold on the genuine engine callback without changing producer identity");
            const auto count = scenario ? 1u : 3u;
            for (std::uint32_t row = 0; row != count; ++row)
                rows[row] = std::make_unique<CamRowObserver>(row, runtime.access);
            if (scenario)
                Check(access.AcquireCameraRowBorrow(1, raw) == ClockResult::Ok, "history refusal fixture uses the existing raw row token channel");
        };
        pulse();
        Check(runtime.cameraRowBorrows.IsRetained(), "history row wrappers use the existing native lifetime retention");
        if (!scenario)
        {
            auto expected = fixture.Get<CameraBytes>(P::CameraRva);
            for (std::uint32_t row = 0; row != 3; ++row)
            {
                callback.action = [&, row](NativeGameTimingAccess&)
                {
                    CVector* pointer = rows[row]->GetTargetHistoryPos();
                    Check(pointer == reinterpret_cast<CVector*>(fixture.image + RowRva(row) + HistoryOffset),
                          "actual SDK history returns the beginning of this held native row's four-vector array");
                    for (std::uint32_t index = 0; index != 4; ++index)
                    {
                        const std::array<std::uint32_t, 3> bits{0x3f800000u + row * 16 + index, 0xc0000000u + index, index == 3 ? 0x7fc54321u : 0x80000000u};
                        pointer[index].fX = CameraDirectionOwnerFixture::FromBits(bits[0]);
                        pointer[index].fY = CameraDirectionOwnerFixture::FromBits(bits[1]);
                        pointer[index].fZ = CameraDirectionOwnerFixture::FromBits(bits[2]);
                        std::memcpy(expected.data() + RowRva(row) - P::CameraRva + HistoryOffset + index * sizeof(CVector), bits.data(), sizeof(bits));
                        Check(reinterpret_cast<std::uintptr_t>(&pointer[index]) == fixture.image + RowRva(row) + HistoryOffset + index * 12,
                              "all four constructed target-history vectors have their full twelve-byte native stride");
                    }
                    Check(rows[row]->GetTargetHistoryPos() == pointer,
                          "history pointer identity survives direct writes across the full forty-eight-byte extent");
                    Check(rows[row]->GetSource() == reinterpret_cast<CVector*>(fixture.image + RowRva(row) + 0x11c),
                          "history shares the same held row as the existing source getter");
                };
                pulse();
                Check(expected == fixture.Get<CameraBytes>(P::CameraRva),
                      "direct history writes affect only its forty-eight native bytes and preserve every neighboring field");
                for (std::uint32_t other = 0; other != 3; ++other)
                    Check(fixture.Get<std::array<std::uint32_t, 5>>(RowRva(other) + MetadataOffset) == metadata[other],
                          "history getter and indexed writes preserve all timestamps and counts in every row");
            }
            Check(!service.End() && runtime.cameraRowBorrows.IsRetained() && runtime.generation.load() == generation &&
                      runtime.admissionSerial.load() == serial && runtime.frameHooked.load(),
                  "history pointer clients retain the exact generation and hooks after End requests Stop");
            pulse(true);
            Check(!service.End() && expected == fixture.Get<CameraBytes>(P::CameraRva),
                  "stopped natural frames neither clear history holds nor restore caller history writes");
            for (auto& row : rows)
                row.reset();
            Check(!runtime.cameraRowBorrows.IsRetained(), "typed history wrapper destruction completes on the producer before final retirement");
            finish(&expected);
            std::puts("Production camera history actual SDK owner fixture completed");
            std::puts("Production camera history pointer extent fixture completed");
            std::puts("Production camera history preservation fixture completed");
            std::puts("Production camera history producer drain fixture completed");
            return;
        }
        callback.action = [&](NativeGameTimingAccess& access)
        {
            const auto refusal = [&](const char* why)
            {
                const auto before = fixture.Get<CameraBytes>(P::CameraRva);
                CVector*   output = reinterpret_cast<CVector*>(std::uintptr_t{0x12345678});
                bool       threw = false;
                try
                {
                    rows[0]->GetTargetHistoryPos();
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                const auto result = access.ReadCameraRowVector(raw, CameraRowVectorKind::TargetHistory, output);
                Check(threw && result != ClockResult::Ok && output == reinterpret_cast<CVector*>(std::uintptr_t{0x12345678}) &&
                          runtime.cameraRowBorrows.IsRetained() && before == fixture.Get<CameraBytes>(P::CameraRva),
                      why);
            };
            const auto frame = frameCallbackSerial;
            --frameCallbackSerial;
            refusal("history refuses stale frame authority without publishing an array");
            frameCallbackSerial = frame;
            fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
            refusal("history array publication is excluded during GC");
            fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
            fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
            refusal("history requires current playing9 state");
            fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
            const auto type = fixture.Get<std::uintptr_t>(P::CameraRva);
            fixture.Put(P::CameraRva, std::uintptr_t{0});
            refusal("history requires constructed camera storage independently of image readability");
            fixture.Put(P::CameraRva, type);
            NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{123});
            refusal("history rejects stale typed-world membership");
            NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{3});
            const auto original = Q::Pins[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
            fixture.Patch(Q::Pins[0].rva, &changed, 1);
            refusal("independently qualified history producer code is required before any target-history pointer escapes");
            Check(rows[0]->GetFront() == reinterpret_cast<CVector*>(fixture.image + RowRva(0) + 0x110),
                  "the separate history profile does not strengthen the five existing row-vector getter paths");
            fixture.Patch(Q::Pins[0].rva, &original, 1);
        };
        pulse();
        Check(NativeStreamingCompletionContext::TryEnter(), "hold the shared native gate independently before history publication");
        bool gateThrew = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                rows[0]->GetTargetHistoryPos();
            }
            catch (const std::exception&)
            {
                gateThrew = true;
            }
        };
        pulse();
        NativeStreamingCompletionContext::Leave();
        Check(gateThrew && !runtime.stopping.load(), "history gate refusal leaves the existing client hold healthy");
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Stop(ClockResult::Stopped);
            bool threw = false;
            try
            {
                rows[0]->GetTargetHistoryPos();
            }
            catch (const std::exception&)
            {
                threw = true;
            }
            CVector* output = reinterpret_cast<CVector*>(std::uintptr_t{0x12345678});
            Check(threw && access.ReadCameraRowVector(raw, CameraRowVectorKind::TargetHistory, output) != ClockResult::Ok &&
                      output == reinterpret_cast<CVector*>(std::uintptr_t{0x12345678}) && runtime.cameraRowBorrows.IsRetained(),
                  "Stop closes history getter publication without forcing either existing row hold to disappear");
        };
        pulse();
        const auto expected = fixture.Get<CameraBytes>(P::CameraRva);
        Check(!service.End() && service.IsRetained(), "stopped history clients retain the actual service until producer drain");
        for (std::uint32_t row = 0; row != 3; ++row)
            Check(fixture.Get<std::array<std::uint32_t, 5>>(RowRva(row) + MetadataOffset) == metadata[row],
                  "history refusals and Stop preserve timestamp/count metadata exactly");
        rows[0].reset();
        Check(runtime.cameraRowBorrows.IsRetained(), "the second existing raw row hold still retains the history owner's module");
        Check(runtime.access.ReleaseCameraRowBorrow(raw) == ClockResult::Ok && !raw && !runtime.cameraRowBorrows.IsRetained(),
              "history uses the same producer-only CPU release after Stop with no new token class or cleanup policy");
        finish(&expected);
        std::puts("Production camera history refusal fixture completed");
        std::puts("Production camera history preservation fixture completed");
        std::puts("Production camera history producer drain fixture completed");
    }
}

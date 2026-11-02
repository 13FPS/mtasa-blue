/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real SDK vector clients retain the native owner through producer drain
 ****************************************************************************/
namespace CameraRowBorrowOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace Q = NativeCameraRowBorrowProfile;
    using Token = NativeCameraRowBorrow::Token;
    constexpr std::array<std::size_t, 5>         Offsets{0x110, 0x134, 0x11c, 0xe0, 0xd4};
    constexpr std::array<CameraRowVectorKind, 5> Kinds{CameraRowVectorKind::Front, CameraRowVectorKind::Up, CameraRowVectorKind::Source,
                                                       CameraRowVectorKind::FixedModeSource, CameraRowVectorKind::FixedModeVector};
    using Getter = CVector* (CCam::*)() const;
    constexpr std::array<Getter, 5> Getters{&CCam::GetFront, &CCam::GetUp, &CCam::GetSource, &CCam::GetFixedModeSource, &CCam::GetFixedModeVector};
    std::uintptr_t                  RowRva(std::uint32_t row)
    {
        return P::CameraRva + P::ArrayOffset + row * P::RowBytes;
    }
    using CameraBytes = std::array<unsigned char, P::CameraBytes>;

    // The producer remains the actual Begin/End thread. A distinct engine
    // thread executes the genuine frame hook, while public Renew keeps long
    // qualification callbacks alive without changing runtime owner identity.
    struct EnginePump
    {
        ClockImageFixture& fixture;
        HANDLE             run = CreateEventW(nullptr, FALSE, FALSE, nullptr), done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        std::atomic<bool>  stopping{false};
        std::atomic<DWORD> thread{0};
        std::thread        worker;
        explicit EnginePump(ClockImageFixture& image)
            : fixture(image),
              worker(
                  [this]
                  {
                      thread.store(GetCurrentThreadId());
                      while (WaitForSingleObject(run, 5000) == WAIT_OBJECT_0)
                      {
                          if (stopping.load())
                              break;
                          fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
                          SetEvent(done);
                      }
                  })
        {
            Check(run && done, "create bounded events for the real row-borrow engine handoff");
            const auto start = GetTickCount64();
            while (!thread.load() && GetTickCount64() - start < 5000)
                Sleep(1);
            Check(thread.load() && thread.load() != GetCurrentThreadId(), "row callback engine is distinct from the Begin/End producer");
        }
        void Pulse(NativeClockService& service, std::uint64_t generation, bool stopped = false)
        {
            if (!stopped)
                Check(service.Renew(generation), "renew the unchanged row-borrow producer before handing off a frame");
            Check(SetEvent(run) != 0, "hand one genuine application frame to the row-borrow engine");
            const auto start = GetTickCount64();
            DWORD      result = WAIT_TIMEOUT;
            while ((result = WaitForSingleObject(done, 10)) == WAIT_TIMEOUT && GetTickCount64() - start < 50000)
                if (!runtime.stopping.load())
                    Check(service.Renew(generation), "public producer renewal remains available while a different engine thread qualifies vectors");
            Check(result == WAIT_OBJECT_0, "row-borrow engine frame returns within its bounded handoff");
        }
        void Stop() noexcept
        {
            stopping.store(true);
            SetEvent(run);
            if (worker.joinable())
                worker.join();
        }
        ~EnginePump()
        {
            Stop();
            if (run)
                CloseHandle(run);
            if (done)
                CloseHandle(done);
        }
    };

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
        if (scenario < 2)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(GetCurrentThreadId() == engine.thread.load() && runtime.producerThread == producer && producer != GetCurrentThreadId(),
                      "real engine callback acquires rows without impersonating the immutable producer");
                const auto count = scenario ? 1u : 3u;
                for (std::uint32_t row = 0; row != count; ++row)
                    rows[row] = std::make_unique<CamRowObserver>(row, runtime.access);
            };
            pulse();
            Check(runtime.cameraRowBorrows.IsRetained() && runtime.producerThread == producer && State(service).engineThread == engine.thread.load(),
                  "actual partial SDK row clients retain the unchanged service generation across the thread handoff");
        }
        if (scenario == 0)
        {
            auto expected = fixture.Get<CameraBytes>(P::CameraRva);
            for (std::uint32_t row = 0; row != 3; ++row)
                for (std::size_t kind = 0; kind != Kinds.size(); ++kind)
                {
                    const std::array<std::uint32_t, 3> contents{0x7fc12345u, 0xff800000u, 0x80000000u};
                    fixture.Put(RowRva(row) + Offsets[kind], contents);
                    std::memcpy(expected.data() + RowRva(row) - P::CameraRva + Offsets[kind], contents.data(), sizeof(contents));
                    callback.action = [&, row, kind](NativeGameTimingAccess&)
                    {
                        auto* pointer = (rows[row].get()->*Getters[kind])();
                        Check(pointer == reinterpret_cast<CVector*>(fixture.image + RowRva(row) + Offsets[kind]),
                              "actual SDK getter returns the exact current native vector subobject for this immutable row");
                        std::array<std::uint32_t, 3> observed{};
                        std::memcpy(observed.data(), pointer, sizeof(observed));
                        Check(observed == contents, "native vector publication preserves NaN infinity and signed-zero contents without a finite clamp");
                        const std::array<std::uint32_t, 3> replacement{0x3f800000u + row, 0xc0000000u + static_cast<std::uint32_t>(kind), 0x7fc54321u};
                        pointer->fX = CameraDirectionOwnerFixture::FromBits(replacement[0]);
                        pointer->fY = CameraDirectionOwnerFixture::FromBits(replacement[1]);
                        pointer->fZ = CameraDirectionOwnerFixture::FromBits(replacement[2]);
                        std::memcpy(expected.data() + RowRva(row) - P::CameraRva + Offsets[kind], replacement.data(), sizeof(replacement));
                        Check((rows[row].get()->*Getters[kind])() == pointer, "held row pointer identity survives direct caller writes");
                    };
                    pulse();
                    Check(expected == fixture.Get<CameraBytes>(P::CameraRva),
                          "direct CVector writes change only their actual twelve native bytes and never a cached copy");
                }
            callback.action = [&](NativeGameTimingAccess& access)
            {
                Token duplicate, invalid;
                Check(access.AcquireCameraRowBorrow(0, duplicate) != ClockResult::Ok && !duplicate &&
                          access.AcquireCameraRowBorrow(0xffffffffu, invalid) == ClockResult::InvalidArgument && !invalid,
                      "occupied fixed slots and full invalid row indices cannot issue extra camera clients");
                bool threw = false;
                try
                {
                    CamRowObserver duplicateSdk(0, runtime.access);
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                Check(threw && runtime.cameraRowBorrows.IsRetained(), "duplicate actual SDK constructor refuses without consuming another row client");
            };
            pulse();
            const auto context = runtime.timingContext;
            Check(!service.End() && service.IsRetained() && runtime.generation.load() == generation && runtime.admissionSerial.load() == serial &&
                      runtime.timingContext == context && runtime.frameHooked.load() && runtime.cameraRowBorrows.IsRetained(),
                  "End keeps the same generation context admission hooks and module while actual SDK row wrappers are alive");
            NativeClockService  replacement;
            ClockServiceStateV1 refused;
            Check(!replacement.BeginGameTiming(request, refused, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                      runtime.generation.load() == generation,
                  "Begin cannot replace a generation whose stopped camera clients have not drained");
            pulse(true);
            Check(!service.End() && expected == fixture.Get<CameraBytes>(P::CameraRva),
                  "natural stopped frames neither force camera drops nor restore caller vector writes");
            rows[0].reset();
            rows[1].reset();
            Check(runtime.cameraRowBorrows.IsRetained(), "one remaining SDK row still retains the owner");
            rows[2].reset();
            Check(!runtime.cameraRowBorrows.IsRetained(), "same-producer typed destruction returns before the last camera hold disappears");
            finish(&expected);
            std::puts("Production camera row borrow pointer identity fixture completed");
            std::puts("Production camera row borrow lifetime fixture completed");
            std::puts("Production camera row borrow actual SDK owner fixture completed");
            return;
        }
        if (scenario == 1)
        {
            callback.action = [&](NativeGameTimingAccess& access)
            {
                const auto refusal = [&](const char* why)
                {
                    const auto before = fixture.Get<CameraBytes>(P::CameraRva);
                    Token      fresh;
                    bool       threw = false;
                    try
                    {
                        rows[0]->GetFront();
                    }
                    catch (const std::exception&)
                    {
                        threw = true;
                    }
                    CVector*   output = reinterpret_cast<CVector*>(std::uintptr_t{0x12345678});
                    Token      empty;
                    const auto result = access.ReadCameraRowVector(empty, CameraRowVectorKind::Front, output);
                    Check(threw && result != ClockResult::Ok && output == reinterpret_cast<CVector*>(std::uintptr_t{0x12345678}) &&
                              access.AcquireCameraRowBorrow(1, fresh) != ClockResult::Ok && !fresh && runtime.cameraRowBorrows.IsRetained() &&
                              before == fixture.Get<CameraBytes>(P::CameraRva),
                          why);
                };
                const auto frame = frameCallbackSerial;
                --frameCallbackSerial;
                refusal("stale frame serial grants neither pointer publication nor a new hold");
                frameCallbackSerial = frame;
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                refusal("GC excludes vector publication before a pointer can escape");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
                refusal("playing9 is independently required for native vector publication");
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
                fixture.Put(P::CameraRva, std::uintptr_t{0});
                refusal("constructed camera type is required beyond readable writable storage");
                fixture.Put(P::CameraRva, vtable);
                const auto original = Q::Pins[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(Q::Pins[0].rva, &changed, 1);
                refusal("exact field-role source code remains required for every native pointer publication");
                fixture.Patch(Q::Pins[0].rva, &original, 1);
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{123});
                refusal("stale typed-world membership cannot authorize a native row pointer");
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{3});
            };
            pulse();
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the real shared native gate before row access");
            bool gateThrew = false;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    rows[0]->GetUp();
                }
                catch (const std::exception&)
                {
                    gateThrew = true;
                }
            };
            pulse();
            NativeStreamingCompletionContext::Leave();
            Check(gateThrew && !runtime.stopping.load(), "shared-gate refusal publishes no pointer and does not stop a healthy row owner");
            callback.action = [&](NativeGameTimingAccess&)
            {
                Stop(ClockResult::Stopped);
                bool getterThrew = false, constructorThrew = false;
                try
                {
                    rows[0]->GetSource();
                }
                catch (const std::exception&)
                {
                    getterThrew = true;
                }
                try
                {
                    CamRowObserver closed(1, runtime.access);
                }
                catch (const std::exception&)
                {
                    constructorThrew = true;
                }
                Check(getterThrew && constructorThrew && runtime.cameraRowBorrows.IssuanceClosed() && runtime.cameraRowBorrows.IsRetained(),
                      "Stop revokes getters and new SDK clients even inside the otherwise current engine callback");
            };
            pulse();
            Check(!service.End() && service.IsRetained(), "a stopped real SDK row holds retirement until producer drain");
            const auto original = Q::Pins[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
            fixture.Patch(Q::Pins[0].rva, &changed, 1);
            fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
            const auto beforeDrop = fixture.Get<CameraBytes>(P::CameraRva);
            rows[0].reset();
            Check(!runtime.cameraRowBorrows.IsRetained() && beforeDrop == fixture.Get<CameraBytes>(P::CameraRva),
                  "producer destruction after Stop is CPU-only despite unavailable native source and GC authority");
            fixture.Patch(Q::Pins[0].rva, &original, 1);
            fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
            finish();
            std::puts("Production camera row borrow refusals fixture completed");
            std::puts("Production camera row borrow producer drain fixture completed");
            return;
        }
        Token issued;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(access.AcquireCameraRowBorrow(1, issued) == ClockResult::Ok && issued && issued.Row() == 1,
                  "actual engine callback acquires a private move-only token for a logical row");
            Check(issued.Owner().producerThread == producer && GetCurrentThreadId() != producer,
                  "token records the real producer rather than its engine acquisition thread");
        };
        pulse();
        const auto firstId = issued.Id();
        Token      moved(std::move(issued));
        Check(!issued && moved && runtime.access.ReleaseCameraRowBorrow(issued) == ClockResult::InvalidArgument && runtime.cameraRowBorrows.IsRetained(),
              "moving a token transfers one hold and its empty predecessor cannot drop it");
        ClockResult foreignResult = ClockResult::Ok;
        std::thread foreign([&] { foreignResult = runtime.access.ReleaseCameraRowBorrow(moved); });
        foreign.join();
        Check(foreignResult == ClockResult::WrongThread && moved.Id() == firstId && runtime.cameraRowBorrows.IsRetained(),
              "foreign release returns without consuming token or hold and cannot race producer unload");
        NativeCameraRowBorrow other;
        Token                 unrelated;
        const auto            identity = CameraRowBorrowIdentity();
        Check(other.Open(identity) == ClockResult::Ok, "independent CPU ledger uses the actual binding only for adversarial token identity checking");
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(other.Acquire(identity, 1, &CheckApplicationTimingOwner, unrelated) == ClockResult::Ok && unrelated.Id() != firstId,
                  "different ledgers never recycle a token identity");
            CVector* pointer = reinterpret_cast<CVector*>(std::uintptr_t{0x12345678});
            Check(access.ReadCameraRowVector(unrelated, CameraRowVectorKind::Front, pointer) != ClockResult::Ok &&
                      pointer == reinterpret_cast<CVector*>(std::uintptr_t{0x12345678}),
                  "matching row and owner facts cannot adopt another ledger's token");
            pointer = reinterpret_cast<CVector*>(std::uintptr_t{0x12345678});
            Check(access.ReadCameraRowVector(moved, static_cast<CameraRowVectorKind>(0xffffffffu), pointer) != ClockResult::Ok &&
                      pointer == reinterpret_cast<CVector*>(std::uintptr_t{0x12345678}),
                  "invalid full vector-kind values preserve caller output");
        };
        pulse();
        Check(runtime.access.ReleaseCameraRowBorrow(unrelated) != ClockResult::Ok && unrelated && moved && runtime.cameraRowBorrows.IsRetained(),
              "an unrelated token cannot consume the native row slot");
        Check(other.Release(unrelated) == ClockResult::Ok && !unrelated, "independent token returns only to its own CPU ledger");
        Check(!service.End() && runtime.cameraRowBorrows.IsRetained() && runtime.frameHooked.load(), "stopped raw row hold still blocks hook retirement");
        Check(runtime.access.ReleaseCameraRowBorrow(moved) == ClockResult::Ok && !moved && !runtime.cameraRowBorrows.IsRetained(),
              "joined foreign attempt permits the actual producer's later explicit CPU drop after Stop");
        Check(runtime.cameraRowBorrows.Open(identity) == ClockResult::OwnerConflict, "a stopped drained generation cannot reopen camera issuance");
        if (!service.End())
        {
            pulse(true);
            Check(service.End(), "old token generation completes real engine cleanup before replacement");
        }
        ClockServiceStateV1 nextState;
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool restarted = service.BeginGameTiming(request, nextState, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(restarted && nextState.generation > generation && CheckBinding(), "a fully drained native owner can admit a later genuine generation");
        if (runtime.cameraRowBorrows.IssuanceClosed())
            Check(runtime.cameraRowBorrows.Open(CameraRowBorrowIdentity()) == ClockResult::Ok,
                  "later synthetic generation opens only after its real binding check");
        Token next;
        callback.action = [&](NativeGameTimingAccess& access)
        { Check(access.AcquireCameraRowBorrow(1, next) == ClockResult::Ok, "later real engine callback acquires its own row identity"); };
        engine.Pulse(service, nextState.generation);
        Check(next.Id() > firstId && next.Owner().generation > identity.generation && next.Owner().admissionSerial > identity.admissionSerial &&
                  runtime.access.ReleaseCameraRowBorrow(issued) == ClockResult::InvalidArgument && runtime.cameraRowBorrows.IsRetained(),
              "token identities never recycle and an empty predecessor cannot drop a newer generation");
        Check(!service.End() && runtime.access.ReleaseCameraRowBorrow(next) == ClockResult::Ok && !next,
              "later generation still requires producer-owned Stop and explicit row drain");
        finish();
        std::puts("Production camera row borrow token identity fixture completed");
        std::puts("Production camera row borrow producer drain fixture completed");
    }
}

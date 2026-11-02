/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Publish stable actual GetCam rows and drain typed products on producer
 ****************************************************************************/
namespace CameraGetCamOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace Q = NativeCameraHistoryProfile;
    namespace R = NativeCameraRowBorrowProfile;
    using Token = NativeCameraRowBorrow::Token;
    using CameraBytes = CameraRowBorrowOwnerFixture::CameraBytes;
    using EnginePump = CameraRowBorrowOwnerFixture::EnginePump;
    using CameraRowBorrowOwnerFixture::RowRva;
    using CameraTargetOwnerFixture::Refused;
    // Forward to the actual Runtime for every grant/refusal. Counts merely
    // observe stable publication and typed producer release; this provider
    // never synthesizes a successful native owner or drain admission.
    struct Access final : NativeGameTimingAccess
    {
        NativeGameTimingAccess& actual;
        const DWORD             producer;
        unsigned                acquisitions = 0, validations = 0, drainChecks = 0, drops = 0, dropMask = 0;
        explicit Access(NativeGameTimingAccess& owner, DWORD thread) : actual(owner), producer(thread) {}
        ClockResult SetTimeScale(float x) noexcept override { return actual.SetTimeScale(x); }
        ClockResult SetMinuteDuration(std::uint32_t x) noexcept override { return actual.SetMinuteDuration(x); }
        ClockResult Pause(bool x) noexcept override { return actual.Pause(x); }
        ClockResult AcquireCameraRowBorrow(std::uint32_t row, Token& token) noexcept override
        {
            ++acquisitions;
            return actual.AcquireCameraRowBorrow(row, token);
        }
        ClockResult ReleaseCameraRowBorrow(Token& token) noexcept override
        {
            const auto row = token.Row();
            ++drops;
            Check(GetCurrentThreadId() == producer && row < 3 && !(dropMask & (1u << row)),
                  "each actual typed cache product releases once on the recorded Begin/End producer");
            const auto result = actual.ReleaseCameraRowBorrow(token);
            Check(result == ClockResult::Ok && !token, "typed row destruction consumes the actual existing CPU token without native target cleanup");
            dropMask |= 1u << row;
            return result;
        }
        ClockResult ReadCameraRowVector(const Token& token, CameraRowVectorKind kind, CVector*& value) noexcept override
        {
            return actual.ReadCameraRowVector(token, kind, value);
        }
        ClockResult ReadCameraRowTarget(const Token& token, std::uintptr_t& value) noexcept override { return actual.ReadCameraRowTarget(token, value); }
        ClockResult SetCameraRowTarget(const Token& token, const NativePedReferenceToken* target) noexcept override
        {
            return actual.SetCameraRowTarget(token, target);
        }
        ClockResult ValidateCameraRowBorrow(const Token& token) noexcept override
        {
            ++validations;
            return actual.ValidateCameraRowBorrow(token);
        }
        ClockResult CheckCameraRowBorrowDrain(const Token& token) noexcept override
        {
            ++drainChecks;
            return actual.CheckCameraRowBorrowDrain(token);
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
        CameraBytes nativeCamera;
        nativeCamera.fill(0x5a);
        const auto vtable = fixture.image + P::CameraVtableRva;
        std::memcpy(nativeCamera.data(), &vtable, sizeof(vtable));
        nativeCamera[0x5b] = 255;
        apply(P::CameraRva, nativeCamera.data(), nativeCamera.size(), PAGE_READWRITE);
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

        Access access(runtime.access, producer);
        auto   registryStorage = std::make_unique<pool_registry_fixture::Pools>(access);
        auto&  registry = *registryStorage;
        // This bounded host owns the facade and explicit drain. Core's
        // current cleanup has no camera-owner drain hook; a future complete
        // CGame camera factory must wire this sequence before publication.
        CameraCamsObserver   camera(registry, access);
        std::array<CCam*, 3> products{};
        Token                independent;
        // Unknown native target bits are not entity adoption: publication
        // validation must discard the raw observation rather than map it.
        for (std::uint32_t row = 0; row != 3; ++row)
            fixture.Put(RowRva(row) + 0x1a0, (std::numeric_limits<std::uintptr_t>::max)() - row);
        const auto expectedCamera = fixture.Get<CameraBytes>(P::CameraRva);
        Check(camera.DrainCamsOnProducer() == ClockResult::Busy && !access.drops, "an open facade cannot drop products before close and native Stop");
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(GetCurrentThreadId() == engine.thread.load() && runtime.producerThread == producer && producer != GetCurrentThreadId(),
                  "actual GetCam publishes on the genuine after-application callback without impersonating its producer");
            const auto acquisitions = access.acquisitions, validations = access.validations;
            for (unsigned id = 3; id != 256; ++id)
                Check(camera.GetCam(static_cast<BYTE>(id)) == nullptr, "all classic out-of-range BYTE IDs preserve bounds-first null");
            Check(access.acquisitions == acquisitions && access.validations == validations,
                  "invalid GetCam IDs never enter a native provider or allocate a product");
            if (scenario == 1)
                Check(runtime.access.AcquireCameraRowBorrow(1, independent) == ClockResult::Ok,
                      "independent native row owner occupies row one before lazy facade publication");
            products[0] = camera.GetCam(0);
            Check(products[0] && camera.GetCam(0) == products[0],
                  "actual GetCam returns a stable owned typed product rather than constructing a second facade");
            if (scenario == 1)
            {
                Check(Refused([&] { camera.GetCam(1); }) && camera.GetCam(0) == products[0] && !access.drops,
                      "one genuine Busy row constructor failure leaves the earlier published product and hold owned");
                products[2] = camera.GetCam(2);
                Check(products[2] && camera.GetCam(2) == products[2], "a later independent row can publish despite another row's constructor refusal");
            }
            else
                for (BYTE id = 1; id != 3; ++id)
                    products[id] = camera.GetCam(id);
            Check(products[0]->GetFront() == reinterpret_cast<CVector*>(fixture.image + RowRva(0) + 0x110),
                  "published CCam points to the actual inherited native vector implementation of its held logical row");
        };
        pulse();
        Check(Refused([&] { camera.GetCam(0); }) && !access.drops, "a cached product alone grants no between-callback publication authority");
        if (scenario == 1)
        {
            Check(runtime.access.ReleaseCameraRowBorrow(independent) == ClockResult::Ok && !independent,
                  "the independent blocking row token drains only on its recorded producer");
            callback.action = [&](NativeGameTimingAccess&)
            {
                products[1] = camera.GetCam(1);
                Check(products[1] && camera.GetCam(0) == products[0] && camera.GetCam(2) == products[2] && !access.drops,
                      "a fresh admitted retry publishes only the formerly blocked row while both prior identities stay stable");
            };
            pulse();
            std::puts("Production camera GetCam independent publication fixture completed");
        }
        callback.action = [&](NativeGameTimingAccess&)
        {
            const auto acquisitions = access.acquisitions;
            for (BYTE id = 0; id != 3; ++id)
                Check(products[id] && camera.GetCam(id) == products[id] &&
                          products[id]->GetSource() == reinterpret_cast<CVector*>(fixture.image + RowRva(id) + 0x11c),
                      "all three actual GetCam products retain stable SDK identity and their own current native row");
            Check(
                products[0] != products[1] && products[1] != products[2] && products[0] != products[2] && access.acquisitions == acquisitions && !access.drops,
                "cached publication performs fresh validation without reacquiring or releasing any unique row slot");
        };
        pulse();
        Check(expectedCamera == fixture.Get<CameraBytes>(P::CameraRva) && !runtime.pedReferences.IsRetained() && !runtime.cameraTargetSetter.IsRetained(),
              "GetCam validation interprets no raw target and preserves whole camera reference cells without acquiring ped or setter debt");
        if (scenario == 2)
        {
            const auto entered = CreateEventW(nullptr, TRUE, FALSE, nullptr), release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Check(entered && release, "own bounded events for genuine in-flight producer drain refusal");
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto original = R::Pins[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(R::Pins[0].rva, &changed, 1);
                Check(Refused([&] { camera.GetCam(0); }) && !access.drops, "fresh source refusal cannot publish an already cached valid row");
                fixture.Patch(R::Pins[0].rva, &original, 1);
                Check(camera.GetCam(0) == products[0], "a new qualified observation returns the same retained product after an owned source fault ends");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                Check(Refused([&] { camera.GetCam(0); }) && !access.drops, "GC refusal is propagated instead of returning a cached pointer or null");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                Stop(ClockResult::Stopped);
                Check(Refused([&] { camera.GetCam(0); }) && !access.drops && camera.GetCam(255) == nullptr,
                      "native Stop closes cached valid-row publication while invalid IDs retain the classic bounds-first null branch");
                SetEvent(entered);
                Check(WaitForSingleObject(release, 5000) == WAIT_OBJECT_0, "the genuine callback waits only for bounded producer refusal observations");
            };
            Check(service.Renew(generation), "renew the actual producer before asynchronous native refusal qualification");
            SetEvent(engine.run);
            const auto started = GetTickCount64();
            DWORD      enteredResult = WAIT_TIMEOUT;
            while ((enteredResult = WaitForSingleObject(entered, 10)) == WAIT_TIMEOUT && GetTickCount64() - started < 50000)
                if (!runtime.stopping.load())
                    Check(service.Renew(generation), "public producer renews while the genuine engine qualifies cached refusal cases");
            Check(enteredResult == WAIT_OBJECT_0 && runtime.inFlight.load(), "actual engine callback remains in flight after all SDK borrowers quiesce");
            // The callback promises no further SDK access before signaling.
            // Close publication, retain that quiescence, then request Stop.
            camera.CloseCamPublication();
            Check(!service.End() && camera.DrainCamsOnProducer() == ClockResult::Busy && !access.drops && runtime.cameraRowBorrows.IsRetained(),
                  "a real in-flight callback refuses producer drain without dropping any cached product or hold");
            ClockResult foreign = ClockResult::Ok;
            std::thread outsider([&] { foreign = camera.DrainCamsOnProducer(); });
            outsider.join();
            Check(foreign == ClockResult::WrongThread && !access.drops && runtime.cameraRowBorrows.IsRetained(),
                  "foreign typed drain refuses on the actual native producer check and preserves all cached products");
            SetEvent(release);
            Check(WaitForSingleObject(engine.done, 5000) == WAIT_OBJECT_0 && !runtime.inFlight.load(),
                  "genuine callback returns before producer-owned typed drain begins");
            CloseHandle(entered);
            CloseHandle(release);
            std::puts("Production camera GetCam cached refusal fixture completed");
        }
        else
        {
            camera.CloseCamPublication();
            Check(camera.DrainCamsOnProducer() == ClockResult::Busy && !access.drops,
                  "closing publication alone never substitutes for actual native Stop admission");
            Check(!service.End() && runtime.cameraRowBorrows.IsRetained(),
                  "first producer End stops issuance while all three actual typed products retain the module");
            pulse(true);
        }
        const auto validations = access.validations, acquisitions = access.acquisitions;
        Check(Refused([&] { camera.GetCam(0); }) && camera.GetCam(3) == nullptr && access.validations == validations && access.acquisitions == acquisitions,
              "closed facade publication refuses valid products without reentering native callbacks and keeps invalid IDs bounds-first null");
        Check(camera.DrainCamsOnProducer() == ClockResult::Ok && access.drops == 3 && access.dropMask == 7 && !runtime.cameraRowBorrows.IsRetained(),
              "after close quiescence and Stop the actual producer prechecks every held row then drains exactly three typed CPU products");
        finish(&expectedCamera);
        Check(!runtime.cameraTargetSetter.IsRetained() && !runtime.pedReferences.IsRetained(),
              "row facade drain creates no engine target cleanup or entity lifetime authority");
        std::puts("Production camera GetCam actual SDK owner fixture completed");
        std::puts("Production camera GetCam stable rows fixture completed");
        std::puts("Production camera GetCam producer drain fixture completed");
    }
}

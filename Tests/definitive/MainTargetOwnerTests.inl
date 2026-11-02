/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe actual main-camera targets through the shared checked SDK path
 ****************************************************************************/
namespace CameraMainTargetOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace R = NativePedReferenceProfile;
    namespace S = NativeCameraMainTargetProfile;
    using Token = NativeCameraRowBorrow::Token;
    using CameraBytes = std::array<unsigned char, S::CameraBytes>;
    using EnginePump = CameraRowBorrowOwnerFixture::EnginePump;
    using StablePed = CameraTargetOwnerFixture::StablePed;
    using CameraRowBorrowOwnerFixture::RowRva;
    using CameraTargetOwnerFixture::Refused;
    static_assert(S::TargetOffset == 0x7f8 && S::TargetBytes == 8 && S::TargetOffset + S::TargetBytes == S::CameraBytes);
    static_assert(P::CameraBytes < S::TargetOffset);
    // All grants still come from the actual Runtime. The only scheduled
    // mutations occur between complete raw reads, making shared SDK token
    // and target bookends observable without bypassing native qualification.
    struct Access final : NativeGameTimingAccess
    {
        NativeGameTimingAccess& actual;
        unsigned                poolReads = 0, targetReads = 0, rowAcquires = 0;
        unsigned                mutateAt = 0;
        std::function<void()>   afterTarget;
        explicit Access(NativeGameTimingAccess& owner) : actual(owner) {}
        ClockResult SetTimeScale(float x) noexcept override { return actual.SetTimeScale(x); }
        ClockResult SetMinuteDuration(std::uint32_t x) noexcept override { return actual.SetMinuteDuration(x); }
        ClockResult Pause(bool x) noexcept override { return actual.Pause(x); }
        ClockResult CheckPedPoolAccess() noexcept override
        {
            ++poolReads;
            return actual.CheckPedPoolAccess();
        }
        ClockResult ProbePedPoolHandle(std::uint32_t h, bool& present, NativePedPoolReader::Value& v) noexcept override
        {
            return actual.ProbePedPoolHandle(h, present, v);
        }
        ClockResult ResolvePedPoolHandle(std::uint32_t h, NativePedPoolReader::Value& v) noexcept override { return actual.ResolvePedPoolHandle(h, v); }
        ClockResult RecheckPedPoolHandle(const NativePedPoolReader::Value& v) noexcept override { return actual.RecheckPedPoolHandle(v); }
        ClockResult AcquirePedReference(std::uint32_t h, NativePedReferenceToken& t) noexcept override { return actual.AcquirePedReference(h, t); }
        ClockResult ResolvePedReference(const NativePedReferenceToken& t, NativePedPoolReader::Value& v) noexcept override
        {
            return actual.ResolvePedReference(t, v);
        }
        ClockResult ReleasePedReference(const NativePedReferenceToken& t) noexcept override { return actual.ReleasePedReference(t); }
        ClockResult AcquireCameraRowBorrow(std::uint32_t row, Token& token) noexcept override
        {
            ++rowAcquires;
            return actual.AcquireCameraRowBorrow(row, token);
        }
        ClockResult ReleaseCameraRowBorrow(Token& token) noexcept override { return actual.ReleaseCameraRowBorrow(token); }
        ClockResult ReadCameraRowVector(const Token& token, CameraRowVectorKind kind, CVector*& value) noexcept override
        {
            return actual.ReadCameraRowVector(token, kind, value);
        }
        ClockResult ReadCameraMainTarget(std::uintptr_t& value) noexcept override
        {
            const auto result = actual.ReadCameraMainTarget(value);
            ++targetReads;
            if (result == ClockResult::Ok && afterTarget && targetReads == mutateAt)
            {
                auto action = std::move(afterTarget);
                afterTarget = {};
                action();
            }
            return result;
        }
        ClockResult ReadCameraRowTarget(const Token& token, std::uintptr_t& value) noexcept override { return actual.ReadCameraRowTarget(token, value); }
        ClockResult ValidateCameraRowBorrow(const Token& token) noexcept override { return actual.ValidateCameraRowBorrow(token); }
        ClockResult CheckCameraRowBorrowDrain(const Token& token) noexcept override { return actual.CheckCameraRowBorrowDrain(token); }
        void        Schedule(unsigned read, std::function<void()> action)
        {
            targetReads = 0;
            mutateAt = read;
            afterTarget = std::move(action);
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
        for (const auto& pin : NativeCameraRowBorrowProfile::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeCameraHistoryProfile::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& group : P::Groups)
        {
            writes.Global(group.slotRva, fixture.image + group.entryRva, PAGE_READONLY);
            writes.Global(group.tableSlotRva, static_cast<std::uint32_t>(group.branchRva), PAGE_EXECUTE_READ);
        }
        writes.Global(P::CrtPointerRva, fixture.image + P::CrtWrapperRva, PAGE_READONLY);
        for (const auto& pin : NativePedPoolProfile::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
        for (const auto& pin : S::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        fixture.Seed();
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        writes.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
        std::vector<unsigned char> referenceBank(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t index = 0; index + 1 < R::ReferenceCount; ++index)
        {
            const auto next = fixture.image + R::ReferencesRva + (index + 1) * R::ReferenceBytes;
            std::memcpy(referenceBank.data() + index * R::ReferenceBytes, &next, sizeof(next));
        }
        apply(R::ReferencesRva, referenceBank.data(), referenceBank.size(), PAGE_READWRITE);
        writes.Global(R::FreeHeadRva, fixture.image + R::ReferencesRva);
        ClockTeleportPool pool(fixture, NativePedPoolProfile::RootRva, NativePedPoolProfile::NativeCapacity, NativePedPoolProfile::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto pedAddress = reinterpret_cast<std::uintptr_t>(pool.objects);
        const auto pedVtable = fixture.image + R::PedVtableRva;
        std::memcpy(pool.objects, &pedVtable, sizeof(pedVtable));
        pool.objects[R::TypeOffset] = 3;
        // A second genuinely occupied native slot has no SDK registration.
        pool.flags[8] = 127;
        auto* unregistered = pool.objects + 8 * NativePedPoolProfile::ObjectStride;
        std::memcpy(unregistered, &pedVtable, sizeof(pedVtable));
        unregistered[R::TypeOffset] = 3;
        const auto oldReferenceImage = ClockPedReferenceOwnerTest::image;
        ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
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
            ClockPedReferenceOwnerTest::image = oldReferenceImage;
        };

        Access                                            access(runtime.access), foreignAccess(runtime.access);
        auto                                              registryStorage = std::make_unique<pool_registry_fixture::Pools>(access);
        auto&                                             registry = *registryStorage;
        auto                                              foreignStorage = std::make_unique<pool_registry_fixture::Pools>(foreignAccess);
        auto&                                             foreignRegistry = *foreignStorage;
        CPedBorrowedFactoryDE<StablePed>                  firstFactory(registry), secondFactory(registry);
        CPedBorrowedFactoryDE<camera_target_fixture::Ped> foreignFactory(foreignRegistry);
        CameraTargetObserver                              mainCamera(registry, access);
        CPed*                                             sdkPed = nullptr;
        CCam*                                             row = nullptr;
        const auto                                        mainCell = P::CameraRva + S::TargetOffset, rowCell = RowRva(0) + 0x1a0;
        fixture.Put(mainCell, std::uintptr_t{0});
        fixture.Put(rowCell, pedAddress);
        const auto bankSnapshot = [&]
        {
            std::vector<unsigned char> bank(referenceBank.size());
            std::memcpy(bank.data(), reinterpret_cast<void*>(fixture.image + R::ReferencesRva), bank.size());
            return bank;
        };
        const auto finishFacade = [&]
        {
            const auto expected = fixture.Get<CameraBytes>(P::CameraRva);
            mainCamera.CloseCamPublication();
            Check(!service.End() && runtime.cameraRowBorrows.IsRetained(),
                  "first End stops native issuance while inherited actual row product retains the main camera owner");
            pulse(true);
            Check(mainCamera.DrainCamsOnProducer() == ClockResult::Ok && !runtime.cameraRowBorrows.IsRetained(),
                  "main target slice preserves the actual producer-only inherited Cam product drain");
            finish(&expected);
        };
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(GetCurrentThreadId() == engine.thread.load() && runtime.producerThread == producer && producer != GetCurrentThreadId(),
                  "main target uses the actual current after-application owner without a row-token substitute");
            Check(firstFactory.Create(1, nullptr, registry) == ClockResult::Ok && firstFactory.Read(sdkPed) == ClockResult::Ok && sdkPed,
                  "main target mapping reuses a genuine controller-issued and CPU-registered SDK ped");
            const auto acquisitions = access.rowAcquires, poolReads = access.poolReads, reads = access.targetReads;
            Check(mainCamera.GetTargetEntity() == nullptr && access.rowAcquires == acquisitions && access.poolReads == poolReads &&
                      access.targetReads == reads + 2,
                  "qualified zero main target returns null without a row hold pool lookup native reference or fabricated entity");
            row = mainCamera.GetCam(0);
            Check(row && row->GetTargetEntity() == static_cast<CEntity*>(sdkPed) && mainCamera.GetTargetEntity() == nullptr,
                  "main7F8 remains null while row1A0 names a controlled ped so main getter cannot alias the selected row target");
        };
        pulse();
        Check(Refused([&] { mainCamera.GetTargetEntity(); }), "cached facade and a genuine ped token grant no between-callback main target publication");
        const auto refuseSdk = [&](const char* why)
        {
            auto*      output = reinterpret_cast<CEntity*>(std::uintptr_t{0x12345678});
            const auto beforeCamera = fixture.Get<CameraBytes>(P::CameraRva);
            const auto bank = bankSnapshot();
            const auto registrations = ClockPedReferenceOwnerTest::registers, unregistrations = ClockPedReferenceOwnerTest::unregisters;
            Check(Refused([&] { output = mainCamera.GetTargetEntity(); }) && output == reinterpret_cast<CEntity*>(std::uintptr_t{0x12345678}) &&
                      beforeCamera == fixture.Get<CameraBytes>(P::CameraRva) && bank == bankSnapshot() &&
                      registrations == ClockPedReferenceOwnerTest::registers && unregistrations == ClockPedReferenceOwnerTest::unregisters,
                  why);
        };
        if (scenario == 0)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                fixture.Put(mainCell, pedAddress);
                fixture.Put(rowCell, std::uintptr_t{0});
                const auto cameraBefore = fixture.Get<CameraBytes>(P::CameraRva);
                const auto bank = bankSnapshot();
                const auto acquisitions = access.rowAcquires;
                Check(mainCamera.GetTargetEntity() == static_cast<CEntity*>(sdkPed) && row->GetTargetEntity() == nullptr,
                      "actual main getter returns its controlled CEntity virtual base independently of a null held row target");
                std::uintptr_t raw = 0;
                Check(runtime.access.ReadCameraMainTarget(raw) == ClockResult::Ok && raw == pedAddress && access.rowAcquires == acquisitions &&
                          cameraBefore == fixture.Get<CameraBytes>(P::CameraRva) && bank == bankSnapshot() && ClockPedReferenceOwnerTest::registers == 1 &&
                          !ClockPedReferenceOwnerTest::unregisters,
                      "raw and actual main observers preserve the full800 camera extent and3200 reference bank without row acquisition or native effects");
                Check(reinterpret_cast<std::uintptr_t>(sdkPed->GetInterface()) == pedAddress,
                      "controlled main mapping shares the same actual pooled interface provenance");
                Check(firstFactory.Retire() == ClockResult::Ok,
                      "explicit real factory retirement withdraws the SDK identity without writing main or row target cells");
                refuseSdk("a nonzero main native target whose SDK registration is withdrawn is explicit refusal rather than qualified null");
            };
            pulse();
            finishFacade();
            std::puts("Production camera main target actual SDK owner fixture completed");
            std::puts("Production camera main target row distinction fixture completed");
            std::puts("Production camera main target controlled lookup fixture completed");
            return;
        }
        if (scenario == 1)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                for (const auto unknown : {std::uintptr_t{1}, (std::numeric_limits<std::uintptr_t>::max)(), reinterpret_cast<std::uintptr_t>(unregistered),
                                           pedAddress ^ (std::uintptr_t{1} << 32)})
                {
                    fixture.Put(mainCell, unknown);
                    std::uintptr_t raw = 0;
                    Check(runtime.access.ReadCameraMainTarget(raw) == ClockResult::Ok && raw == unknown,
                          "qualified raw main target observes unknown or noncanonical bits without dereferencing them");
                    refuseSdk("unknown nonzero or occupied unregistered main targets cannot become null-success or adopted SDK entities");
                }
                CPed* foreignPed = nullptr;
                Check(Refused([&] { CameraTargetObserver wrong(foreignRegistry, access); }),
                      "main facade rejects a mismatched CPU pool/native owner before publishing a product");
                Check(foreignFactory.Create(0x87f, nullptr, foreignRegistry) == ClockResult::Ok && foreignFactory.Read(foreignPed) == ClockResult::Ok &&
                          foreignPed,
                      "foreign-registry target is a real separately controlled SDK ped rather than a forged object");
                fixture.Put(mainCell, reinterpret_cast<std::uintptr_t>(unregistered));
                refuseSdk("a native ped registered only by another SDK owner cannot be adopted by main target lookup");
                Check(foreignFactory.Retire() == ClockResult::Ok, "the separate genuine foreign factory retires its own private reference explicitly");
                fixture.Put(mainCell, pedAddress);
                const auto refuseRaw = [&](const char* why)
                {
                    std::uintptr_t output = 0x12345678;
                    Check(runtime.access.ReadCameraMainTarget(output) != ClockResult::Ok && output == 0x12345678, why);
                    refuseSdk(why);
                };
                const auto frame = frameCallbackSerial;
                --frameCallbackSerial;
                refuseRaw("stale frame refuses raw and actual main output without a row-token bypass");
                frameCallbackSerial = frame;
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                refuseRaw("GC excludes main target publication");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
                refuseRaw("main target requires current playing state");
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{123});
                refuseRaw("main target rejects stale typed-world membership");
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{3});
                const auto original = S::Pins[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(S::Pins[0].rva, &changed, 1);
                refuseRaw("main target consumes its independently qualified exact source profile");
                Check(row->GetTargetEntity() == static_cast<CEntity*>(sdkPed),
                      "separate main source refusal does not strengthen or reorder the established row getter path");
                fixture.Patch(S::Pins[0].rva, &original, 1);
                const auto token = firstFactory.ReadReceipt().reference;
                pool.flags[0] = 2;
                refuseSdk("reused native generation refuses the controlled main SDK token");
                pool.flags[0] = 1;
                NativePedPoolReader::Value value;
                Check(runtime.access.ResolvePedReference(token, value) == ClockResult::NotInitialized,
                      "restoring owned pool bytes does not revive a revoked native relation");
                Check(firstFactory.Retire() == ClockResult::Ok, "real revoked relation retires through its existing explicit factory release");
                refuseSdk("withdrawn main registration remains unsupported without creating or adopting a replacement wrapper");
                Stop(ClockResult::Stopped);
                std::uintptr_t output = 0x12345678;
                Check(runtime.access.ReadCameraMainTarget(output) != ClockResult::Ok && output == 0x12345678 && Refused([&] { mainCamera.GetTargetEntity(); }),
                      "Stop closes main publication independently of any cached row product or stale entity token");
            };
            pulse();
            finishFacade();
            std::puts("Production camera main target identity refusal fixture completed");
            std::puts("Production camera main target bookend fixture completed");
            return;
        }
        callback.action = [&](NativeGameTimingAccess&)
        {
            fixture.Put(mainCell, pedAddress);
            access.Schedule(1, [&] { fixture.Put(mainCell, std::uintptr_t{0}); });
            Check(Refused([&] { mainCamera.GetTargetEntity(); }), "nonzero-to-zero main drift refuses binding rather than publishing stale SDK identity");
            access.Schedule(1, [&] { fixture.Put(mainCell, pedAddress); });
            Check(Refused([&] { mainCamera.GetTargetEntity(); }), "zero-to-nonzero main drift refuses a stale qualified null");
            const auto                 oldToken = firstFactory.ReadReceipt().reference;
            NativePedPoolReader::Value before, after;
            Check(runtime.access.ResolvePedReference(oldToken, before) == ClockResult::Ok, "actual old token is current before main lookup ABA");
            auto* const oldSdk = sdkPed;
            const auto  cameraBefore = fixture.Get<CameraBytes>(P::CameraRva);
            const auto  registrations = ClockPedReferenceOwnerTest::registers, unregistrations = ClockPedReferenceOwnerTest::unregisters;
            access.Schedule(
                2,
                [&]
                {
                    Check(firstFactory.Retire() == ClockResult::Ok && !CameraTargetOwnerFixture::storageUsed,
                          "first real factory withdraws the old main identity before deterministic CPU storage reuse");
                    Check(secondFactory.Create(1, nullptr, registry) == ClockResult::Ok && secondFactory.Read(sdkPed) == ClockResult::Ok && sdkPed == oldSdk,
                          "second actual factory reuses the same SDK address with a genuinely newly issued native token");
                    const auto next = secondFactory.ReadReceipt().reference;
                    Check((oldToken.slot != next.slot || oldToken.version != next.version || oldToken.ownerGeneration != next.ownerGeneration) &&
                              runtime.access.ResolvePedReference(next, after) == ClockResult::Ok && CameraTargetOwnerFixture::SameValue(before, after),
                          "native value remains identical while the actual controller token identity changes across main lookup");
                });
            Check(Refused([&] { mainCamera.GetTargetEntity(); }),
                  "shared main lookup refuses genuine token ABA despite identical SDK pointer and native value");
            Check(cameraBefore == fixture.Get<CameraBytes>(P::CameraRva) && ClockPedReferenceOwnerTest::registers == registrations + 1 &&
                      ClockPedReferenceOwnerTest::unregisters == unregistrations + 1 && mainCamera.GetTargetEntity() == static_cast<CEntity*>(sdkPed) &&
                      row->GetTargetEntity() == static_cast<CEntity*>(sdkPed),
                  "only explicit factory lifecycle changes references and fresh row/main lookups anchor the newly controlled identity");
            Check(secondFactory.Retire() == ClockResult::Ok, "second genuine factory explicitly retires before main owner Stop");
        };
        pulse();
        finishFacade();
        std::puts("Production camera main target token ABA fixture completed");
        std::puts("Production camera main target bookend fixture completed");
    }
}

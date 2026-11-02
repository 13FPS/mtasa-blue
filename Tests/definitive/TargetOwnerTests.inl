/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Resolve held camera targets through actual registered SDK ped identity
 ****************************************************************************/
namespace CameraTargetOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace R = NativePedReferenceProfile;
    using Token = NativeCameraRowBorrow::Token;
    using CameraBytes = CameraRowBorrowOwnerFixture::CameraBytes;
    using EnginePump = CameraRowBorrowOwnerFixture::EnginePump;
    using CameraRowBorrowOwnerFixture::RowRva;
    constexpr std::size_t TargetOffset = 0x1a0;

    // Every observation still runs the real runtime/controller. This forwarding
    // boundary schedules owned changes only BETWEEN complete native reads, so
    // the SDK must detect changes across its own registry/native bookends.
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
        ClockResult ReadCameraRowTarget(const Token& token, std::uintptr_t& value) noexcept override
        {
            const auto result = actual.ReadCameraRowTarget(token, value);
            ++targetReads;
            if (result == ClockResult::Ok && afterTarget && targetReads == mutateAt)
            {
                auto action = std::move(afterTarget);
                afterTarget = {};
                action();
            }
            return result;
        }
        void Schedule(unsigned read, std::function<void()> action)
        {
            targetReads = 0;
            mutateAt = read;
            afterTarget = std::move(action);
        }
    };

    // Deterministic CPU wrapper storage makes address reuse provable. Both
    // lifecycles still use the actual factory/controller and newly issued
    // native reference token; this allocates or adopts no native entity.
    // This test-only derivative deliberately inherits the actual pooled/vitals
    // implementations through their single virtual CPed. MSVC reports C4250
    // again for this extra storage-control layer; forwarding overrides would
    // hide the real inherited SDK dispatch that the fixture must exercise.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4250)
#endif
    struct StablePed final : camera_target_fixture::Ped
    {
        using camera_target_fixture::Ped::Ped;
        static void* operator new(std::size_t bytes);
        static void  operator delete(void*) noexcept;
    };
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
    alignas(StablePed) std::array<unsigned char, sizeof(StablePed)> stableStorage{};
    bool  storageUsed = false;
    void* StablePed::operator new(std::size_t bytes)
    {
        Check(bytes == stableStorage.size() && !storageUsed, "only one actual typed CPU wrapper occupies the deterministic fixture storage");
        storageUsed = true;
        return stableStorage.data();
    }
    void StablePed::operator delete(void* pointer) noexcept
    {
        Check(pointer == stableStorage.data() && storageUsed, "typed wrapper retirement returns its own CPU fixture storage");
        storageUsed = false;
    }
    bool SameValue(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b)
    {
        return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
               a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
               a.ownsAllocations == b.ownsAllocations;
    }
    template <class Operation>
    bool Refused(Operation&& operation)
    {
        try
        {
            operation();
        }
        catch (const std::exception&)
        {
            return true;
        }
        return false;
    }
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

        Access access(runtime.access), foreign(runtime.access);
        // The actual fixed-capacity registries contain their full native-sized
        // CPU bookkeeping. Own them off the bounded host stack so nested real
        // Begin/image qualification retains its normal stack budget.
        auto                               registryStorage = std::make_unique<pool_registry_fixture::Pools>(access);
        auto                               foreignRegistryStorage = std::make_unique<pool_registry_fixture::Pools>(foreign);
        auto&                              registry = *registryStorage;
        auto&                              foreignRegistry = *foreignRegistryStorage;
        CPedBorrowedFactoryDE<StablePed>   firstFactory(registry), secondFactory(registry);
        std::unique_ptr<CamTargetObserver> row;
        CPed*                              sdkPed = nullptr;
        Token                              raw;
        const auto                         cell = RowRva(0) + TargetOffset;
        fixture.Put(cell, std::uintptr_t{0});
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(GetCurrentThreadId() == engine.thread.load() && runtime.producerThread == producer && producer != GetCurrentThreadId(),
                  "target composition runs on the actual engine callback without rewriting producer identity");
            const auto before = access.rowAcquires;
            Check(
                Refused([&] { CamTargetObserver wrong(2, foreignRegistry, access); }) && access.rowAcquires == before && !runtime.cameraRowBorrows.IsRetained(),
                "different registry owner refuses before any camera hold can be acquired or stranded");
            Check(firstFactory.Create(1, nullptr, registry) == ClockResult::Ok && firstFactory.Read(sdkPed) == ClockResult::Ok && sdkPed,
                  "actual controlled factory acquires and registers the real native ped before target lookup");
            row = std::make_unique<CamTargetObserver>(0, registry, access);
            if (scenario == 1)
                Check(runtime.access.AcquireCameraRowBorrow(1, raw) == ClockResult::Ok, "raw refusal channel uses the existing second row hold");
        };
        pulse();
        Check(ClockPedReferenceOwnerTest::registers == 1 && !ClockPedReferenceOwnerTest::unregisters && runtime.pedReferences.IsRetained(),
              "one actual reference-controller registration backs the composed pooled ped");
        Check(Refused([&] { sdkPed->GetInterface(); }) && Refused([&] { row->GetTargetEntity(); }),
              "neither an opaque interface tag nor target getter escapes between genuine native callbacks");
        const auto bankSnapshot = [&]()
        {
            std::vector<unsigned char> result(referenceBank.size());
            std::memcpy(result.data(), reinterpret_cast<void*>(fixture.image + R::ReferencesRva), result.size());
            return result;
        };
        const auto targetRefusal = [&](const char* why)
        {
            auto*      output = reinterpret_cast<CEntity*>(std::uintptr_t{0x12345678});
            const auto cameraBefore = fixture.Get<CameraBytes>(P::CameraRva);
            const auto bank = bankSnapshot();
            const auto registrations = ClockPedReferenceOwnerTest::registers, unregistrations = ClockPedReferenceOwnerTest::unregisters;
            Check(Refused([&] { output = row->GetTargetEntity(); }) && output == reinterpret_cast<CEntity*>(std::uintptr_t{0x12345678}) &&
                      cameraBefore == fixture.Get<CameraBytes>(P::CameraRva) && bank == bankSnapshot() &&
                      registrations == ClockPedReferenceOwnerTest::registers && unregistrations == ClockPedReferenceOwnerTest::unregisters,
                  why);
        };
        if (scenario == 0)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto poolBefore = access.poolReads, rawBefore = access.targetReads;
                Check(row->GetTargetEntity() == nullptr && access.poolReads == poolBefore && access.targetReads == rawBefore + 2,
                      "stable native zero is a qualified null without consulting or constructing a ped registry object");
                fixture.Put(cell, pedAddress);
                const auto cameraBefore = fixture.Get<CameraBytes>(P::CameraRva);
                const auto bank = bankSnapshot();
                const auto freeHead = fixture.Get<std::uintptr_t>(R::FreeHeadRva);
                const auto entityHead = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(pedAddress + R::EntityHeadOffset);
                Check(row->GetTargetEntity() == static_cast<CEntity*>(sdkPed), "actual target getter returns the controlled SDK virtual-base identity");
                Check(reinterpret_cast<std::uintptr_t>(sdkPed->GetInterface()) == pedAddress,
                      "actual pooled GetInterface returns the freshly requalified opaque native address of this registered SDK ped");
                Check(cameraBefore == fixture.Get<CameraBytes>(P::CameraRva) && bank == bankSnapshot() &&
                          freeHead == fixture.Get<std::uintptr_t>(R::FreeHeadRva) &&
                          entityHead == ClockPedReferenceOwnerTest::Get<std::uintptr_t>(pedAddress + R::EntityHeadOffset) &&
                          ClockPedReferenceOwnerTest::registers == 1 && !ClockPedReferenceOwnerTest::unregisters && registry.GetPedCount() == 1,
                      "both actual getters preserve the whole camera target cell native reference graph and CPU registration count");
                Check(firstFactory.Retire() == ClockResult::Ok && registry.GetPedCount() == 0,
                      "real factory withdrawal precedes typed SDK destruction and native unlink");
                targetRefusal("a remaining nonzero native target with its SDK registration withdrawn is refusal rather than null success");
            };
            pulse();
            std::puts("Production camera target actual SDK owner fixture completed");
            std::puts("Production ped pooled interface fixture completed");
            std::puts("Production camera target controlled lookup fixture completed");
            std::puts("Production camera target owner composition fixture completed");
        }
        else if (scenario == 1)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                for (const auto unknown : {std::uintptr_t{1}, (std::numeric_limits<std::uintptr_t>::max)(), reinterpret_cast<std::uintptr_t>(unregistered),
                                           pedAddress ^ (std::uintptr_t{1} << 32)})
                {
                    fixture.Put(cell, unknown);
                    targetRefusal("unknown or occupied unregistered nonzero targets are explicitly refused without dereferencing arbitrary native bits");
                }
                bool                       present = false;
                NativePedPoolReader::Value occupied;
                Check(runtime.access.ProbePedPoolHandle(0x87f, present, occupied) == ClockResult::Ok && present &&
                          occupied.nativePed == reinterpret_cast<std::uintptr_t>(unregistered),
                      "unsupported target is genuinely present in the native pool but has no controlled SDK registration");
                fixture.Put(cell, pedAddress);
                const auto refuseRaw = [&](const char* why)
                {
                    std::uintptr_t output = 0x12345678;
                    Check(runtime.access.ReadCameraRowTarget(raw, output) != ClockResult::Ok && output == 0x12345678, why);
                    targetRefusal(why);
                };
                const auto frame = frameCallbackSerial;
                --frameCallbackSerial;
                refuseRaw("raw target and actual SDK getter reject stale frame authority while preserving outputs");
                frameCallbackSerial = frame;
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                refuseRaw("GC excludes target publication");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
                refuseRaw("target publication requires current playing state");
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
                const auto original = NativeCameraRowBorrowProfile::Pins[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(NativeCameraRowBorrowProfile::Pins[0].rva, &changed, 1);
                refuseRaw("target field-role source qualification remains mandatory");
                fixture.Patch(NativeCameraRowBorrowProfile::Pins[0].rva, &original, 1);
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{123});
                refuseRaw("stale typed-world membership refuses target publication");
                NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), 0xc, std::int32_t{3});
                SClientEntity<CPed>* link = nullptr;
                Check(registry.ReadPedByAddress(pedAddress, link) == ClockResult::Ok && link && link->pEntity == sdkPed,
                      "actual mutable SDK registry row is controlled before fault injection");
                link->pEntity = nullptr;
                targetRefusal("withdrawn mutable SDK row cannot be replaced by a native address cast");
                Check(Refused([&] { sdkPed->GetInterface(); }), "opaque pooled interface also refuses a broken controlled registry inverse");
                link->pEntity = sdkPed;
                pool.flags[0] = 2;
                targetRefusal("a reused native generation refuses the old controlled token instead of returning its cached SDK object");
                Check(Refused([&] { sdkPed->GetInterface(); }), "GetInterface requires current native generation provenance");
                pool.flags[0] = 1;
                Check(firstFactory.Retire() == ClockResult::Ok, "restore owned fault fields before the one actual factory retirement");
                Stop(ClockResult::Stopped);
                std::uintptr_t output = 0x12345678;
                Check(runtime.access.ReadCameraRowTarget(raw, output) != ClockResult::Ok && output == 0x12345678 && Refused([&] { row->GetTargetEntity(); }),
                      "Stop closes raw and actual target publication without forcing held rows away");
            };
            pulse();
            std::puts("Production camera target refusals fixture completed");
        }
        else
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                fixture.Put(cell, pedAddress);
                access.Schedule(1, [&] { fixture.Put(cell, std::uintptr_t{0}); });
                Check(Refused([&] { row->GetTargetEntity(); }) && fixture.Get<std::uintptr_t>(cell) == 0,
                      "nonzero to zero target drift between real native reads is binding refusal rather than qualified absence");
                access.Schedule(1, [&] { fixture.Put(cell, pedAddress); });
                Check(Refused([&] { row->GetTargetEntity(); }) && fixture.Get<std::uintptr_t>(cell) == pedAddress,
                      "zero to nonzero target drift cannot publish the stale null snapshot");
                const auto                 oldToken = firstFactory.ReadReceipt().reference;
                NativePedPoolReader::Value before, after;
                Check(runtime.access.ResolvePedReference(oldToken, before) == ClockResult::Ok, "actual issuer qualifies the original factory token before ABA");
                auto* const originalSdk = sdkPed;
                const auto  registrations = ClockPedReferenceOwnerTest::registers, unregistrations = ClockPedReferenceOwnerTest::unregisters;
                const auto  cameraBefore = fixture.Get<CameraBytes>(P::CameraRva);
                access.Schedule(
                    2,
                    [&]
                    {
                        Check(firstFactory.Retire() == ClockResult::Ok && !storageUsed, "real first factory withdraws and unlinks before CPU storage reuse");
                        Check(secondFactory.Create(1, nullptr, registry) == ClockResult::Ok && secondFactory.Read(sdkPed) == ClockResult::Ok &&
                                  sdkPed == originalSdk,
                              "actual second factory registers a new native token at the identical SDK wrapper address");
                        const auto next = secondFactory.ReadReceipt().reference;
                        Check((oldToken.version != next.version || oldToken.slot != next.slot || oldToken.ownerGeneration != next.ownerGeneration) &&
                                  runtime.access.ResolvePedReference(next, after) == ClockResult::Ok && SameValue(before, after),
                              "token identity changes even while the SDK pointer and complete native pool value are reused");
                    });
                Check(Refused([&] { row->GetTargetEntity(); }),
                      "whole target getter refuses genuine controller token ABA across same SDK pointer and native value");
                Check(cameraBefore == fixture.Get<CameraBytes>(P::CameraRva) && ClockPedReferenceOwnerTest::registers == registrations + 1 &&
                          ClockPedReferenceOwnerTest::unregisters == unregistrations + 1 && registry.GetPedCount() == 1,
                      "only the explicit factory lifecycle changes reference registration while target getter preserves the native cell");
                Check(row->GetTargetEntity() == static_cast<CEntity*>(sdkPed) && reinterpret_cast<std::uintptr_t>(sdkPed->GetInterface()) == pedAddress,
                      "fresh subsequent lookup succeeds only after anchoring the newly issued controlled identity");
                Check(secondFactory.Retire() == ClockResult::Ok, "second actual factory retires once before native owner Stop");
            };
            pulse();
            std::puts("Production camera target controller token ABA fixture completed");
            std::puts("Production camera target refusals fixture completed");
        }
        Check(!registry.GetPedCount() && !runtime.pedReferences.IsRetained() && !storageUsed &&
                  ClockPedReferenceOwnerTest::registers == ClockPedReferenceOwnerTest::unregisters,
              "all real typed ped factory lifecycles drain their reference cells before the camera owner stops");
        const auto expected = fixture.Get<CameraBytes>(P::CameraRva);
        Check(!service.End() && runtime.cameraRowBorrows.IsRetained() && runtime.frameHooked.load(),
              "target wrapper holds the same actual generation and hooks after End requests Stop");
        pulse(true);
        row.reset();
        if (raw)
            Check(runtime.access.ReleaseCameraRowBorrow(raw) == ClockResult::Ok && !raw, "same producer releases raw row CPU hold after Stop");
        Check(!runtime.cameraRowBorrows.IsRetained(), "typed target wrapper drains its inherited row hold only on the recorded producer");
        finish(&expected);
    }
}

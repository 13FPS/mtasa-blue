/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual SDK target assignment and retained native prefixes
 ****************************************************************************/
#if !defined(MTA_DE_CAMERA_TARGET_OWNER_BODY)
namespace ClockCameraTargetSetterOwnerTest
{
    using namespace mta::game_de;
    namespace C = NativeCameraTargetSetterCalls;
    unsigned            scenario = 0, unregisterCalls = 0, pointerCalls = 0, registerCalls = 0, step = 0;
    std::array<char, 3> order{};
    void                Reset(unsigned selected = 0)
    {
        scenario = selected;
        unregisterCalls = pointerCalls = registerCalls = step = 0;
        order.fill(0);
    }
    void Record(char operation)
    {
        if (step >= order.size())
            std::abort();
        order[step++] = operation;
    }
    ClockResult Unregister(C::UnregisterFunction function, void* ped, void** cell, std::uint32_t& exception) noexcept
    {
        ++unregisterCalls;
        Record('U');
        if (scenario == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        const auto result = ClockPedReferenceOwnerTest::Unregister(function, ped, cell, exception);
        if (scenario == 3)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        return result;
    }
    ClockResult Exchange(std::uintptr_t cell, std::uintptr_t before, std::uintptr_t after, std::uintptr_t& observed, bool& accepted,
                         std::uint32_t& exception) noexcept
    {
        ++pointerCalls;
        Record('C');
        // The returned mismatch is a real expected-old CAS against an owned
        // competing value. No rejected comparison is silently overwritten.
        if (scenario == 1 || scenario == 4)
            *reinterpret_cast<std::uintptr_t*>(cell) = after;
        auto* previous =
            InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(cell), reinterpret_cast<void*>(after), reinterpret_cast<void*>(before));
        observed = reinterpret_cast<std::uintptr_t>(previous);
        accepted = observed == before;
        if (scenario == 5)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        return ClockResult::Ok;
    }
    ClockResult Register(C::RegisterFunction function, void* ped, void** cell, bool force, std::uint32_t& exception) noexcept
    {
        ++registerCalls;
        Record('R');
        if (force)
            std::abort();
        if (scenario == 6)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        const auto result = ClockPedReferenceOwnerTest::Register(function, ped, cell, force, exception);
        if (scenario == 7)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        return result;
    }
}
#else
namespace CameraTargetSetterOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace R = NativePedReferenceProfile;
    namespace Host = ClockCameraTargetSetterOwnerTest;
    namespace Calls = NativeCameraTargetSetterCalls;
    using CameraBytes = CameraRowBorrowOwnerFixture::CameraBytes;
    using EnginePump = CameraRowBorrowOwnerFixture::EnginePump;
    using CameraRowBorrowOwnerFixture::RowRva;
    using CameraTargetOwnerFixture::Refused;
    constexpr std::size_t TargetOffset = 0x1a0;
    struct Node
    {
        std::uintptr_t next = 0, cell = 0;
    };
    static_assert(sizeof(Node) == R::ReferenceBytes);
    using Bank = std::array<Node, R::ReferenceCount>;
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
        pool.flags[1] = 1;
        auto* secondNative = pool.objects + NativePedPoolProfile::ObjectStride;
        std::memcpy(secondNative, &pedVtable, sizeof(pedVtable));
        secondNative[R::TypeOffset] = 3;
        const auto secondAddress = reinterpret_cast<std::uintptr_t>(secondNative);
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
        const auto finish = [&](const CameraBytes* expected = nullptr, const Bank* expectedBank = nullptr)
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
            if (expectedBank)
                Check(std::memcmp(expectedBank, reinterpret_cast<void*>(fixture.image + R::ReferencesRva), sizeof(Bank)) == 0,
                      "successful End preserves the complete engine-owned weak reference graph without unregistering or restoring it");
            engine.Stop();
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
            ClockPedReferenceOwnerTest::image = oldReferenceImage;
        };

        auto                             registryStorage = std::make_unique<pool_registry_fixture::Pools>(runtime.access);
        auto&                            registry = *registryStorage;
        CameraTargetOwnerFixture::Access foreign(runtime.access);
        auto                             foreignStorage = std::make_unique<pool_registry_fixture::Pools>(foreign);
        auto&                            foreignRegistry = *foreignStorage;
        using Factory = CPedBorrowedFactoryDE<camera_target_fixture::Ped>;
        Factory                                  firstFactory(registry), secondFactory(registry), foreignFactory(foreignRegistry);
        CPed *                                   firstPed = nullptr, *secondPed = nullptr, *foreignPed = nullptr;
        std::unique_ptr<CamTargetSetterObserver> row;
        const auto                               cell = RowRva(0) + TargetOffset;
        const auto                               cellAddress = fixture.image + cell;
        fixture.Put(cell, std::uintptr_t{0});
        Host::Reset();
        const auto cameraOriginal = fixture.Get<CameraBytes>(P::CameraRva);
        const auto bankSnapshot = [&]
        {
            auto bank = std::make_unique<Bank>();
            std::memcpy(bank.get(), reinterpret_cast<void*>(fixture.image + R::ReferencesRva), sizeof(Bank));
            return bank;
        };
        const auto engineReferences = [&](std::uintptr_t ped)
        {
            auto     current = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(ped + R::EntityHeadOffset);
            unsigned count = 0, steps = 0;
            while (current)
            {
                Check(++steps <= R::ReferenceCount && current >= fixture.image + R::ReferencesRva &&
                          current < fixture.image + R::ReferencesRva + sizeof(Bank) && (current - fixture.image - R::ReferencesRva) % sizeof(Node) == 0,
                      "the engine-owned target chain remains bounded within actual aligned native reference nodes");
                const auto node = ClockPedReferenceOwnerTest::Get<Node>(current);
                if (node.cell == cellAddress)
                    ++count;
                current = node.next;
            }
            return count;
        };
        const auto preserveCamera = [&](std::uintptr_t target)
        {
            auto expected = cameraOriginal;
            std::memcpy(expected.data() + cell - P::CameraRva, &target, sizeof(target));
            Check(expected == fixture.Get<CameraBytes>(P::CameraRva), "target assignment preserves every camera byte outside the engine-owned QWORD cell");
        };
        const auto retireFactories = [&]
        {
            Check(firstFactory.Retire() == ClockResult::Ok && secondFactory.Retire() == ClockResult::Ok && foreignFactory.Retire() == ClockResult::Ok &&
                      !registry.GetPedCount() && !foreignRegistry.GetPedCount(),
                  "explicit actual factories withdraw SDK rows and retire only their own private native weak cells");
        };
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(GetCurrentThreadId() == engine.thread.load() && runtime.producerThread == producer && producer != GetCurrentThreadId(),
                  "actual setter acquires its row on the genuine engine callback with immutable producer identity");
            Check(firstFactory.Create(1, nullptr, registry) == ClockResult::Ok && firstFactory.Read(firstPed) == ClockResult::Ok && firstPed &&
                      secondFactory.Create(0x101, nullptr, registry) == ClockResult::Ok && secondFactory.Read(secondPed) == ClockResult::Ok && secondPed,
                  "two actual controlled factories register distinct real native ped identities for target assignment");
            row = std::make_unique<CamTargetSetterObserver>(0, registry, runtime.access);
        };
        pulse();
        Check(Refused([&] { row->SetTargetEntity(firstPed); }) && !Host::step,
              "a matching SDK object cannot assign a target outside the natural native callback");
        if (scenario == 0)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto before = bankSnapshot();
                row->SetTargetEntity(nullptr);
                Check(!Host::step && runtime.cameraTargetSetterReceipt.noEffect && !runtime.cameraTargetSetter.IsRetained() &&
                          std::memcmp(before.get(), bankSnapshot().get(), sizeof(Bank)) == 0,
                      "stable null clear is qualified no-effect without native primitives or reference changes");
                row->SetTargetEntity(firstPed);
                Check(Host::step == 2 && Host::order[0] == 'C' && Host::order[1] == 'R' && !Host::unregisterCalls &&
                          fixture.Get<std::uintptr_t>(cell) == pedAddress && engineReferences(pedAddress) == 1 && engineReferences(secondAddress) == 0 &&
                          runtime.cameraTargetSetterReceipt.effectsVerified && !runtime.cameraTargetSetter.IsRetained(),
                      "actual SDK null-to-ped assignment performs one pointer CAS then ordinary non-forced whole native Register");
                preserveCamera(pedAddress);
                Host::Reset();
                const auto sameBank = bankSnapshot();
                row->SetTargetEntity(firstPed);
                Check(!Host::step && runtime.cameraTargetSetterReceipt.noEffect && std::memcmp(sameBank.get(), bankSnapshot().get(), sizeof(Bank)) == 0,
                      "qualified old equals new performs no unregister CAS or register and preserves the complete native graph");
                row->SetTargetEntity(secondPed);
                Check(Host::step == 3 && Host::order == std::array<char, 3>{'U', 'C', 'R'} && fixture.Get<std::uintptr_t>(cell) == secondAddress &&
                          engineReferences(pedAddress) == 0 && engineReferences(secondAddress) == 1 && runtime.cameraTargetSetterReceipt.effectsVerified,
                      "retarget to another registered ped acknowledges whole Unregister then expected-old pointer CAS then whole Register exactly once");
                preserveCamera(secondAddress);
                Host::Reset();
                row->SetTargetEntity(nullptr);
                Check(Host::step == 2 && Host::order[0] == 'U' && Host::order[1] == 'C' && !Host::registerCalls && fixture.Get<std::uintptr_t>(cell) == 0 &&
                          !engineReferences(pedAddress) && !engineReferences(secondAddress),
                      "actual clear unlinks the engine-owned weak registration then clears only the pointer without emulating a null Register");
                preserveCamera(0);
                Host::Reset();
                row->SetTargetEntity(secondPed);
                Check(row->GetTargetEntity() == static_cast<CEntity*>(secondPed),
                      "getter and setter compose through the same held row and actual registered SDK identity");
                const auto setterCalls = Host::step;
                retireFactories();
                Check(Host::step == setterCalls && fixture.Get<std::uintptr_t>(cell) == secondAddress && engineReferences(secondAddress) == 1,
                      "typed wrapper and private-cell retirement leave the engine-owned row target weak reference registered without strong ownership");
            };
            pulse();
            const auto graphBeforeEnd = bankSnapshot();
            const auto expected = fixture.Get<CameraBytes>(P::CameraRva);
            const auto calls = Host::step;
            Check(!service.End() && runtime.cameraRowBorrows.IsRetained(),
                  "successful engine target assignment still retains the inherited SDK row until producer release");
            pulse(true);
            row.reset();
            finish(&expected, graphBeforeEnd.get());
            Check(Host::step == calls, "normal End and typed row destruction never call target Unregister or restore the engine-owned weak target");
            std::puts("Production camera target setter actual SDK owner fixture completed");
            std::puts("Production camera target setter reference effects fixture completed");
            std::puts("Production camera target setter null and no effect fixture completed");
            return;
        }
        if (scenario == 1)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto refusal = [&](CEntity* target, const char* why)
                {
                    const auto before = bankSnapshot();
                    const auto cameraBefore = fixture.Get<CameraBytes>(P::CameraRva);
                    const auto calls = Host::step;
                    Check(Refused([&] { row->SetTargetEntity(target); }) && Host::step == calls && !runtime.cameraTargetSetter.RecoveryPending() &&
                              !runtime.stopping.load() && cameraBefore == fixture.Get<CameraBytes>(P::CameraRva) &&
                              std::memcmp(before.get(), bankSnapshot().get(), sizeof(Bank)) == 0,
                          why);
                };
                NativePedReferenceToken    invalid;
                camera_target_fixture::Ped unregisteredSdk(runtime.access, invalid, registry);
                refusal(&unregisteredSdk, "an unregistered actual SDK ped with invalid token cannot supply native target authority");
                Check(foreignFactory.Create(1, nullptr, foreignRegistry) == ClockResult::Ok && foreignFactory.Read(foreignPed) == ClockResult::Ok,
                      "foreign SDK negative case uses a genuine separately controlled registry owner");
                refusal(foreignPed, "a registered ped from a different SDK registry owner refuses before any native primitive");
                const auto freeHead = fixture.Get<std::uintptr_t>(R::FreeHeadRva);
                fixture.Put(R::FreeHeadRva, std::uintptr_t{0});
                refusal(firstPed, "effectful assignment requires preflight free capacity and cannot enter Register global exhaustion fallback");
                fixture.Put(R::FreeHeadRva, freeHead);
                fixture.Put(cell, std::uintptr_t{1});
                refusal(firstPed, "unknown nonzero old target refuses before any native dereference or effect");
                fixture.Put(cell, std::uintptr_t{0});
                const auto frame = frameCallbackSerial;
                --frameCallbackSerial;
                refusal(firstPed, "stale frame admission refuses actual setter without native effects");
                frameCallbackSerial = frame;
                Host::Reset(1);
                Check(Refused([&] { row->SetTargetEntity(firstPed); }) && Host::pointerCalls == 1 && !Host::unregisterCalls && !Host::registerCalls &&
                          !runtime.cameraTargetSetter.RecoveryPending() && !runtime.stopping.load() && fixture.Get<std::uintptr_t>(cell) == pedAddress,
                      "first real CAS mismatch with no earlier effect preserves the competing pointer and has no retained setter debt");
                // Only this explicit owned competing writer resets its own
                // unregistered test word. Production performs no repair.
                fixture.Put(cell, std::uintptr_t{0});
                Host::Reset();
                row->SetTargetEntity(firstPed);
                Host::Reset();
                row->SetTargetEntity(nullptr);
                // Resolve permanently revokes publication after a lost native
                // relation. Exercise the healthy CAS before destructive faults,
                // and use separate genuine tokens for the two revocation cases.
                const auto                 firstToken = firstFactory.ReadReceipt().reference;
                const auto                 secondToken = secondFactory.ReadReceipt().reference;
                NativePedPoolReader::Value current;
                Check(runtime.access.ResolvePedReference(firstToken, current) == ClockResult::Ok &&
                          runtime.access.ResolvePedReference(secondToken, current) == ClockResult::Ok,
                      "both real controller-issued relations are current before independent destructive refusal cases");
                pool.flags[0] = 2;
                refusal(firstPed, "reused native generation refuses the original controlled setter token");
                pool.flags[0] = 1;
                Check(runtime.access.ResolvePedReference(firstToken, current) == ClockResult::NotInitialized && !runtime.stopping.load(),
                      "restoring owned pool bytes does not revive a controller token whose native relation was revoked");
                const auto original = R::Code[0].bytes[0], changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(R::Code[0].rva, &changed, 1);
                refusal(secondPed, "whole native reference source authority remains mandatory for a separately current token before effects");
                fixture.Patch(R::Code[0].rva, &original, 1);
                Check(runtime.access.ResolvePedReference(secondToken, current) == ClockResult::NotInitialized && !runtime.stopping.load(),
                      "restoring owned source bytes cannot republish the separately revoked native reference relation");
                retireFactories();
            };
            pulse();
            row.reset();
            finish();
            std::puts("Production camera target setter refusal fixture completed");
            return;
        }
        callback.action = [&](NativeGameTimingAccess&)
        {
            row->SetTargetEntity(firstPed);
            Host::Reset(scenario);
            const auto cameraBefore = fixture.Get<CameraBytes>(P::CameraRva);
            const auto bankBefore = bankSnapshot();
            Check(Refused([&] { row->SetTargetEntity(secondPed); }), "an unknown entered target itinerary refuses the actual SDK request");
            const auto receipt = runtime.cameraTargetSetterReceipt;
            Check(receipt.debt && receipt.effectsEntered && runtime.cameraTargetSetter.RecoveryPending() && runtime.stopping.load() && service.IsRetained() &&
                      runtime.frameHooked.load(),
                  "entered or partially accepted target effects retain the first journal and stop new issuance");
            const bool unregistered = scenario >= 3;
            const bool pointerChanged = scenario >= 4;
            const bool registered = scenario == 7;
            Check(Host::unregisterCalls == 1 && Host::pointerCalls == (scenario >= 4 ? 1u : 0u) && Host::registerCalls == (scenario >= 6 ? 1u : 0u) &&
                      fixture.Get<std::uintptr_t>(cell) == (pointerChanged ? secondAddress : pedAddress) &&
                      engineReferences(pedAddress) == (unregistered ? 0u : 1u) && engineReferences(secondAddress) == (registered ? 1u : 0u),
                  "unknown prefix preserves exactly its accepted native weak-reference and pointer effects without invented later phases");
            preserveCamera(pointerChanged ? secondAddress : pedAddress);
            if (scenario == 2)
                Check(cameraBefore == fixture.Get<CameraBytes>(P::CameraRva) && std::memcmp(bankBefore.get(), bankSnapshot().get(), sizeof(Bank)) == 0,
                      "unknown first entry can retain debt even when no native effect was visible");
            if (scenario == 4)
                Check(receipt.unregisterCall.verified && receipt.pointerWrite.returned && !receipt.pointerWrite.accepted,
                      "known CAS rejection cannot erase the already acknowledged Unregister prefix");
            if (scenario == 7)
                Check(!receipt.registerCall.returned && engineReferences(secondAddress) == 1,
                      "complete visible graph after a native exception is not a claimed final acknowledgement");
            // Private adapter cells are unrelated to this engine-owned row
            // registration. Their explicit recovery-capable release remains
            // inside this genuine callback before its lexical authority ends.
            retireFactories();
            Check(runtime.cameraTargetSetter.RecoveryPending(), "private SDK factory drain never consumes the setter's engine-owned effect journal");
        };
        pulse();
        const auto expected = fixture.Get<CameraBytes>(P::CameraRva);
        const auto bankAfter = bankSnapshot();
        const auto receipt = runtime.cameraTargetSetterReceipt;
        const auto calls = Host::step;
        Check(!service.End() && service.IsRetained(), "producer End cannot roll back or discard an uncertain target assignment");
        pulse(true);
        row.reset();
        Check(!runtime.cameraRowBorrows.IsRetained() && !service.End() && runtime.cameraTargetSetter.RecoveryPending(),
              "dropping the inherited row hold cannot retire the independent unresolved target effect journal");
        pulse(true);
        NativeCameraTargetSetterCalls::Resolver resolver{&runtime.access,
                                                         [](void* owner, const NativePedReferenceToken& token, NativePedPoolReader::Value& value) noexcept
                                                         { return static_cast<NativeGameTimingAccess*>(owner)->ResolvePedReference(token, value); }};
        NativeCameraRowBorrow::Token            empty;
        Check(runtime.cameraTargetSetter.Set(fixture.image, runtime.cameraRowBorrows, CameraRowBorrowIdentity(), empty, nullptr, resolver, &CheckTimingOwner,
                                             &CheckApplicationTimingOwner) == ClockResult::RestoreFailure &&
                  runtime.cameraTargetSetterReceipt.rowTokenId == receipt.rowTokenId && runtime.cameraTargetSetterReceipt.oldTarget == receipt.oldTarget &&
                  runtime.cameraTargetSetterReceipt.newTarget == receipt.newTarget && Host::step == calls &&
                  expected == fixture.Get<CameraBytes>(P::CameraRva) && std::memcmp(bankAfter.get(), bankSnapshot().get(), sizeof(Bank)) == 0,
              "repeated native setter End and stopped frames preserve the first journal and graph without replay rollback or replacement");
        engine.Stop();
        const char* names[] = {"success",   "refusals",        "unregister-before", "unregister-after", "cas-rejected-after-unregister",
                               "cas-after", "register-before", "register-after"};
        std::printf("Production camera target setter %s retained debt fixture completed\n", names[scenario]);
        // This fresh process retains its unresolved journal until process
        // retirement. Do not manufacture acknowledgement by restoring bytes.
    }
}
#endif

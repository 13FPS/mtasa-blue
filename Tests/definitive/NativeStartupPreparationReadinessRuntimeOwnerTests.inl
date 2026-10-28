/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualify startup preparation through the real timer-owned runtime
 ****************************************************************************/
namespace StartupPreparationReadinessRuntimeFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using Writes = WorldGroundOwnerFixture::Writes;
    using Creation = NativePedModelCreation::Record;
    using Collision = NativeStartupModelCollision::Record;

    template <class T>
    std::vector<unsigned char> Bytes(const T& value)
    {
        const auto* first = reinterpret_cast<const unsigned char*>(&value);
        return {first, first + sizeof(value)};
    }

    struct Snapshot
    {
        // Readiness must leave both the public debt summaries and the private
        // per-effect journals untouched. These object snapshots are taken on
        // the serialized callback owner, never while another writer runs.
        using State = decltype(std::make_tuple(runtime.generation.load(), runtime.pedModelCreationGeneration.load(), runtime.startupCollisionGeneration.load(),
                                               runtime.pedModelCreationRetained.load(), runtime.startupCollisionRetained.load(),
                                               runtime.pedModelCreationBusy.load(), runtime.startupCollisionBusy.load(), runtime.stopping.load(),
                                               runtime.failure.load(), runtime.active.load(), runtime.restored.load(), runtime.nativeTouched.load()));
        State                                                                state;
        std::vector<std::pair<const Creation*, std::vector<unsigned char>>>  creations;
        std::vector<std::pair<const Collision*, std::vector<unsigned char>>> collisions;
        unsigned                                                             mallocs, frees;

        explicit Snapshot(const Host::State& host) : state(StateOf()), mallocs(host.mallocCalls), frees(host.freeCalls)
        {
            for (const auto& record : runtime.pedModelCreations)
                creations.emplace_back(record.get(), Bytes(*record));
            for (const auto& record : runtime.startupCollisionCopies)
                collisions.emplace_back(record.get(), Bytes(*record));
        }
        static State StateOf()
        {
            return std::make_tuple(runtime.generation.load(), runtime.pedModelCreationGeneration.load(), runtime.startupCollisionGeneration.load(),
                                   runtime.pedModelCreationRetained.load(), runtime.startupCollisionRetained.load(), runtime.pedModelCreationBusy.load(),
                                   runtime.startupCollisionBusy.load(), runtime.stopping.load(), runtime.failure.load(), runtime.active.load(),
                                   runtime.restored.load(), runtime.nativeTouched.load());
        }
        void Unchanged(const Host::State& host) const
        {
            Check(state == StateOf() && mallocs == host.mallocCalls && frees == host.freeCalls && creations.size() == runtime.pedModelCreations.size() &&
                      collisions.size() == runtime.startupCollisionCopies.size(),
                  "readiness preserves runtime generation, debt summaries, Stop state and native allocator counts");
            auto created = runtime.pedModelCreations.begin();
            for (const auto& saved : creations)
            {
                Check(created->get() == saved.first && Bytes(**created) == saved.second,
                      "readiness never appends, retires, reorders or rewrites a genuine creation journal");
                ++created;
            }
            auto copied = runtime.startupCollisionCopies.begin();
            for (const auto& saved : collisions)
            {
                Check(copied->get() == saved.first && Bytes(**copied) == saved.second,
                      "readiness never appends, retires, reorders or rewrites a genuine collision journal");
                ++copied;
            }
        }
    };

    ClockResult UnknownCollision(std::uintptr_t, std::uintptr_t, std::uintptr_t, NativeStartupModelCollision::CompareExchangeReceipt& receipt) noexcept
    {
        receipt.exceptionCode = 0xc0000005;
        return ClockResult::OriginalException;
    }

    void Run(ClockImageFixture& fixture)
    {
        // Reuse the established owned PE, world, heap and canonical-default
        // fixtures. No model allocation or native effect stands in for a
        // successful readiness check, and no future special-ped row is needed.
        Writes     writes{fixture, {}};
        const auto admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph                     graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &Writes::Apply, &writes);
        NativePedClothesFixture::Install(fixture.image, &Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &Writes::Apply, &writes);
        Host::State host;
        host.Reset(fixture.image, &Writes::Apply, &writes);
        Host::active = &host;
        graph.Reset();
        PedMetadataLifecycleFixture::DefaultData defaults;
        defaults.Seed(fixture, writes);
        Writes::Apply(&writes, NativeModelObjectProfile::CollisionConsumerRva, NativeModelObjectProfile::CollisionConsumer.data(),
                      NativeModelObjectProfile::CollisionConsumer.size(), PAGE_EXECUTE_READ);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        ClockReaderTestHost::image = fixture.image;
        const auto closed = CheckTimingOwner();
        Check(closed != ClockResult::Ok, "outside a timer callback no startup preparation authority exists");
        {
            const Snapshot before(host);
            Check(runtime.access.RequireStartupPedModelCreationReady() == closed && runtime.access.RequireStartupSpecialModelRequestReady() == closed &&
                      runtime.access.RequireStartupCollisionCopyReady() == closed,
                  "all actual readiness wrappers forward the absent callback owner without trying native prerequisites");
            before.Unchanged(host);
        }

        WorldGroundOwnerFixture::Callback                                                                 callback;
        NativeClockService                                                                                service;
        unsigned                                                                                          calls = 0;
        alignas(std::uintptr_t) std::array<unsigned char, NativeModelObjectProfile::CollisionPrefixBytes> collisionTarget{};
        callback.action = [&](NativeGameTimingAccess& access)
        {
            ++calls;
            const auto generation = runtime.generation.load();
            Check(CheckTimingOwner() == ClockResult::Ok && generation != 0,
                  "focused fixture entered the actual admitted timer owner before querying optional startup readiness");
            {
                const Snapshot before(host);
                Check(access.RequireStartupPedModelCreationReady() == ClockResult::Ok && access.RequireStartupCollisionCopyReady() == ClockResult::Ok,
                      "real runtime qualifies model-independent creation and collision prerequisites with all future rows absent");
                const auto special = NativeStreamingSpecialModelCalls::CheckReady(fixture.image, &CheckTimingOwner);
                Check(special != ClockResult::Ok && access.RequireStartupSpecialModelRequestReady() == special,
                      "special-request wrapper forwards its real incomplete provider/profile refusal rather than inventing readiness");
                before.Unchanged(host);
            }
            {
                PedModelCreationScope scope;
                Check(scope.held, "fixture owns the real transient creation scope for a bounded contention check");
                const Snapshot before(host);
                Check(access.RequireStartupPedModelCreationReady() == ClockResult::Busy, "actual creation readiness respects existing CPU exclusion");
                before.Unchanged(host);
            }
            {
                StartupCollisionScope scope;
                Check(scope.held, "fixture owns the real transient collision scope for a bounded contention check");
                const Snapshot before(host);
                Check(access.RequireStartupCollisionCopyReady() == ClockResult::Busy, "actual collision readiness respects existing CPU exclusion");
                before.Unchanged(host);
            }
            runtime.pedModelCreationGeneration.store(generation + 1);
            runtime.startupCollisionGeneration.store(generation + 1);
            {
                const Snapshot before(host);
                Check(access.RequireStartupPedModelCreationReady() == ClockResult::OwnerConflict &&
                          access.RequireStartupCollisionCopyReady() == ClockResult::OwnerConflict,
                      "readiness rejects cross-generation retained ownership before native qualification");
                before.Unchanged(host);
            }
            runtime.pedModelCreationGeneration.store(0);
            runtime.startupCollisionGeneration.store(0);
            runtime.bound.store(false);
            {
                const Snapshot before(host);
                Check(access.RequireStartupPedModelCreationReady() == ClockResult::NotInitialized &&
                          access.RequireStartupSpecialModelRequestReady() == ClockResult::NotInitialized &&
                          access.RequireStartupCollisionCopyReady() == ClockResult::NotInitialized,
                      "all readiness channels preserve exact current-owner refusal without stopping or manufacturing debt");
                before.Unchanged(host);
            }
            runtime.bound.store(true);
            const unsigned char changed = 0xcc;
            Writes::Apply(&writes, NativeCustomAnimSequenceStorageProfile::AuthorityMallocRva, &changed, 1, PAGE_EXECUTE_READ);
            {
                const Snapshot before(host);
                Check(NativePedModelCreation::CheckReady(fixture.image, &CheckTimingOwner) == ClockResult::BindingChanged &&
                          access.RequireStartupPedModelCreationReady() == ClockResult::BindingChanged,
                      "creation wrapper propagates actual allocator profile refusal without entering allocation");
                before.Unchanged(host);
            }
            Writes::Apply(&writes, NativeCustomAnimSequenceStorageProfile::AuthorityMallocRva, NativeCustomAnimSequenceStorageProfile::AuthorityMalloc,
                          sizeof(NativeCustomAnimSequenceStorageProfile::AuthorityMalloc), PAGE_EXECUTE_READ);
            Writes::Apply(&writes, NativeModelObjectProfile::CollisionConsumerRva, &changed, 1, PAGE_EXECUTE_READ);
            {
                const Snapshot before(host);
                Check(NativeStartupModelCollision::CheckReady(fixture.image, &CheckTimingOwner) == ClockResult::BindingChanged &&
                          access.RequireStartupCollisionCopyReady() == ClockResult::BindingChanged,
                      "collision wrapper propagates actual metadata-consumer refusal without a model-field exchange");
                before.Unchanged(host);
            }
            Writes::Apply(&writes, NativeModelObjectProfile::CollisionConsumerRva, NativeModelObjectProfile::CollisionConsumer.data(),
                          NativeModelObjectProfile::CollisionConsumer.size(), PAGE_EXECUTE_READ);
            Check(!runtime.stopping.load() && host.mallocCalls == 0 && host.freeCalls == 0 && runtime.pedModelCreations.empty() &&
                      runtime.startupCollisionCopies.empty(),
                  "all readiness success/refusal paths remain read-only and keep the callback alive");
            std::puts("Production startup preparation readiness guards and real refusals preserve owner");

            Check(access.CreateAndPublishPedModelMetadata(PedMetadataLifecycleFixture::Model) == ClockResult::Ok && runtime.pedModelCreations.size() == 1 &&
                      runtime.pedModelCreationGeneration.load() == generation &&
                      runtime.pedModelCreations.front()->GetStage() == NativePedModelCreation::Stage::Published,
                  "one separate real creation effect seeds a genuine same-generation published journal");
            {
                const Snapshot before(host);
                Check(access.RequireStartupPedModelCreationReady() == ClockResult::Ok,
                      "healthy published metadata from this generation does not falsely block startup preparation");
                before.Unchanged(host);
            }
            std::puts("Production startup preparation readiness accepts healthy same-generation publication");

            // Seed an actual uncertain controller receipt, not a forged stage.
            // Calling the controller directly lets this fixture examine the
            // wrapper's debt guard while its timing owner remains active.
            auto                            unknown = std::make_unique<Creation>();
            NativePedModelCreation::Receipt receipt;
            writes.Global(NativeModelBankProfile::BankRva + 18 * 8, std::uintptr_t{0});
            host.mallocResult = ClockResult::OriginalException;
            Check(NativePedModelCreation::CreateAndPublish(fixture.image, *unknown, generation, 18, &CheckTimingOwner, &receipt, nullptr, nullptr,
                                                           &Host::Malloc) == ClockResult::OriginalException &&
                      receipt.allocation.entered && unknown->GetStage() == NativePedModelCreation::Stage::FaultDebt,
                  "real entered allocation uncertainty produces the retained journal used by the guard fixture");
            host.mallocResult = ClockResult::Ok;
            runtime.pedModelCreations.push_back(std::move(unknown));
            {
                const Snapshot before(host);
                Check(access.RequireStartupPedModelCreationReady() == ClockResult::OwnerConflict,
                      "uncertain genuine allocation debt refuses readiness without a new allocation, journal or Stop");
                before.Unchanged(host);
            }

            writes.Global(NativeModelBankProfile::BankRva + 19 * 8, reinterpret_cast<std::uintptr_t>(collisionTarget.data()));
            auto                                 uncertainCopy = std::make_unique<Collision>();
            NativeStartupModelCollision::Receipt copyReceipt;
            Check(NativeStartupModelCollision::Copy(fixture.image, *uncertainCopy, generation, 19, PedMetadataLifecycleFixture::Model, &CheckTimingOwner,
                                                    &copyReceipt, &UnknownCollision) == ClockResult::OriginalException &&
                      copyReceipt.entered && uncertainCopy->HasDebt(),
                  "real entered collision uncertainty produces the retained copy journal used by the guard fixture");
            runtime.startupCollisionCopies.push_back(std::move(uncertainCopy));
            runtime.startupCollisionGeneration.store(generation);
            runtime.startupCollisionRetained.store(true);
            {
                const Snapshot before(host);
                Check(access.RequireStartupCollisionCopyReady() == ClockResult::OwnerConflict,
                      "genuine retained collision debt refuses readiness without replay, rollback, journal change or Stop");
                before.Unchanged(host);
            }
            Check(!runtime.stopping.load() && runtime.active.load() && !runtime.pedModelCreationBusy.load() && !runtime.startupCollisionBusy.load(),
                  "read-only debt refusal leaves the actual runtime active and both temporary CPU scopes released");
            std::puts("Production startup preparation readiness rejects genuine uncertainty without mutating debt");
        };
        BeginOwned(service, fixture, true, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(calls == 1 && !runtime.stopping.load(), "one finite timer callback executed only the new readiness path");
        std::puts("Production startup preparation readiness runtime fixture completed");
        std::printf("Passed %u production clock service checks\n", checks.load());
        std::fflush(stdout);
        // The isolated fixture must not erase genuine published/uncertain
        // records or free their allocations merely to manufacture a clean End.
        // The process owns this synthetic PE and all test backing allocations.
        std::_Exit(0);
    }
}

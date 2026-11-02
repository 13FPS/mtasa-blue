/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain the actual prepared Runtime until explicit producer drain
 ****************************************************************************/
namespace PreparedGameBorrowOwnerFixture
{
    using Token = NativePreparedGameBorrow::Token;
    class Product final : public lifecycle_fixture::NoPresentationGameBase<CGameBuildingPoolDefaultDE>
    {
        using Base = lifecycle_fixture::NoPresentationGameBase<CGameBuildingPoolDefaultDE>;

    public:
        inline static unsigned destroyed = 0;
        inline static DWORD    destroyedOn = 0;
        Product(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& owner, const NativeStreamingFileIdsReader& streaming,
                const NativeGameStateReader& state, const NativeGameConstructorServices& services, bool fail)
            : Base(timing, clock, owner, streaming, state, services)
        {
            if (fail)
                throw std::runtime_error("owned partial product constructor failure");
        }
        ~Product()
        {
            ++destroyed;
            destroyedOn = GetCurrentThreadId();
        }

    protected:
        // These are still-missing test guards, never production providers.
        void RequireResetReady() override { Refuse(); }
        void ResetVehicleModelDummies() override { Refuse(); }
        void RestoreObjectModelGroups() override { Refuse(); }
        void RestoreDefaultObjectGroupProperties() override { Refuse(); }
        void ResetVehicleModelWheelSizes() override { Refuse(); }
        void ResetModelTextureDictionaries() override { Refuse(); }
        void RequireWorldLifecycleReady() override { Refuse(); }
        void DisableWorldIplStreaming() override { Refuse(); }
        void RemoveWorldCovers() override { Refuse(); }
        void RemoveWorldPlants() override { Refuse(); }
        void DestroyWorldStencilShadows() override { Refuse(); }
        void DropWorldBuildingCaches() override { Refuse(); }
        void EnableWorldIplStreaming(MtaGame::CGameWorldPolicy::IplFilter) override { Refuse(); }

    private:
        [[noreturn]] static void Refuse() { throw std::runtime_error("unqualified native lifecycle hook"); }
    };
    static_assert(!std::is_abstract_v<Product> && std::is_abstract_v<CGameBuildingPoolDefaultDE>);
    void Run(ClockImageFixture& fixture)
    {
        // Reuse the genuine completed constructor prerequisite. The graph
        // holder itself performs none of these native initialization writes.
        alignas(std::uintptr_t) std::array<unsigned char, 8> model{};
        ConstructorWeaponFixture::Seed(fixture, model);
        ApplicationFrameContext         observer;
        NativeClockService              preparation;
        NativeWeaponConstructorProgress progress;
        BeginApplicationOwned(preparation, fixture, observer, true, &ConstructorServicesFixture::Prepare, &progress);
        for (std::size_t row = 0; row != NativeWeaponInfoProfile::RowCount; ++row)
        {
            Check(preparation.Renew(runtime.generation.load()), "renew between actual constructor prerequisite frames");
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
        }
        Check(progress.complete && !progress.failed, "prepared graph borrows already completed constructor services");
        FinishApplicationOwned(preparation);
        ConstructorServicesFixture::SeedServices(fixture);
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto oldImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        const auto                    beforeWrites = ClockTestFaults::clockDataWrites.load();
        const auto                    beforeWeapons = ClockTestFaults::weaponExchanges.load();
        NativeGameTimingReader        timing;
        NativeStreamingFileIdsReader  streaming;
        NativeGameStateReader         stateReader;
        CGamePreparedBorrowDE         graph(runtime.access), competitor(runtime.access);
        NativeGameConstructorServices bundle;
        std::unique_ptr<Product>      product;
        Product::destroyed = 0;
        Product::destroyedOn = 0;
        const auto refuses = [](auto&& action)
        {
            try
            {
                action();
            }
            catch (const std::runtime_error&)
            {
                return true;
            }
            return false;
        };
        WorldGroundOwnerFixture::Callback callback;
        unsigned                          phase = 0;
        const auto                        begin = [&](NativeClockService& service)
        {
            ClockServiceRequestV1 request;
            request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
            ClockServiceStateV1 rejected, state;
            // The existing OS PIN lasts for this process. A later rejected
            // Begin must preserve that prior pin, not pretend to undo it.
            const auto pinBefore = static_cast<std::uint32_t>(runtime.modulePinned.load());
            const bool shippingAdmitted = service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            std::printf("Prepared shipping refusal status %u; module pin before %u after %u\n", static_cast<unsigned>(rejected.failure), pinBefore,
                        rejected.modulePinned);
            Check(!shippingAdmitted && rejected.failure == ClockResult::UnsupportedBuild && rejected.modulePinned == pinBefore,
                  "prepared shipping rejection preserves the existing process pin without acquiring another");
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(began && state.bound && state.hooked && runtime.frameHooked.load() && CheckBinding() && !runtime.cameraRowBorrows.IssuanceClosed() &&
                      !runtime.preparedGameBorrow.IssuanceClosed(),
                  "successful genuine Begin keeps both existing camera and new prepared issuance open");
            return state.generation;
        };
        const auto run = [&](NativeClockService& service, std::uint64_t generation, std::function<void(NativeGameTimingAccess&)> action)
        {
            const auto selectedPhase = ++phase;
            callback.action = [&, selectedPhase, action = std::move(action)](NativeGameTimingAccess& owner)
            {
                const auto started = GetTickCount64();
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "prepared acquisition and validation execute on the actual engine callback, with a distinct fixed producer");
                try
                {
                    action(owner);
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Prepared graph phase %u after %llu ms: %s\n", selectedPhase,
                                 static_cast<unsigned long long>(GetTickCount64() - started), error.what());
                    Check(false, "prepared graph diagnostic catches SDK failure before the native noexcept boundary");
                }
                std::printf("Prepared graph phase %u elapsed %llu ms; renewal age %llu ms\n", selectedPhase,
                            static_cast<unsigned long long>(GetTickCount64() - started),
                            static_cast<unsigned long long>(GetTickCount64() - runtime.refreshed.load()));
            };
            engine.Pulse(service, generation);
        };
        NativeClockService service;
        const auto         generation = begin(service);
        const auto         producer = runtime.producerThread;
        run(service, generation,
            [&](NativeGameTimingAccess& owner)
            {
                Check(owner.CaptureConstructorServices(&runtime.clock, bundle) == ClockResult::Ok && bundle.Complete(),
                      "real after-original callback captures the complete prepared identity value without a hold");
                Check(!runtime.preparedGameBorrow.IsRetained(), "copyable constructor identity is not lifetime retention");
            });
        run(service, generation,
            [&](NativeGameTimingAccess&)
            {
                auto stale = bundle;
                ++stale.generation;
                Check(graph.Acquire(stale) == ClockResult::OwnerConflict && !runtime.preparedGameBorrow.IsRetained(),
                      "stale bundle refuses before the native graph slot can be committed");
                Check(graph.Acquire(bundle) == ClockResult::Ok && runtime.preparedGameBorrow.IsRetained() && service.IsRetained(),
                      "actual SDK envelope acquires the sole prepared hold through this qualified Runtime");
            });
        run(service, generation,
            [&](NativeGameTimingAccess&)
            {
                NativeGameConstructorServices output;
                Check(graph.ReadServices(output) == ClockResult::Ok && output.Same(bundle) && competitor.Acquire(bundle) == ClockResult::Busy,
                      "fresh envelope publication preserves the exact bundle and a second graph cannot acquire the occupied slot");
            });
        run(service, generation,
            [&](NativeGameTimingAccess& owner)
            {
                Check(refuses([&] { Product failed(timing, runtime.clock, owner, streaming, stateReader, bundle, true); }) &&
                          runtime.preparedGameBorrow.IsRetained(),
                      "a real partial-product constructor failure leaves its independently retained envelope reachable");
            });
        run(service, generation,
            [&](NativeGameTimingAccess& owner)
            {
                product = std::make_unique<Product>(timing, runtime.clock, owner, streaming, stateReader, bundle, false);
                Check(product->GetClock() == bundle.clock && product->GetStats() == bundle.stats && runtime.preparedGameBorrow.IsRetained(),
                      "the typed partial graph consumes the same prepared clock and service identities under the existing hold");
            });
        std::puts("Production prepared game borrow actual runtime envelope completed");
        NativeGameConstructorServices untouched;
        untouched.generation = 0xfeed;
        const auto before = untouched;
        Check(graph.ReadServices(untouched) != ClockResult::Ok && untouched.Same(before) && refuses([&] { product->GetStats(); }) &&
                  runtime.preparedGameBorrow.IsRetained(),
              "retention grants neither the producer nor a between-callback product a native service lease");
        const auto               firstIdentity = PreparedGameBorrowIdentity();
        NativePreparedGameBorrow foreignLedger;
        Token                    foreignToken;
        Check(foreignLedger.Open(firstIdentity) == ClockResult::Ok &&
                  foreignLedger.AcquireChecked(firstIdentity, bundle, foreignToken, []() noexcept { return ClockResult::Ok; }) == ClockResult::Ok,
              "the comparison token is genuinely CPU-issued by another ledger, never forged into the Runtime slot");
        run(service, generation,
            [&](NativeGameTimingAccess& owner)
            {
                Check(owner.ValidatePreparedGameBorrow(foreignToken) == ClockResult::OwnerConflict && runtime.preparedGameBorrow.IsRetained(),
                      "same-address same-generation identity from another ledger cannot impersonate the actual graph token");
            });
        // Block one genuinely entered callback, then Stop from the unchanged
        // producer. This exercises real in-flight state instead of faking it.
        HANDLE entered = CreateEventW(nullptr, FALSE, FALSE, nullptr), leave = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Check(entered && leave, "create bounded synchronization for the real prepared callback quiescence test");
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(SetEvent(entered) != 0, "signal entry from inside the actual current engine callback");
            Check(WaitForSingleObject(leave, 5000) == WAIT_OBJECT_0, "producer releases the real callback after its stopped drain refusal");
        };
        Check(service.Renew(generation) && SetEvent(engine.run) != 0 && WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0,
              "producer observes an actual in-flight callback before closing publication");
        graph.ClosePublication();
        Check(!service.End() && runtime.stopping.load() && runtime.inFlight.load() && runtime.preparedGameBorrow.IsRetained() &&
                  graph.CheckProducerDrain() == ClockResult::Busy && graph.ReleaseOnProducer() == ClockResult::Busy,
              "first End closes issuance while in-flight preflight and release preserve the retained graph");
        Check(SetEvent(leave) != 0 && WaitForSingleObject(engine.done, 5000) == WAIT_OBJECT_0 && !runtime.inFlight.load(),
              "the actual stopped application hook returns and proves callback quiescence");
        CloseHandle(entered);
        CloseHandle(leave);
        Check(!service.End() && service.IsRetained() && runtime.preparedGameBorrow.IsRetained() && runtime.preparedGameBorrow.IssuanceClosed() &&
                  runtime.cameraRowBorrows.IssuanceClosed(),
              "the graph hold independently keeps End and its native owner retained after callbacks quiesce");
        NativeClockService  contender;
        ClockServiceStateV1 denied;
        Check(!contender.BeginGameTiming({}, denied, &WorldGroundOwnerFixture::Callback::Invoke, &callback) && runtime.generation.load() == generation &&
                  runtime.preparedGameBorrow.IsRetained(),
              "another Begin cannot replace the retained generation or its service addresses");
        untouched = before;
        Check(graph.ReadServices(untouched) == ClockResult::Stopped && untouched.Same(before),
              "closed SDK publication cannot escape a cached prepared bundle after Stop");
        std::thread foreign(
            [&]
            {
                Check(graph.CheckProducerDrain() == ClockResult::WrongThread && graph.ReleaseOnProducer() == ClockResult::WrongThread,
                      "foreign drain and release preserve the actual envelope and hold");
            });
        foreign.join();
        std::puts("Production prepared game borrow retention and refusal completed");
        Check(graph.CheckProducerDrain() == ClockResult::Ok && runtime.preparedGameBorrow.IsRetained() && product,
              "successful producer preflight preserves all products until the host explicitly destroys them");
        product.reset();
        Check(Product::destroyed == 1 && Product::destroyedOn == producer && graph.ReleaseOnProducer() == ClockResult::Ok &&
                  !runtime.preparedGameBorrow.IsRetained(),
              "recorded producer destroys the typed product before explicit CPU hold retirement");
        Check(service.End() && !service.IsRetained() && !runtime.bound.load(),
              "only after producer release may the original retained Runtime complete retirement");
        std::puts("Production prepared game borrow producer drain completed");
        // A later epoch reuses SDK member addresses but must not revive the
        // earlier identity value or the other ledger's old-generation token.
        NativeClockService            later;
        const auto                    nextGeneration = begin(later);
        Token                         issued;
        NativeGameConstructorServices current;
        run(later, nextGeneration,
            [&](NativeGameTimingAccess& owner)
            {
                Check(owner.CaptureConstructorServices(&runtime.clock, current) == ClockResult::Ok && current.generation != bundle.generation &&
                          current.stats == bundle.stats && current.clock == bundle.clock,
                      "new epoch requalifies reused service addresses with a distinct actual generation");
            });
        run(later, nextGeneration,
            [&](NativeGameTimingAccess& owner)
            {
                Check(owner.AcquirePreparedGameBorrow(bundle, issued) == ClockResult::OwnerConflict && !issued &&
                          owner.ValidatePreparedGameBorrow(foreignToken) == ClockResult::OwnerConflict,
                      "old bundle and genuine old-ledger token cannot revive a hold in a same-address later epoch");
                Check(owner.AcquirePreparedGameBorrow(current, issued) == ClockResult::Ok && issued && issued.Owner().generation == nextGeneration &&
                          issued.Id() != foreignToken.Id(),
                      "later graph acquisition issues a never-reused identity only after current bundle validation");
            });
        Token      moved(std::move(issued));
        const auto id = moved.Id();
        Check(!issued && moved && runtime.access.ReleasePreparedGameBorrow(issued) == ClockResult::InvalidArgument &&
                  runtime.access.CheckPreparedGameBorrowDrain(moved) == ClockResult::Busy &&
                  runtime.access.ReleasePreparedGameBorrow(moved) == ClockResult::Busy && moved.Id() == id && runtime.preparedGameBorrow.IsRetained(),
              "move transfers the token only and live or empty-token release cannot consume the actual held slot");
        run(later, nextGeneration,
            [&](NativeGameTimingAccess& owner)
            {
                Check(owner.ValidatePreparedGameBorrow(moved) == ClockResult::Ok,
                      "moved token validates freshly only inside the actual later application callback");
            });
        Check(!later.End() && runtime.access.CheckPreparedGameBorrowDrain(moved) == ClockResult::Ok,
              "later stopped producer may preflight the exact retained raw token without consuming it");
        {
            ApiOperation busy;
            Check(busy.acquired && runtime.access.ReleasePreparedGameBorrow(moved) == ClockResult::Busy && moved.Id() == id &&
                      runtime.preparedGameBorrow.IsRetained(),
                  "release independently rechecks the real API-operation gate after a successful earlier drain preflight");
        }
        std::thread foreignRelease(
            [&]
            {
                Check(runtime.access.ReleasePreparedGameBorrow(moved) == ClockResult::WrongThread && moved.Id() == id,
                      "foreign final release cannot discard or replace a stopped producer's raw token");
            });
        foreignRelease.join();
        Check(runtime.access.ReleasePreparedGameBorrow(foreignToken) == ClockResult::OwnerConflict && foreignToken && runtime.preparedGameBorrow.IsRetained(),
              "stale foreign-ledger release preserves both the foreign token and the genuine current hold");
        Check(runtime.access.ReleasePreparedGameBorrow(moved) == ClockResult::Ok && !moved && !runtime.preparedGameBorrow.IsRetained(),
              "only the exact stopped quiescent producer can retire the later CPU hold");
        Check(later.End() && !later.IsRetained() && !runtime.bound.load() && !runtime.frameHooked.load() && !runtime.inFlight.load() &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && CheckBinding(true),
              "final producer End detaches the native owner after all graph debt is explicitly drained");
        foreignLedger.CloseIssuance();
        Check(foreignLedger.Release(foreignToken, firstIdentity) == ClockResult::Ok && !foreignToken,
              "test-only comparison issuer independently retires its own CPU token without touching the Runtime");
        std::puts("Production prepared game borrow current and stale refusal completed");
        untouched = before;
        Check(graph.ReadServices(untouched) == ClockResult::Stopped && untouched.Same(before) && ClockTestFaults::clockDataWrites.load() == beforeWrites &&
                  ClockTestFaults::weaponExchanges.load() == beforeWeapons && !runtime.nativeTouched.load() && runtime.pedModelCreations.empty() &&
                  runtime.startupCollisionCopies.empty() && runtime.startupMenuCloses.empty() && !runtime.cameraRowBorrows.IsRetained(),
              "graph retention executes no initialization replay, native write, journal cleanup substitute or camera hold");
        std::puts("Production prepared game borrow no effects completed");
        engine.Stop();
        writes.Restore();
        ClockReaderTestHost::image = oldImage;
        std::puts("Production prepared game borrow focused owner fixture completed");
    }
}

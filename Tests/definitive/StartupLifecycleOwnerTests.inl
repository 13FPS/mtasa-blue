/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One composed game borrows real ready owners without fake lifecycle effects
 ****************************************************************************/
namespace StartupLifecycleOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        // Constructor preparation is a distinct prerequisite on real normal
        // frames. It is not replayed by the composed constructor/Initialize.
        alignas(std::uintptr_t) std::array<unsigned char, 8> constructorModel{};
        ConstructorWeaponFixture::Seed(fixture, constructorModel);
        ApplicationFrameContext         observer;
        NativeClockService              constructorService;
        NativeWeaponConstructorProgress progress;
        BeginApplicationOwned(constructorService, fixture, observer, true, &ConstructorServicesFixture::Prepare, &progress);
        for (std::size_t index = 0; index != NativeWeaponInfoProfile::RowCount; ++index)
        {
            Check(constructorService.Renew(runtime.generation.load()), "renew between actual constructor prerequisite frames");
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
        }
        Check(progress.complete && !progress.failed && progress.completedRows == NativeWeaponInfoProfile::RowCount,
              "the composed lifecycle constructor consumes completed native prerequisites");
        FinishApplicationOwned(constructorService);
        ConstructorServicesFixture::SeedServices(fixture);

        // Generated from only the existing owned request/special/readiness
        // setup prefixes. No historical operation/refusal matrix is replayed.
#include "StartupLifecycleSetup.generated.inl"
        (void)seedRow;
        (void)specialSdk;
        class Game final : public lifecycle_fixture::NoPresentationGameBase<CGameStartupLifecycleDE>
        {
            using Base = lifecycle_fixture::NoPresentationGameBase<CGameStartupLifecycleDE>;

        public:
            Game(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& access, const NativeStreamingFileIdsReader& streamingReader,
                 const NativeGameStateReader& stateReader, const NativeGameConstructorServices& bundle)
                : Base(timing, clock, access, streamingReader, stateReader, bundle)
            {
            }
            void             CheckInitializationForFixture() { this->RequireInitializationReady(); }
            bool             RemovedForFixture() const noexcept { return this->IsGameWorldRemoved(); }
            unsigned         resetGuards = 0, worldGuards = 0;
            mutable unsigned unqualifiedEffects = 0;

        protected:
            // These are test guards for still-pure native hooks, never native
            // effect implementations or successful substitutes for services.
            void RequireResetReady() override
            {
                ++resetGuards;
                throw std::runtime_error("missing native Reset lifecycle");
            }
            void RequireWorldLifecycleReady() override
            {
                ++worldGuards;
                throw std::runtime_error("missing native World lifecycle");
            }
            void ResetVehicleModelDummies() override { RefuseEffect(); }
            void RestoreObjectModelGroups() override { RefuseEffect(); }
            void RestoreDefaultObjectGroupProperties() override { RefuseEffect(); }
            void ResetVehicleModelWheelSizes() override { RefuseEffect(); }
            void ResetModelTextureDictionaries() override { RefuseEffect(); }
            int  GetResetBuildingPoolCapacity() const override { RefuseEffect(); }
            void DisableWorldIplStreaming() override { RefuseEffect(); }
            void RemoveWorldCovers() override { RefuseEffect(); }
            void RemoveWorldPlants() override { RefuseEffect(); }
            void DestroyWorldStencilShadows() override { RefuseEffect(); }
            void DropWorldBuildingCaches() override { RefuseEffect(); }
            void EnableWorldIplStreaming(MtaGame::CGameWorldPolicy::IplFilter) override { RefuseEffect(); }

        private:
            [[noreturn]] void RefuseEffect() const
            {
                ++unqualifiedEffects;
                throw std::runtime_error("unqualified lifecycle effect reached");
            }
        };
        static_assert(!std::is_abstract_v<Game> && std::is_convertible_v<Game*, CGame*> && std::is_abstract_v<CGameStartupLifecycleDE>);
        const auto refuses = [](auto&& operation)
        {
            try
            {
                operation();
            }
            catch (const std::runtime_error&)
            {
                return true;
            }
            return false;
        };
        NativeGameTimingReader       timingReader;
        NativeStreamingFileIdsReader streamingReader;
        NativeGameStateReader        stateReader;
        Check(stateReader.Bind() == Status::Ok, "the actual production game-state reader binds only the owned image identity seam");
        NativeGameConstructorServices bundle;
        std::unique_ptr<Game>         game;
        const auto                    handler = +[](CPlayerPed*, bool) { return true; };
        const auto                    effects = [&]
        {
            return std::make_tuple(host.mallocCalls, host.freeCalls, ClockSpecialModelOwnerTest::calls, ClockStreamingRequestOwnerTest::calls,
                                   ClockNamedGarageOwnerTest::stores, ClockStartupMenuInvokeTest::calls, ClockStartupCollisionAtomicTest::calls);
        };
        ClockNamedGarageOwnerTest::stores = ClockNamedGarageOwnerTest::action = ClockNamedGarageOwnerTest::geometryReadAction = 0;
        ClockStartupMenuInvokeTest::calls = ClockStartupCollisionAtomicTest::calls = 0;
        const auto         beforeWeaponWrites = ClockTestFaults::weaponExchanges.load();
        const auto         beforeClockWrites = ClockTestFaults::clockDataWrites.load();
        NativeClockService service;
        unsigned           phase = 0, callbacks = 0;
        specialCall.action = [&](NativeGameTimingAccess& access)
        {
            // Report ordinary SDK admission exceptions before they can escape
            // through the native hook's noexcept boundary and lose the reason.
            const auto  phaseStarted = GetTickCount64();
            const char* step = "capture constructor bundle";
            try
            {
                ++callbacks;
                if (phase == 0)
                {
                    Check(access.CaptureConstructorServices(&runtime.clock, bundle) == ClockResult::Ok && bundle.Complete(),
                          "one actual callback captures the complete current constructor bundle");
                }
                else if (phase == 11)
                {
                    // Full CGame construction freshly recaptures its own
                    // services. It therefore gets a real callback independent
                    // of the caller's preceding complete bundle capture.
                    step = "construct full composed game inside separately renewed callback";
                    game = std::make_unique<Game>(timingReader, runtime.clock, access, streamingReader, stateReader, bundle);
                    CGame& api = *game;
                    Check(static_cast<CGame*>(static_cast<CGameDE*>(game.get())) == &api && api.GetClock() == &runtime.clock,
                          "one unambiguous composed CGame store borrows the actual current clock");
                    api.SetPreWeaponFireHandler(handler);
                    Check(static_cast<WeaponFireHandlerAccess&>(*game).GetPreWeaponFireHandler() == handler && !game->RemovedForFixture(),
                          "the composed graph shares one borrowed handler store and one world state");
                }
                else if (phase >= 3 && phase <= 7)
                {
                    // Each complete selected service freshly checks its own
                    // native source. A separate real callback keeps this CPU
                    // fixture within the unchanged public renewal lease.
                    CGame& api = *game;
                    if (phase == 3)
                    {
                        step = "read prepared stats in a fresh callback";
                        Check(api.GetStats() == bundle.stats, "actual composed stats getter shares the captured service identity");
                    }
                    else if (phase == 4)
                    {
                        step = "read prepared player info in a fresh callback";
                        Check(api.GetPlayerInfo() == bundle.playerInfo, "actual composed player getter shares the captured service identity");
                    }
                    else if (phase == 5)
                    {
                        step = "read prepared complete weather in a fresh callback";
                        Check(api.GetWeather() == bundle.weather, "actual composed complete weather getter shares the captured service identity");
                    }
                    else if (phase == 6)
                    {
                        step = "read prepared ropes in a fresh callback";
                        Check(api.GetRopes() == bundle.ropes, "actual composed ropes getter shares the captured service identity");
                    }
                    else
                    {
                        step = "read prepared weapon manager in a fresh callback";
                        Check(api.GetWeaponStatManager() == bundle.weaponManager, "actual composed weapon manager getter shares the captured service identity");
                    }
                }
                else if (phase == 8)
                {
                    step = "positive inherited startup readiness in a fresh callback";
                    game->CheckInitializationForFixture();
                }
                else if (phase == 9)
                {
                    step = "read selected startup garages in a fresh callback";
                    Check(game->GetGarages() == &runtime.startupGarages, "actual inherited startup garages borrow the qualified owner");
                }
                else if (phase == 10)
                {
                    step = "read selected startup streaming in a fresh callback";
                    Check(game->GetStreaming() == &runtime.startupStreaming && game->GetRenderPreparation().GetNoTexture() == nullptr,
                          "actual inherited startup streaming is borrowed without executing Initialize renderer effects");
                }
                else if (phase == 1)
                {
                    step = "actual Initialize denied prerequisite";
                    Check(refuses([&] { game->Initialize(); }) && !runtime.stopping.load() && !game->RemovedForFixture(),
                          "the actual Initialize body refuses its real denied native prerequisite before its first effect");
                }
                else if (phase == 2)
                {
                    step = "Reset World guards after separately restored readiness";
                    CGame& api = *game;
                    Check(api.GetSystemState() == SystemState::GS_PLAYING_GAME,
                          "Reset's playing gate reads the genuine production state reader and current native state");
                    Check(refuses([&] { api.Reset(); }) && game->resetGuards == 1,
                          "actual composed Reset reaches its still-missing native readiness guard before fire/camera/HUD effects");
                    Check(refuses([&] { api.RemoveGameWorld(); }) && refuses([&] { api.RestoreGameWorld(); }) &&
                              refuses([&] { api.SetBuildingPoolSize(1234); }) && game->worldGuards == 3,
                          "actual composed World operations refuse before IPL/global cleanup or pool effects");
                    Check(!game->unqualifiedEffects && !game->RemovedForFixture() && api.GetClock() == bundle.clock &&
                              static_cast<WeaponFireHandlerAccess&>(*game).GetPreWeaponFireHandler() == handler,
                          "guard refusal preserves the single world/clock/handler state and invokes none of the other native hooks");
                }
                else
                    std::abort();
                std::printf("Composed lifecycle phase %u elapsed %llu ms; renewal age %llu ms\n", phase,
                            static_cast<unsigned long long>(GetTickCount64() - phaseStarted),
                            static_cast<unsigned long long>(GetTickCount64() - runtime.refreshed.load()));
            }
            catch (const std::exception& error)
            {
                std::fprintf(stderr, "Composed lifecycle phase %u at %s after %llu ms (renewal age %llu ms): %s\n", phase, step,
                             static_cast<unsigned long long>(GetTickCount64() - phaseStarted),
                             static_cast<unsigned long long>(GetTickCount64() - runtime.refreshed.load()), error.what());
                Check(false, "an unexpected SDK admission exception stays inside the owned callback diagnostic boundary");
            }
            catch (...)
            {
                std::fprintf(stderr, "Composed lifecycle phase %u at %s: unknown C++ exception\n", phase, step);
                Check(false, "an unexpected C++ exception stays inside the owned callback diagnostic boundary");
            }
        };
        BeginOwned(service, fixture, false, true, true, &SpecialCallback::Invoke, &specialCall);
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew between separate composed owner phases");
            fixture.invokeTimer();
        };
        invoke();
        Check(callbacks == 1 && bundle.Complete() && !game, "capture publishes the one current bundle before a separate construction callback");
        phase = 11;
        invoke();
        Check(callbacks == 2 && game && effects() == std::make_tuple(0u, 0u, 0u, 0u, 0u, 0u, 0u),
              "composed construction executes zero Initialize native effects");
        // The public producer renews only between these genuine callbacks;
        // neither Fresh nor the500 ms native lease is bypassed or extended.
        for (phase = 3; phase <= 10; ++phase)
            invoke();
        Check(callbacks == 10 && effects() == std::make_tuple(0u, 0u, 0u, 0u, 0u, 0u, 0u),
              "fresh selected service getters and positive readiness execute zero Initialize native effects");
        std::puts("Production composed startup lifecycle single CGame clock bundle handler identity completed");
        phase = 1;
        writes.Global(G::CountRva, N::CertifiedPublishedCount - 1);
        invoke();
        writes.Global(G::CountRva, N::CertifiedPublishedCount);
        std::puts("Production composed startup lifecycle actual Initialize readiness and preeffect refusal completed");
        phase = 8;
        invoke();
        phase = 2;
        invoke();
        std::puts("Production composed startup lifecycle Reset World guards refused before effects");
        Check(callbacks == 13 && effects() == std::make_tuple(0u, 0u, 0u, 0u, 0u, 0u, 0u) && runtime.pedModelCreations.empty() &&
                  runtime.startupCollisionCopies.empty() && runtime.startupMenuCloses.empty() &&
                  ClockTestFaults::weaponExchanges.load() == beforeWeaponWrites && ClockTestFaults::clockDataWrites.load() == beforeClockWrites &&
                  game->GetRenderPreparation().GetNoTexture() == nullptr && !runtime.nativeTouched.load() && !runtime.stopping.load(),
              "the focused lifecycle scope creates no startup journal, request, allocation, garage/menu/collision effect or constructor replay");
        Check(refuses([&] { game->CheckInitializationForFixture(); }) && refuses([&] { game->GetStats(); }) && !runtime.stopping.load(),
              "retained composed products grant no native readiness or service access between callbacks");
        game.reset();
        ConstructorServicesFixture::Retire(service, fixture);
        stateReader.Reset();
        (void)retireSpecial;
        Allocator::active = previousAllocator;
        writes.Restore();
        std::puts("Production composed startup lifecycle native effects absent and clean retirement completed");
    }  // Existing joint-readiness setup scope.
}  // Existing special-model setup scope.
ClockReaderTestHost::image = ClockStreamingRequestOwnerTest::image = ClockStreamingRequestOwnerTest::root = 0;
ClockStreamingRequestOwnerTest::changeRoot = false;
for (auto item = saved.rbegin(); item != saved.rend(); ++item)
    fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
std::puts("Production composed startup lifecycle actual owner fixture completed");
}
}

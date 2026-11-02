/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the genuine original default without building pool effects
 ****************************************************************************/
namespace BuildingPoolDefaultOwnerFixture
{
    namespace P = NativeBuildingPoolDefaultProfile;
    using Writes = WorldGroundOwnerFixture::Writes;
    unsigned              ownerChecks = 0;
    std::function<void()> afterOwner;
    ClockResult           MutatingOwner() noexcept
    {
        const auto result = CheckTimingOwner();
        if (result == ClockResult::Ok && ++ownerChecks == 2 && afterOwner)
        {
            auto action = std::move(afterOwner);
            afterOwner = {};
            action();
        }
        return result;
    }
    class Game final : public lifecycle_fixture::NoPresentationGameBase<CGameBuildingPoolDefaultDE>
    {
        using Base = lifecycle_fixture::NoPresentationGameBase<CGameBuildingPoolDefaultDE>;

    public:
        Game(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& owner, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state)
            : Base(timing, clock, owner, streaming, state)
        {
        }
        int DefaultForFixture() const { return this->GetResetBuildingPoolCapacity(); }

    protected:
        // Only the production capacity hook is implemented by this leaf's
        // base. The other thirteen hooks remain rejecting test guards.
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
    static_assert(!std::is_abstract_v<Game> && std::is_abstract_v<CGameBuildingPoolDefaultDE>);
    class Legacy final : public NativeGameTimingAccess
    {
    public:
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    };
    void Run(ClockImageFixture& fixture)
    {
        constexpr std::uint32_t sentinel = 0xdeadbeef;
        Legacy                  legacy;
        std::uint32_t           value = sentinel;
        Check(legacy.ReadDefaultBuildingPoolCapacity(value) == ClockResult::NotInitialized && value == sentinel,
              "older native owners refuse the new scalar without manufacturing a default or changing output");
        fixture.AdmitApplicationPins();
        Writes     writes{fixture, {}};
        const auto apply = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        { Writes::Apply(&writes, rva, bytes, size, protection); };
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
        writes.Global(P::VtableRva, fixture.image + P::DestructorRva, PAGE_READONLY);
        fixture.Seed();
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()), &Writes::Apply, &writes);
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        // A synthetic current header is deliberately different from the
        // original. It creates no native pool, building object or allocation.
        alignas(std::uintptr_t) std::array<unsigned char, 0x20> liveHeader{};
        liveHeader.fill(0xa5);
        NativeStreamingLoadAllFixture::Graph::Put(liveHeader.data(), 0x10, std::uint32_t{23001});
        writes.Global(P::PoolRootRva, reinterpret_cast<std::uintptr_t>(liveHeader.data()));
        const auto unchangedHeader = liveHeader;
        const auto oldImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        stateReader;
        auto                         game = std::make_unique<Game>(timing, runtime.clock, runtime.access, streaming, stateReader);
        const auto                   refuses = [](auto&& action)
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
        NativeClockService                service;
        ClockServiceRequestV1             request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.modulePinned,
              "the new scalar preserves the unchanged public shipping executable refusal");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && state.hooked && runtime.frameHooked.load() && CheckBinding(),
              "default lookup runs through the real retained after-original application owner");
        const auto generation = state.generation, serial = runtime.admissionSerial.load();
        const auto producer = runtime.producerThread;
        const auto beforeWrites = ClockTestFaults::clockDataWrites.load();
        const auto beforeWeapons = ClockTestFaults::weaponExchanges.load();
        const auto run = [&](std::function<void(NativeGameTimingAccess&)> action)
        {
            callback.action = [&, action = std::move(action)](NativeGameTimingAccess& owner)
            {
                Check(GetCurrentThreadId() == engine.thread.load() && runtime.producerThread == producer && producer != GetCurrentThreadId(),
                      "real engine callbacks preserve the distinct immutable producer identity");
                try
                {
                    action(owner);
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Building default owner callback: %s\n", error.what());
                    Check(false, "unexpected SDK refusal remains inside the actual callback diagnostic boundary");
                }
            };
            engine.Pulse(service, generation);
        };
        value = sentinel;
        Check(runtime.access.ReadDefaultBuildingPoolCapacity(value) != ClockResult::Ok && value == sentinel &&
                  refuses([&] { static_cast<const Game&>(*game).DefaultForFixture(); }),
              "the producer cannot borrow the engine's callback or turn refused const access into a literal fallback");
        run(
            [&](NativeGameTimingAccess& owner)
            {
                value = sentinel;
                Check(owner.ReadDefaultBuildingPoolCapacity(value) == ClockResult::Ok && value == 14000 && liveHeader == unchangedHeader,
                      "the real current runtime reports original14000 independently of deliberately different live23001");
            });
        run(
            [&](NativeGameTimingAccess&)
            {
                Check(static_cast<const Game&>(*game).DefaultForFixture() == 14000 && liveHeader == unchangedHeader,
                      "the actual production const CGame hook uses the same admitted native owner and original scalar");
            });
        std::puts("Production building pool default actual const SDK owner completed");
        NativeStreamingLoadAllFixture::Graph::Put(liveHeader.data(), 0x10, std::uint32_t{17});
        run(
            [&](NativeGameTimingAccess&)
            {
                Check(static_cast<const Game&>(*game).DefaultForFixture() == 14000, "a second live resize value cannot replace or cache the original default");
            });
        liveHeader = unchangedHeader;
        std::puts("Production building pool default original versus live capacity completed");
        const auto& sourcePin = P::Pins[4];
        const auto  changed = static_cast<unsigned char>(sourcePin.bytes[0] ^ 1);
        fixture.Patch(sourcePin.rva, &changed, 1);
        run(
            [&](NativeGameTimingAccess& owner)
            {
                value = sentinel;
                Check(owner.ReadDefaultBuildingPoolCapacity(value) == ClockResult::UnsupportedBuild && value == sentinel &&
                          refuses([&] { static_cast<const Game&>(*game).DefaultForFixture(); }) && !runtime.stopping.load(),
                      "changed typed BUILDING source preserves output and the const hook refuses without stopping an unrelated owner");
            });
        fixture.Patch(sourcePin.rva, sourcePin.bytes, 1);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        run(
            [&](NativeGameTimingAccess& owner)
            {
                value = sentinel;
                Check(owner.ReadDefaultBuildingPoolCapacity(value) == ClockResult::NotInitialized && value == sentinel,
                      "nonplaying current world refuses the original scalar without output publication");
            });
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        std::puts("Production building pool default refusal and output preservation completed");
        run(
            [&](NativeGameTimingAccess&)
            {
                ownerChecks = 0;
                afterOwner = [&] { fixture.Patch(sourcePin.rva, &changed, 1); };
                value = sentinel;
                const auto result = NativeBuildingPoolDefaultMemory::Read(fixture.image, &MutatingOwner, &CheckApplicationTimingOwner, value);
                fixture.Patch(sourcePin.rva, sourcePin.bytes, 1);
                Check(result == ClockResult::BindingChanged && ownerChecks == 2 && !afterOwner && value == sentinel,
                      "source changes after the scalar sample are detected by the actual helper's post-read bookend");
            });
        run(
            [&](NativeGameTimingAccess&)
            {
                ownerChecks = 0;
                afterOwner = [&] { NativeStreamingLoadAllFixture::Graph::Put(graph.items.data(), 3 * 24 + 0x10, std::uint32_t{104}); };
                value = sentinel;
                const auto result = NativeBuildingPoolDefaultMemory::Read(fixture.image, &MutatingOwner, &CheckApplicationTimingOwner, value);
                NativeStreamingLoadAllFixture::Graph::Put(graph.items.data(), 3 * 24 + 0x10, std::uint32_t{103});
                Check(result == ClockResult::BindingChanged && ownerChecks == 2 && !afterOwner && value == sentinel,
                      "world member generation changes after sampling are rejected before default publication");
            });
        std::puts("Production building pool default source and world bookends completed");
        run(
            [&](NativeGameTimingAccess& owner)
            {
                value = sentinel;
                Check(owner.ReadDefaultBuildingPoolCapacity(value) == ClockResult::Ok && value == 14000 && liveHeader == unchangedHeader &&
                          runtime.generation.load() == generation && runtime.admissionSerial.load() == serial && !runtime.nativeTouched.load() &&
                          runtime.pedModelCreations.empty() && runtime.startupCollisionCopies.empty() && runtime.startupMenuCloses.empty() &&
                          ClockTestFaults::clockDataWrites.load() == beforeWrites && ClockTestFaults::weaponExchanges.load() == beforeWeapons &&
                          !runtime.stopping.load() && !runtime.cameraRowBorrows.IsRetained(),
                      "restored source remains usable and creates no writes, lifecycle journals, row holds or native pool effects");
            });
        game.reset();
        if (!service.End())
        {
            engine.Pulse(service, generation, true);
            Check(service.End(), "only the actual engine callback acknowledges any pending readonly retirement");
        }
        engine.Stop();
        value = sentinel;
        Check(!service.IsRetained() && !runtime.bound.load() && !runtime.frameHooked.load() && !runtime.inFlight.load() &&
                  runtime.access.ReadDefaultBuildingPoolCapacity(value) != ClockResult::Ok && value == sentinel &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && CheckBinding(true),
              "readonly default lookup detaches cleanly and retired authority cannot publish a scalar");
        afterOwner = {};
        writes.Restore();
        ClockReaderTestHost::image = oldImage;
        std::puts("Production building pool default no effects and retirement completed");
        std::puts("Production building pool default focused owner fixture completed");
    }
}

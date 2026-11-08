/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real Runtime/model-wheel CAS and lifetime gates on owned image data
 ****************************************************************************/
namespace ClockWorldIplOwnerTest
{
    namespace P = mta::game_de::NativeWorldIplProfile;
    Result Remove(Calls::RemoveFunction function, std::int32_t index, std::uint32_t& exception) noexcept
    {
        exception = 0;
        ++removeCalls;
        if (!current || index < 1 || index > 2 || reinterpret_cast<std::uintptr_t>(function) != image + P::RemoveAndIgnoreRva)
        {
            invalidInvocation = true;
            return Result::InvalidArgument;
        }
        // This is the explicit native-original double for the already-empty
        // fixture IPL. The actual Runtime, admission, snapshots and controller
        // still execute; no mapped game removal/allocator body is invoked.
        NativeWorldIplFixture::Put(current->ipl.Row(static_cast<std::size_t>(index)), P::DisabledOffset, std::uint16_t{1});
        return Result::Ok;
    }
    Result Add(Calls::AddFunction function, void* node, void* item, const void* rect, std::uint32_t& exception) noexcept
    {
        exception = 0;
        ++addCalls;
        if (!current || reinterpret_cast<std::uintptr_t>(function) != image + P::QuadAddRva || reinterpret_cast<std::uintptr_t>(node) != current->quad.Row(0) ||
            reinterpret_cast<std::uintptr_t>(item) != current->ipl.Row(1) || rect != item || addCalls != 1)
        {
            invalidInvocation = true;
            return Result::InvalidArgument;
        }
        current->List(1);
        return Result::Ok;
    }
    Result Clear(Calls::ClearFunction function, void* head, std::uint32_t& exception) noexcept
    {
        exception = 0;
        ++clearCalls;
        if (!current || reinterpret_cast<std::uintptr_t>(function) != image + P::SingleClearRva ||
            reinterpret_cast<std::uintptr_t>(head) != current->quad.Row(0) + P::QuadListOffset)
        {
            invalidInvocation = true;
            return Result::InvalidArgument;
        }
        current->List(0);
        return Result::Ok;
    }
}

namespace ModelWheelResetOwnerFixture
{
    namespace W = NativeModelWheelProfile;
    namespace Request = NativeStreamingRequestProfile;
    using Sizes = NativeModelWheelSizes;
    using Group = NativeModelWheelGroup;

    class Game final : public lifecycle_fixture::NoPresentationGameBase<CGameWorldIplDE>
    {
        using Base = lifecycle_fixture::NoPresentationGameBase<CGameWorldIplDE>;

    public:
        Game(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& owner, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state)
            : Base(timing, clock, owner, streaming, state)
        {
        }
        void ResetWheelsForFixture() { this->ResetVehicleModelWheelSizes(); }
        void RestoreIplForFixture() { this->EnableWorldIplStreaming(&MtaGame::CGameWorldPolicy::ShouldRestoreIpl); }

    protected:
        // The real wheel/IPL overrides come from the production partial base.
        // Other world effects cannot become successful fixture substitutes.
        void RequireResetReady() override { Refuse(); }
        void ResetVehicleModelDummies() override { Refuse(); }
        void RestoreObjectModelGroups() override { Refuse(); }
        void RestoreDefaultObjectGroupProperties() override { Refuse(); }
        void ResetModelTextureDictionaries() override { Refuse(); }
        void RequireWorldLifecycleReady() override { Refuse(); }
        void RemoveWorldCovers() override { Refuse(); }
        void RemoveWorldPlants() override { Refuse(); }
        void DestroyWorldStencilShadows() override { Refuse(); }
        void DropWorldBuildingCaches() override { Refuse(); }

    private:
        [[noreturn]] static void Refuse() { throw std::runtime_error("unqualified wheel/IPL lifecycle dependency"); }
    };
    static_assert(!std::is_abstract_v<Game> && std::is_abstract_v<CGameWorldIplDE>);

    std::array<std::uint32_t, 2> Bits(const Sizes& value)
    {
        std::array<std::uint32_t, 2> result{};
        static_assert(sizeof(value) == sizeof(result));
        std::memcpy(result.data(), &value, sizeof(value));
        return result;
    }
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite wheel/world owner scenario is selected");
        const bool retainedCase = scenario == 1, worldRetainedCase = scenario == 2;
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* value, std::size_t bytes, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, value, bytes, protection); };
        const auto code = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        const auto typed = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        code(NativePedPoolProfile::Code);
        code(NativeModelBankProfile::Code);
        code(Request::Code);
        code(NativeStreamingLoadAllProfile::Code);
        typed(NativeStartupOwnerProfile::pins);
        typed(NativeStartupWorldProfile::pins);
        typed(NativeGameStateProfile::pins);
        code(NativeUObjectMembershipProfile::pins);
        code(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        typed(W::Pins);

        // The same frame receiver carries the real typed world/provider graph.
        // The model provider reuses an independently qualified member already
        // in that graph, preserving its native UObject membership and serial.
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        std::vector<unsigned char>           providerRows(10 * Request::ProviderRowStride);
        const auto                           rows = reinterpret_cast<std::uintptr_t>(providerRows.data());
        const auto                           provider = graph.Game();
        const auto                           store = [](std::uintptr_t object, std::size_t offset, const auto& value)
        { std::memcpy(reinterpret_cast<void*>(object + offset), &value, sizeof(value)); };
        store(reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()), Request::ProviderOffset, provider);
        store(provider, Request::ProviderRowsOffset, rows);
        store(provider, Request::ProviderCountOffset, std::int32_t{10});

        // Match the proven fixed native vehicle pool, rather than blessing an
        // arbitrary heap object whose type virtual happens to return six.
        std::vector<unsigned char> pool(W::PoolCapacity * W::PoolStride);
        apply(W::PoolRva, pool.data(), pool.size(), PAGE_READWRITE);
        writes.Global(W::PoolCountRva, std::int32_t{2});
        const auto                                          modelAt = fixture.image + W::PoolRva;
        const auto                                          secondAt = modelAt + W::PoolStride;
        std::array<std::uintptr_t, W::VtableMethods.size()> table{};
        for (std::size_t index = 0; index < table.size(); ++index)
            table[index] = fixture.image + W::VtableMethods[index];
        apply(W::VtableRva, table.data(), sizeof(table), PAGE_READONLY);
        store(modelAt, 0, fixture.image + W::VtableRva);
        store(secondAt, 0, fixture.image + W::VtableRva);
        const Sizes original{4.0f, 6.0f}, secondOriginal{5.0f, 10.0f};
        store(modelAt, W::FrontOffset, original);
        store(secondAt, W::FrontOffset, secondOriginal);
        std::vector<std::uintptr_t> bank(NativeModelBankProfile::ModelCount);
        bank[3] = modelAt;
        bank[4] = secondAt;
        apply(NativeModelBankProfile::BankRva, bank.data(), NativeModelBankProfile::BankBytes, PAGE_READWRITE);
        NativeWorldIplFixture::State world(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Check(world.Ready(), "actual IPL memory fixture owns its three native pool allocations");
        world.List(1);
        world.Stream(1, 1);
        ClockWorldIplOwnerTest::current = &world;
        ClockWorldIplOwnerTest::image = fixture.image;
        ClockWorldIplOwnerTest::removeCalls = ClockWorldIplOwnerTest::addCalls = ClockWorldIplOwnerTest::clearCalls = 0;
        ClockWorldIplOwnerTest::invalidInvocation = false;
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockModelWheelOwnerTest::model = modelAt;
        ClockModelWheelOwnerTest::providerCount = provider + Request::ProviderCountOffset;
        ClockModelWheelOwnerTest::failAfterCommitted = false;
        ClockModelWheelOwnerTest::injected = false;

        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "wheel owner preserves normal shipping rejection of the test executable");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.bound && runtime.frameHooked.load(), "wheel scope enters the real existing post-application Runtime");
        const auto                   generation = admitted.generation;
        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        stateReader;
        auto                         game = std::make_unique<Game>(timing, runtime.clock, runtime.access, streaming, stateReader);
        const auto                   pulse = [&](const auto& action, bool expectActive = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& access)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "wheel operations execute on the actual distinct engine callback thread");
                action(access);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!expectActive || !runtime.stopping.load()), "one requested wheel action uses one admitted owner callback");
        };
        const auto raw = [&]
        {
            Sizes value;
            std::memcpy(&value, reinterpret_cast<void*>(modelAt + W::FrontOffset), sizeof(value));
            return Bits(value);
        };
        Sizes      sentinel{123.0f, 456.0f};
        const auto before = Bits(sentinel);
        Check(runtime.access.ReadModelWheelSizes(3, sentinel) != ClockResult::Ok && Bits(sentinel) == before && !runtime.modelWheels.Pending(),
              "producer refusal preserves output and cannot create a wheel-default cache");
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                Sizes value;
                Check(access.ReadModelWheelSizes(3, value) == ClockResult::Ok && Bits(value) == Bits(original) && !runtime.modelWheels.Pending(),
                      "real native memory/domain reads the qualified vehicle pair without defaults or effects");
                NativeWorldIplMemory::Snapshot observed;
                Check(NativeWorldIplMemory::Capture(fixture.image, &CheckTimingOwner, &CheckApplicationTimingOwner, observed) == ClockResult::Ok &&
                          !observed.rows.empty(),
                      "real Runtime IPL snapshot qualifies the owned pools/tree under this same world owner");
                Check(access.CheckWorldIplStreamingReady() == ClockResult::Ok && access.DisableWorldIplStreaming() == ClockResult::UnsupportedBuild &&
                          !runtime.worldIpl.Retained() && !ClockWorldIplOwnerTest::removeCalls && !ClockWorldIplOwnerTest::clearCalls &&
                          *reinterpret_cast<unsigned char*>(world.ipl.Row(1) + NativeWorldIplProfile::DisabledOffset) == 0 &&
                          *reinterpret_cast<unsigned char*>(world.ipl.Row(1) + NativeWorldIplProfile::DisabledOffset + 1) == 0x5a,
                      "actual NativeAccess refuses every loaded IPL before any flag/removal/list prefix");
            });
        world.Stream(1, 0);
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                Check(access.DisableWorldIplStreaming() == ClockResult::Ok && runtime.worldIpl.Retained() && !runtime.modelWheels.Pending() &&
                          ClockWorldIplOwnerTest::removeCalls == 2 && ClockWorldIplOwnerTest::clearCalls == 1 && !ClockWorldIplOwnerTest::invalidInvocation &&
                          runtime.worldIpl.End(generation) == ClockResult::Busy,
                      "actual world owner retains accepted disabled state and will not substitute End for the SDK restoration itinerary");
            });
        if (worldRetainedCase)
        {
            game.reset();
            Check(!service.End(generation), "known disabled world state alone refuses the actual producer End");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.worldIpl.Retained() && !runtime.modelWheels.Pending() &&
                      ClockWorldIplOwnerTest::removeCalls == 2 && !ClockWorldIplOwnerTest::addCalls && ClockWorldIplOwnerTest::clearCalls == 1,
                  "stopped-frame cleanup/new Begin preserve independent world intent and never automatically enable or replay it");
            engine.Stop();
            std::puts("Production world IPL Runtime retained completed");
            return;
        }
        pulse(
            [&](NativeGameTimingAccess&)
            {
                game->RestoreIplForFixture();
                Check(!runtime.worldIpl.Retained() && ClockWorldIplOwnerTest::addCalls == 1 && !ClockWorldIplOwnerTest::invalidInvocation &&
                          *reinterpret_cast<unsigned char*>(world.ipl.Row(1) + NativeWorldIplProfile::DisabledOffset) == 0 &&
                          *reinterpret_cast<unsigned char*>(world.ipl.Row(2) + NativeWorldIplProfile::DisabledOffset) == 1 &&
                          *reinterpret_cast<unsigned char*>(world.ipl.Row(0) + NativeWorldIplProfile::DisabledOffset) == 0,
                      "actual CGame IPL hook uses copied MTA name filtering, preserves generic/barrier policy and explicitly clears known debt");
            });
        store(provider, Request::ProviderCountOffset, std::int32_t{2});
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                auto value = sentinel;
                Check(access.ReadModelWheelSizes(3, value) != ClockResult::Ok && Bits(value) == before && !runtime.modelWheels.Pending(),
                      "current selected-model provider bounds refuse before output or default mutation");
            });
        store(provider, Request::ProviderCountOffset, std::int32_t{10});

        if (retainedCase)
        {
            const Sizes desired{11.0f, 11.0f};
            std::memcpy(&ClockModelWheelOwnerTest::committedPair, &desired, sizeof(desired));
            ClockModelWheelOwnerTest::failAfterCommitted = true;
            pulse(
                [&](NativeGameTimingAccess& access)
                {
                    Check(access.SetModelWheelSize(3, Group::All, 11.0f) != ClockResult::Ok && ClockModelWheelOwnerTest::injected &&
                              runtime.modelWheels.Pending() && runtime.modelWheels.Uncertain() && raw() == Bits(desired),
                          "post-CAS source change retains the actual entered write and its original defaults");
                },
                false);
            ClockModelWheelOwnerTest::failAfterCommitted = false;
            game.reset();
            Check(runtime.stopping.load() && !service.End(generation), "uncertain wheel owner stops and refuses producer End");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.modelWheels.Pending() && runtime.modelWheels.Uncertain() && raw() == Bits(desired),
                  "stopped-frame cleanup and another Begin neither replay nor discard the actual uncertain wheel tuple");
            engine.Stop();
            std::puts("Production model wheel reset owner retained completed");
            return;
        }

        pulse(
            [&](NativeGameTimingAccess& access)
            {
                Check(access.SetModelWheelSize(3, Group::Front, 7.0f) == ClockResult::Ok && runtime.modelWheels.Pending() && raw() == Bits(Sizes{7.0f, 6.0f}),
                      "first real tuple CAS saves both original wheel values");
                Check(access.SetModelWheelSize(3, Group::Rear, 8.0f) == ClockResult::Ok && raw() == Bits(Sizes{7.0f, 8.0f}),
                      "repeated model mutation preserves the first default pair and the unchanged component");
                Check(access.SetModelWheelSize(4, static_cast<Group>(2), 99.0f) == ClockResult::Ok && runtime.modelWheels.Pending(),
                      "classic ignored group still captures qualified vehicle defaults before its no-write switch");
                Check(access.ResetModelWheelSizes(4, nullptr) == ClockResult::Ok, "one-model reset drains its exact default entry");
                game->ResetWheelsForFixture();
                Check(!runtime.modelWheels.Pending() && !runtime.modelWheels.Uncertain() && raw() == Bits(original),
                      "actual CGame Reset wheel hook restores its setters' first tuple through the identical owner domain");
            });
        Sizes       foreign = sentinel;
        ClockResult foreignResult = ClockResult::Ok;
        std::thread foreignThread([&] { foreignResult = runtime.access.ReadModelWheelSizes(3, foreign); });
        foreignThread.join();
        Check(foreignResult != ClockResult::Ok && Bits(foreign) == before && !runtime.modelWheels.Pending(),
              "foreign thread has no native read scope and creates no retained intent");
        game.reset();
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.modelWheels.CanDiscard(), "normal explicit bulk reset permits authoritative owner End");
        NativeClockService  nextOwner;
        ClockServiceStateV1 nextState;
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool nextBegan = nextOwner.BeginGameTiming(request, nextState, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(nextBegan && nextState.generation != generation && !runtime.modelWheels.Pending(),
              "a fully reset real wheel domain permits a distinct next owner generation");
        engine.Pulse(nextOwner, nextState.generation);
        if (!nextOwner.End(nextState.generation))
            engine.Pulse(nextOwner, nextState.generation, true);
        Check(nextOwner.End(nextState.generation), "the distinct no-effect owner also retires normally");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        ClockModelWheelOwnerTest::model = ClockModelWheelOwnerTest::providerCount = 0;
        ClockWorldIplOwnerTest::current = nullptr;
        writes.Restore();
        std::puts("Production model wheel reset owner healthy completed");
    }
}

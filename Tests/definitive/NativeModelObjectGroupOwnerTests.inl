/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual object-group memory/domain/Runtime on owned allocated rows
 ****************************************************************************/
namespace ModelObjectGroupOwnerFixture
{
    namespace P = NativeModelObjectGroupProfile;
    namespace Request = NativeStreamingRequestProfile;
    class Game final : public lifecycle_fixture::NoPresentationGameBase<CGameObjectGroupResetDE>
    {
        using Base = lifecycle_fixture::NoPresentationGameBase<CGameObjectGroupResetDE>;

    public:
        Game(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& owner, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state)
            : Base(timing, clock, owner, streaming, state)
        {
        }
        void RestoreGroupsForFixture() { this->RestoreObjectModelGroups(); }

    protected:
        // Only the production group reset effect executes. Full Reset/world
        // readiness stays unqualified rather than becoming a fixture success.
        void RequireResetReady() override { Refuse(); }
        void ResetVehicleModelDummies() override { Refuse(); }
        void RestoreDefaultObjectGroupProperties() override { Refuse(); }
        void ResetModelTextureDictionaries() override { Refuse(); }
        void RequireWorldLifecycleReady() override { Refuse(); }
        void RemoveWorldCovers() override { Refuse(); }
        void RemoveWorldPlants() override { Refuse(); }
        void DestroyWorldStencilShadows() override { Refuse(); }
        void DropWorldBuildingCaches() override { Refuse(); }

    private:
        [[noreturn]] static void Refuse() { throw std::runtime_error("unqualified object-group lifecycle dependency"); }
    };
    static_assert(!std::is_abstract_v<Game> && std::is_abstract_v<CGameObjectGroupResetDE>);

    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite object-group Runtime lifetime is selected");
        const bool retainedCase = scenario == 1, unknownCase = scenario == 2;
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
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        std::vector<unsigned char>           providerRows(10 * Request::ProviderRowStride);
        const auto                           provider = graph.Game(), rows = reinterpret_cast<std::uintptr_t>(providerRows.data());
        const auto                           store = [](std::uintptr_t object, std::size_t offset, const auto& value)
        { std::memcpy(reinterpret_cast<void*>(object + offset), &value, sizeof(value)); };
        store(reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()), Request::ProviderOffset, provider);
        store(provider, Request::ProviderRowsOffset, rows);
        store(provider, Request::ProviderCountOffset, std::int32_t{10});
        std::vector<std::uintptr_t> bank(NativeModelBankProfile::ModelCount);
        apply(NativeModelBankProfile::BankRva, bank.data(), NativeModelBankProfile::BankBytes, PAGE_READWRITE);
        NativeModelObjectGroupFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes, 2, 3);
        const auto modelAt = NativeModelObjectGroupFixture::Row(fixture.image), secondAt = NativeModelObjectGroupFixture::Row(fixture.image, 1);
        store(secondAt, P::FieldOffset, std::uint16_t{42});
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockObjectGroupOwnerTest::models = {modelAt, secondAt};
        ClockObjectGroupOwnerTest::providerCount = provider + Request::ProviderCountOffset;
        ClockObjectGroupOwnerTest::failAfterCommitted = ClockObjectGroupOwnerTest::failPrefix = false;
        ClockObjectGroupOwnerTest::injected = false;
        ClockObjectGroupOwnerTest::restoredAddress = 0;

        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "object-group owner preserves normal shipping refusal of the test executable");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.bound && runtime.frameHooked.load(), "object-group scope uses the real post-application Runtime");
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
                      "group operations execute on the actual distinct current engine-owner thread");
                action(access);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!expectActive || !runtime.stopping.load()), "one group action uses one admitted owner callback");
        };
        const auto raw = [&](std::uintptr_t at)
        {
            std::uint16_t value = 0;
            std::memcpy(&value, reinterpret_cast<void*>(at + P::FieldOffset), sizeof(value));
            return value;
        };
        const auto adjacent = [&](std::uintptr_t at)
        {
            return *reinterpret_cast<unsigned char*>(at + 0x26) == 0xff && *reinterpret_cast<unsigned char*>(at + 0x27) == 0xff &&
                   *reinterpret_cast<unsigned char*>(at + 0x2a) == 0xc0 && *reinterpret_cast<unsigned char*>(at + 0x2b) == 0;
        };
        std::uint16_t output = 123;
        Check(runtime.access.ReadModelObjectPropertiesGroup(3, output) != ClockResult::Ok && output == 123 && !runtime.modelObjectGroups.Pending(),
              "producer read refusal preserves output and creates no default entry");
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                Check(access.ReadModelObjectPropertiesGroup(3, output) == ClockResult::Ok && output == 0xffff && !runtime.modelObjectGroups.Pending(),
                      "real qualified Atomic metadata exposes its exact unsigned static sentinel");
                Check(access.SetModelObjectPropertiesGroup(3, 0xffff) == ClockResult::Ok && !runtime.modelObjectGroups.Pending() && adjacent(modelAt),
                      "same-value Set skips MTA default capture and every field effect");
            });
        if (retainedCase || unknownCase)
        {
            ClockObjectGroupOwnerTest::committedGroup = 0xfedc;
            ClockObjectGroupOwnerTest::failAfterCommitted = unknownCase;
            pulse(
                [&](NativeGameTimingAccess& access)
                {
                    const auto result = access.SetModelObjectPropertiesGroup(3, 0xfedc);
                    Check((unknownCase ? result != ClockResult::Ok : result == ClockResult::Ok) && raw(modelAt) == 0xfedc && adjacent(modelAt) &&
                              runtime.modelObjectGroups.Pending() && runtime.modelObjectGroups.Uncertain() == unknownCase &&
                              ClockObjectGroupOwnerTest::injected == unknownCase,
                          "actual word CAS retains a known default or post-CAS uncertainty without changing adjacent bytes");
                },
                !unknownCase);
            game.reset();
            Check(!service.End(generation), "actual End refuses pending known/unknown object-group intent");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.modelObjectGroups.Pending() && raw(modelAt) == 0xfedc && adjacent(modelAt),
                  "stopped-frame cleanup/new Begin do not discard, restore or replay retained group intent");
            engine.Stop();
            std::puts(unknownCase ? "Production object-group owner unknown completed" : "Production object-group owner retained completed");
            return;
        }

        // A type virtual alone is not a mutable allocation proof.
        std::vector<unsigned char> heapMetadata(0x100);
        const auto                 heapAt = reinterpret_cast<std::uintptr_t>(heapMetadata.data());
        store(heapAt, 0, fixture.image + P::VtableRva);
        store(heapAt, P::FieldOffset, std::uint16_t{33});
        store(fixture.image + NativeModelBankProfile::BankRva, 3 * sizeof(std::uintptr_t), heapAt);
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                output = 123;
                Check(access.ReadModelObjectPropertiesGroup(3, output) != ClockResult::Ok && output == 123 && !runtime.modelObjectGroups.Pending(),
                      "matching Atomic vtable/type on an arbitrary heap receiver grants no group-field authority");
            });
        store(fixture.image + NativeModelBankProfile::BankRva, 3 * sizeof(std::uintptr_t), modelAt);
        writes.Global(P::PoolCountRva, std::int32_t{1});
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                output = 123;
                Check(access.ReadModelObjectPropertiesGroup(4, output) != ClockResult::Ok && output == 123 && !runtime.modelObjectGroups.Pending(),
                      "a pool row beyond the exact native allocated prefix is refused");
            });
        writes.Global(P::PoolCountRva, std::int32_t{2});
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                Check(access.SetModelObjectPropertiesGroup(3, 0xfedc) == ClockResult::Ok && raw(modelAt) == 0xfedc && adjacent(modelAt),
                      "first real CAS keeps all unsigned16 bits and captures the original group once");
                Check(access.SetModelObjectPropertiesGroup(3, 0x1234) == ClockResult::Ok && raw(modelAt) == 0x1234,
                      "repeated Set retains the first original rather than its previous edited group");
                Check(access.SetModelObjectPropertiesGroup(4, 7) == ClockResult::Ok && access.RestoreModelObjectPropertiesGroup(4) == ClockResult::Ok &&
                          raw(secondAt) == 42 && adjacent(secondAt),
                      "single restore acknowledges the native word before erasing that model's default");
                game->RestoreGroupsForFixture();
                Check(!runtime.modelObjectGroups.Pending() && raw(modelAt) == 0xffff && adjacent(modelAt),
                      "actual CGame Reset group hook drains its model setters' same native registry");
                Check(access.SetModelObjectPropertiesGroup(3, 0x1234) == ClockResult::Ok && access.SetModelObjectPropertiesGroup(4, 0x5678) == ClockResult::Ok,
                      "two new edits prepare a real native bulk restoration prefix");
                ClockObjectGroupOwnerTest::failPrefix = true;
                ClockObjectGroupOwnerTest::injected = false;
                ClockObjectGroupOwnerTest::restoredAddress = 0;
                const auto result = access.RestoreAllModelObjectPropertiesGroups();
                Check(result != ClockResult::Ok && ClockObjectGroupOwnerTest::injected && ClockObjectGroupOwnerTest::restoredAddress &&
                          runtime.modelObjectGroups.Pending() && !runtime.modelObjectGroups.Uncertain() && !runtime.stopping.load(),
                      "known pre-entry refusal after one real acknowledged restore retains the entire default map and its accepted-prefix journal");
                ClockObjectGroupOwnerTest::failPrefix = false;
                const auto acknowledged = ClockObjectGroupOwnerTest::restoredAddress;
                store(acknowledged, 0, std::uint16_t{0x77});
                game->RestoreGroupsForFixture();
                std::uint16_t untouched = 0;
                std::memcpy(&untouched, reinterpret_cast<void*>(acknowledged), sizeof(untouched));
                Check(!runtime.modelObjectGroups.Pending() && untouched == 0x77 && adjacent(modelAt) && adjacent(secondAt),
                      "explicit retry restores only the remaining suffix and never replays a previously acknowledged native word");
            });
        std::uint16_t foreign = 123;
        ClockResult   foreignResult = ClockResult::Ok;
        std::thread   foreignThread([&] { foreignResult = runtime.access.ReadModelObjectPropertiesGroup(3, foreign); });
        foreignThread.join();
        Check(foreignResult != ClockResult::Ok && foreign == 123 && !runtime.modelObjectGroups.Pending(),
              "a foreign thread cannot borrow this model owner's native read or default cache");
        game.reset();
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.modelObjectGroups.CanDiscard(), "explicit healthy group reset permits authoritative owner retirement");
        NativeClockService  nextOwner;
        ClockServiceStateV1 nextState;
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool nextBegan = nextOwner.BeginGameTiming(request, nextState, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(nextBegan && nextState.generation != generation && !runtime.modelObjectGroups.Pending(),
              "a clean real group registry permits the distinct next owner generation");
        engine.Pulse(nextOwner, nextState.generation);
        if (!nextOwner.End(nextState.generation))
            engine.Pulse(nextOwner, nextState.generation, true);
        Check(nextOwner.End(nextState.generation), "the distinct no-effect next owner retires normally");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        ClockObjectGroupOwnerTest::models = {};
        ClockObjectGroupOwnerTest::providerCount = 0;
        writes.Restore();
        std::puts("Production object-group owner healthy completed");
    }
}

/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual Runtime dummy holds, vector stores, reset and retained debt
 *****************************************************************************/
namespace ModelDummiesOwnerFixture
{
    namespace Request = NativeStreamingRequestProfile;
    class Game final : public lifecycle_fixture::NoPresentationGameBase<CGameModelDummiesResetDE>
    {
        using Base = lifecycle_fixture::NoPresentationGameBase<CGameModelDummiesResetDE>;

    public:
        Game(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& owner, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state)
            : Base(timing, clock, owner, streaming, state)
        {
        }
        void RestoreDummiesForFixture() { this->ResetVehicleModelDummies(); }

    protected:
        void RequireResetReady() override { Refuse(); }
        void RestoreDefaultObjectGroupProperties() override { Refuse(); }
        void ResetModelTextureDictionaries() override { Refuse(); }
        void RequireWorldLifecycleReady() override { Refuse(); }
        void RemoveWorldCovers() override { Refuse(); }
        void RemoveWorldPlants() override { Refuse(); }
        void DestroyWorldStencilShadows() override { Refuse(); }
        void DropWorldBuildingCaches() override { Refuse(); }

    private:
        [[noreturn]] static void Refuse() { throw std::runtime_error("unqualified dummy lifecycle dependency"); }
    };
    static_assert(!std::is_abstract_v<Game> && std::is_abstract_v<CGameModelDummiesResetDE>);
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite dummy Runtime lifetime is selected");
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
        const auto root = reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data());
        store(root, Request::ProviderOffset, provider);
        store(provider, Request::ProviderRowsOffset, rows);
        store(provider, Request::ProviderCountOffset, std::int32_t{10});
        std::vector<std::uintptr_t> bank(NativeModelBankProfile::ModelCount);
        apply(NativeModelBankProfile::BankRva, bank.data(), NativeModelBankProfile::BankBytes, PAGE_READWRITE);
        NativeModelDummiesFixture::Storage storage(fixture.image, root, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        const auto                         original = storage.Bits(0), nativeExtra = storage.Bits(14);
        const auto                         secondOriginal = storage.Bits(6, 1);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockModelDummiesOwnerTest::visual = storage.Visual();
        ClockModelDummiesOwnerTest::providerCount = provider + Request::ProviderCountOffset;
        ClockModelDummiesOwnerTest::failAfterCommitted = ClockModelDummiesOwnerTest::injected = false;
        ClockModelDummiesOwnerTest::committedBits = 0x41100000;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "dummy owner preserves shipping refusal of the test executable");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.bound && runtime.frameHooked.load(), "dummy operations use the real post-application Runtime");
        const auto                   generation = admitted.generation;
        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        state;
        auto                         game = std::make_unique<Game>(timing, runtime.clock, runtime.access, streaming, state);
        const auto                   pulse = [&](const auto& action, bool expectActive = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& access)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "dummy native effects run on the real distinct engine owner");
                action(access);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!expectActive || !runtime.stopping.load()), "one dummy action has one admitted callback");
        };
        NativeModelDummyPositions output{};
        output[0] = {101, 102, 103};
        Check(runtime.access.ReadModelVehicleDummyPositions(3, output) != ClockResult::Ok && output[0].x == 101 && !runtime.modelDummies.Pending(),
              "producer dummy read preserves output and creates no cache or native hold");
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                Check(access.ReadModelVehicleDummyPositions(3, output) == ClockResult::Ok && output[0].x == 1 && output[13].x == 14 && storage.Count() == 2 &&
                          !runtime.modelDummies.Pending(),
                      "actual typed visual pool read exposes exactly fourteen vectors without taking a hold");
            });
        if (retainedCase || unknownCase)
        {
            ClockModelDummiesOwnerTest::failAfterCommitted = unknownCase;
            pulse(
                [&](NativeGameTimingAccess& access)
                {
                    const auto result = access.SetModelVehicleDummyPosition(3, 0, {9, 10, 11});
                    Check((unknownCase ? result != ClockResult::Ok : result == ClockResult::Ok) && storage.Count() == 3 && storage.Bits(0)[0] == 0x41100000 &&
                              storage.Bits(14) == nativeExtra && runtime.modelDummies.Pending() && runtime.modelDummies.Uncertain() == unknownCase &&
                              ClockModelDummiesOwnerTest::injected == unknownCase,
                          "actual model hold and component CAS retain known intent or post-CAS uncertainty without touching native slot14");
                },
                !unknownCase);
            const auto after = storage.Bits(0);
            game.reset();
            Check(!service.End(generation), "End cannot discard a known held dummy cache or uncertain vector prefix");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.modelDummies.Pending() && storage.Count() == 3 && storage.Bits(0) == after,
                  "stopped cleanup and new Begin never replay vectors or release the private native hold");
            engine.Stop();
            ClockModelDummiesOwnerTest::visual = ClockModelDummiesOwnerTest::providerCount = 0;
            ClockReaderTestHost::image = previousImage;
            std::puts(unknownCase ? "Production dummy owner unknown completed" : "Production dummy owner retained completed");
            return;
        }
        pulse(
            [&](NativeGameTimingAccess& access)
            {
                NativeModelDummyVector cached{100, 101, 102};
                bool                   found = true;
                Check(access.ReadCachedModelVehicleDummyDefault(3, 0, cached, found) == ClockResult::Ok && !found && cached.x == 100,
                      "empty default lookup reports only a current CPU cache miss");
                Check(access.SetModelVehicleDummyPosition(3, 0, {1, 21, 41}) == ClockResult::Ok && storage.Count() == 3 && runtime.modelDummies.Pending(),
                      "same-value dummy setter still captures its first original and acquires one actual native hold");
                Check(access.SetModelVehicleDummyPosition(3, 0, {9, 10, 11}) == ClockResult::Ok && storage.Count() == 3 && storage.Bits(0)[0] == 0x41100000,
                      "repeated setter stores the raw vector without acquiring a second model hold");
                Check(access.ReadCachedModelVehicleDummyDefault(3, 0, cached, found) == ClockResult::Ok && found && cached.x == 1 && cached.y == 21 &&
                          cached.z == 41,
                      "cached default survives repeated edits with its first original vector");
                Check(access.SetModelVehicleDummyPosition(4, 6, {3, 4, 5}) == ClockResult::Ok && storage.Count(1) == 1,
                      "a second model owns an independent hold even from an occupied zero-count key");
                Check(access.ResetModelVehicleDummies(4, true) == ClockResult::Ok && storage.Count(1) == 0 && storage.Bits(6, 1) == secondOriginal,
                      "single reset restores before a real final-zero KeepResident release");
                game->RestoreDummiesForFixture();
                Check(storage.Count() == 2 && storage.Bits(0) == original && storage.Bits(14) == nativeExtra && !runtime.modelDummies.Pending(),
                      "actual CGame dummy-reset hook drains the same cache, vectors and private holds");
                Check(access.SetModelVehicleDummyPosition(3, 0, {5, 6, 7}) == ClockResult::Ok && storage.Count() == 3 &&
                          access.ResetModelVehicleDummies(3, false) == ClockResult::Ok && storage.Count() == 2 &&
                          access.ResetModelVehicleDummies(3, false) == ClockResult::Ok && storage.Count() == 2,
                      "known Reset(false) acknowledgement never decrements its private hold twice");
                game->RestoreDummiesForFixture();
                Check(!runtime.modelDummies.Pending() && storage.Count() == 2 && storage.Bits(0) == original,
                      "bulk clear retires the already acknowledged false-remove record without replay");
            });
        game.reset();
        Check(!service.End(generation), "first End requests normal frame retirement");
        engine.Pulse(service, generation, true);
        Check(service.End(generation) && !runtime.modelDummies.Pending() && !runtime.modelDummies.Uncertain(),
              "healthy End discards no native effect and drains only released CPU ownership");
        engine.Stop();
        ClockModelDummiesOwnerTest::visual = ClockModelDummiesOwnerTest::providerCount = 0;
        ClockReaderTestHost::image = previousImage;
        std::puts("Production dummy owner healthy completed");
    }
}

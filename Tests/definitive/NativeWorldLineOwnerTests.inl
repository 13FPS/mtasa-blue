/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the retained Runtime owner for actual CWorld clear-line intent
 *****************************************************************************/
namespace WorldLineOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace Host = ClockWorldLineOwnerTest;
        namespace P = NativeWorldLineProfile;
        // Keep the proven owned-image/world/matrix writer and retirement
        // protocol. Defining those helpers does not replay the ground suite.
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &writes);
        NativeWorldGroundFixture::Roots      roots(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        roots.InitializePins();
        NativeWorldLineFixture::Install(&WorldGroundOwnerFixture::Writes::Apply, &writes);
        const auto priorReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "line query scope establishes the same owned current-image selection before GT/world checks");
        Host::image = fixture.image;
        CVector                            start, end;
        const std::array<std::uint32_t, 3> startBits{0x80000000, 0x3f800001, 0xc3964000};
        const std::array<std::uint32_t, 3> endBits{0x42f68001, 0xc04ccccd, 0x00000001};
        std::memcpy(&start, startBits.data(), sizeof(start));
        std::memcpy(&end, endBits.data(), sizeof(end));
        Host::startSource = &start;
        Host::endSource = &end;
        SLineOfSightFlags flags;
        flags.bCheckVehicles = false;
        flags.bCheckObjects = false;
        flags.bSeeThroughStuff = true;
        flags.bShootThroughStuff = true;
        flags.bCheckCarTires = true;
        const NativeWorldLineCalls::Flags expectedFlags{true, false, true, false, true, true, false};
        struct DefaultLineAccess final : NativeWorldLineAccess
        {
        };
        DefaultLineAccess absent;
        bool              sentinel = true;
        Check(absent.IsWorldLineOfSightClear(start, end, flags, sentinel) == ClockResult::NotInitialized && sentinel,
              "older line owner preserves output instead of manufacturing clear or blocked");
        for (unsigned scenario = 0; scenario != 5; ++scenario)
        {
            graph.Reset();
            roots.Reset();
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Host::level = graph.Level();
            Host::scenario = scenario;
            Host::clear = scenario != 1;
            Host::copied = false;
            Host::reenter = nullptr;
            const auto                        calls = Host::calls, effects = Host::effects;
            const bool                        oldValue = !Host::clear;
            bool                              value = oldValue, caught = false;
            ClockResult                       result = ClockResult::NotInitialized;
            WorldGroundOwnerFixture::Callback callback;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.IsWorldLineOfSightClear(start, end, flags, value); };
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            Check(runtime.access.IsWorldLineOfSightClear(start, end, flags, value) != ClockResult::Ok && value == oldValue && Host::calls == calls,
                  "retained service alone cannot lend current line-query callback authority");
            if (!scenario)
            {
                ClockResult foreign = ClockResult::Ok;
                std::thread other([&] { foreign = runtime.access.IsWorldLineOfSightClear(start, end, flags, value); });
                other.join();
                Check(foreign != ClockResult::Ok && value == oldValue && Host::calls == calls,
                      "foreign thread cannot inherit this line operation's current owner");
                Check(NativeStreamingCompletionContext::TryEnter(), "hold actual shared gate before deliberate line intent");
                fixture.invokeTimer();
                NativeStreamingCompletionContext::Leave();
                Check(result == ClockResult::Busy && value == oldValue && Host::calls == calls && !runtime.stopping.load(),
                      "new line intent shares the existing completion gate and preserves a refused output");
            }
            world_line_fixture::WorldLineFixture world(runtime.access, runtime.access);
            CWorld&                              sdk = world;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    value = sdk.IsLineOfSightClear(&start, &end, flags);
                }
                catch (const std::runtime_error& error)
                {
                    caught = std::string(error.what()).find("IsLineOfSightClear") != std::string::npos;
                }
            };
            if (scenario == 4)
            {
                Host::reenter = +[]() noexcept
                {
                    bool              line = false;
                    float             height = -17.25f;
                    SLineOfSightFlags nestedFlags;
                    Check(runtime.access.IsWorldLineOfSightClear(*Host::startSource, *Host::endSource, nestedFlags, line) == ClockResult::Busy && !line,
                          "recursive line intent refuses before a second engine call");
                    Check(runtime.access.FindWorldGroundZ(*Host::startSource, height) == ClockResult::Busy && height == -17.25f &&
                              runtime.access.LoadStreamingScene(Host::startSource) == ClockResult::Busy && !runtime.stopping.load(),
                          "line operation rejects ground and scene adapter reentry through the same shared gate");
                };
            }
            Check(service.Renew(runtime.generation.load()), "renew current owner before one deliberate actual SDK clear-line operation");
            fixture.invokeTimer();
            Check(Host::calls == calls + 1 && Host::effects == effects + 1 && Host::entry == P::EntryRva && Host::copied &&
                      std::memcmp(&Host::receivedStart, startBits.data(), sizeof(start)) == 0 &&
                      std::memcmp(&Host::receivedEnd, endBits.data(), sizeof(end)) == 0 && Host::receivedFlags == expectedFlags &&
                      std::memcmp(&start, startBits.data(), sizeof(start)) == 0 && std::memcmp(&end, endBits.data(), sizeof(end)) == 0,
                  "actual CWorld line dispatch enters genuine native ABI once with owned vectors and only the seven native flags");
            const bool failed = scenario == 2 || scenario == 3;
            Check(failed ? caught && value == oldValue && runtime.stopping.load() : !caught && value == Host::clear && !runtime.stopping.load(),
                  "true clear and false blocked are accepted; entered exception or lost world preserves output and Stops this owner");
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(Host::calls == calls + 1 && Host::effects == effects + 1, "native line retirement preserves accepted effects without query replay");
        }
        Host::image = Host::level = Host::entry = 0;
        Host::startSource = Host::endSource = nullptr;
        Host::reenter = nullptr;
        Host::scenario = 0;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        std::puts("Production native whole line-clear callback owner fixture completed");
        std::puts("Production actual CWorld line-clear SDK callback fixture completed");
    }
}

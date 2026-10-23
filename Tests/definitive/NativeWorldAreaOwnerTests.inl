/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read actual CWorld current-area DWORD through the existing owner
 *****************************************************************************/
namespace WorldAreaOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativeWorldAreaProfile;
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
        NativeWorldAreaFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        const auto priorReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "area scope selects the existing owned image before native thread/world observation");
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        writes.Global(P::AreaRva, std::uint32_t{0x800000ff});
        // This pure observation deliberately initializes no sector/matrix list
        // or new native entry. It only needs the actual current typed world.
        const auto groundCalls = ClockWorldGroundOwnerTest::calls, lineCalls = ClockWorldLineOwnerTest::calls;
        struct DefaultAreaAccess final : NativeWorldAreaAccess
        {
        };
        DefaultAreaAccess absent;
        std::uint32_t     value = 0x12345678;
        Check(absent.ReadWorldCurrentArea(value) == ClockResult::NotInitialized && value == 0x12345678,
              "older area owner preserves output instead of manufacturing successful exterior area0");
        WorldGroundOwnerFixture::Callback callback;
        ClockResult                       result = ClockResult::NotInitialized;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadWorldCurrentArea(value); };
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        Check(runtime.access.ReadWorldCurrentArea(value) != ClockResult::Ok && value == 0x12345678,
              "retained service alone cannot lend current native area-read scope");
        ClockResult foreign = ClockResult::Ok;
        std::thread other([&] { foreign = runtime.access.ReadWorldCurrentArea(value); });
        other.join();
        Check(foreign != ClockResult::Ok && value == 0x12345678, "foreign thread cannot inherit this raw DWORD observation authority");
        Check(NativeStreamingCompletionContext::TryEnter(), "hold effect-entry gate while exercising a separate pure copy");
        fixture.invokeTimer();
        NativeStreamingCompletionContext::Leave();
        Check(result == ClockResult::Ok && value == 0x800000ff && !runtime.stopping.load(),
              "pure current-area read needs no matrix/scan initialization or shared effect-gate acquisition");
        world_area_fixture::WorldAreaFixture world(runtime.access, runtime.access, runtime.access);
        CWorld&                              sdk = world;
        bool                                 caught = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                value = sdk.GetCurrentArea();
            }
            catch (const std::runtime_error& error)
            {
                caught = std::string(error.what()).find("GetCurrentArea") != std::string::npos;
            }
        };
        for (const std::uint32_t area : {0u, 0xffu, 0x100u, 0x80000000u, 0xffffffffu})
        {
            writes.Global(P::AreaRva, area);
            Check(service.Renew(runtime.generation.load()), "renew owner for a deliberate full-width actual CWorld area observation");
            caught = false;
            fixture.invokeTimer();
            Check(!caught && value == area && !runtime.stopping.load(),
                  "actual CWorld GetCurrentArea preserves zero and every high native DWORD bit without a ped BYTE conversion");
        }
        value = 0x12345678;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadWorldCurrentArea(value); };
        DWORD previous = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + P::AreaRva), sizeof(value), PAGE_READONLY, &previous) != FALSE,
              "deny only writable-image qualification of the owned native area field");
        Check(service.Renew(runtime.generation.load()), "renew owner before independent guarded-copy refusal");
        fixture.invokeTimer();
        Check(result == ClockResult::ReadFailure && value == 0x12345678 && !runtime.stopping.load(),
              "guarded area copy refuses changed image protection without Stop or publishing partial output");
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                value = sdk.GetCurrentArea();
            }
            catch (const std::runtime_error& error)
            {
                caught = std::string(error.what()).find("GetCurrentArea") != std::string::npos;
            }
        };
        caught = false;
        Check(service.Renew(runtime.generation.load()), "renew owner before actual SDK refused area observation");
        fixture.invokeTimer();
        Check(caught && value == 0x12345678 && !runtime.stopping.load(),
              "actual SDK refuses missing current data without a zero fallback or pretending a native entry occurred");
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + P::AreaRva), sizeof(value), previous, &ignored) != FALSE,
              "restore only the owned area page before current owner recovery");
        writes.Global(P::AreaRva, std::uint32_t{0xfedcba98});
        caught = false;
        Check(service.Renew(runtime.generation.load()), "same owner remains usable after a side-effect-free refusal");
        fixture.invokeTimer();
        Check(!caught && value == 0xfedcba98 && !runtime.stopping.load(), "later deliberate area read uses the unchanged current owner and actual DWORD");
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(ClockWorldGroundOwnerTest::calls == groundCalls && ClockWorldLineOwnerTest::calls == lineCalls && !runtime.nativeTouched.load(),
              "area observation and retirement neither enter nor replay ground/line native effects or setters");
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        std::puts("Production native current-area DWORD callback owner fixture completed");
        std::puts("Production actual CWorld current-area SDK callback fixture completed");
    }
}

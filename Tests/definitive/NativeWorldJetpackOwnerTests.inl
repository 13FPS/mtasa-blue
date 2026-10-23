/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read actual CWorld jetpack height through the existing native owner
 *****************************************************************************/
namespace WorldJetpackOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativeWorldJetpackProfile;
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
        NativeWorldJetpackFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        const auto priorReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "jetpack scope selects the existing owned image before genuine native thread/world observation");
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        writes.Global(P::LimitRva, std::uint32_t{0x42c80000}, PAGE_READONLY);
        // This pure getter initializes no sector/matrix list and acquires no
        // effect gate. Its owned synthetic constant retains genuine RO access.
        const auto  groundCalls = ClockWorldGroundOwnerTest::calls, lineCalls = ClockWorldLineOwnerTest::calls;
        const auto  bits = &WorldGroundOwnerFixture::Bits;
        const float refused = -173.25f;
        float       value = refused;
        struct DefaultJetpackAccess final : NativeWorldJetpackHeightAccess
        {
        };
        DefaultJetpackAccess absent;
        Check(absent.ReadJetpackMaxHeight(value) == ClockResult::NotInitialized && bits(value) == bits(refused),
              "older getter owner preserves output instead of substituting an observed-build height constant");
        WorldGroundOwnerFixture::Callback callback;
        ClockResult                       result = ClockResult::NotInitialized;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadJetpackMaxHeight(value); };
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        Check(runtime.access.ReadJetpackMaxHeight(value) != ClockResult::Ok && bits(value) == bits(refused),
              "retained service alone cannot lend this native jetpack getter callback scope");
        ClockResult foreign = ClockResult::Ok;
        std::thread other([&] { foreign = runtime.access.ReadJetpackMaxHeight(value); });
        other.join();
        Check(foreign != ClockResult::Ok && bits(value) == bits(refused), "foreign thread cannot inherit current loaded-world getter authority");
        Check(NativeStreamingCompletionContext::TryEnter(), "hold existing effect-entry gate during an independent guarded scalar read");
        fixture.invokeTimer();
        NativeStreamingCompletionContext::Leave();
        Check(result == ClockResult::Ok && bits(value) == 0x42c80000 && !runtime.stopping.load(),
              "read-only native consumer value needs neither writable RData nor scan/matrix initialization or another effect gate");
        world_jetpack_height_fixture::WorldJetpackHeightFixture world(runtime.access, runtime.access, runtime.access, runtime.access);
        CWorld&                                                 sdk = world;
        bool                                                    caught = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                value = sdk.GetJetpackMaxHeight();
            }
            catch (const std::runtime_error& error)
            {
                caught = std::string(error.what()).find("GetJetpackMaxHeight") != std::string::npos;
            }
        };
        for (const std::uint32_t raw : {0u, 0x80000000u, 0xc1a00001u, 0x7f800000u, 0x7fc12345u})
        {
            // Only this isolated SEC_IMAGE fixture changes its own bytes;
            // production admits no native RData store or operand redirection.
            writes.Global(P::LimitRva, raw, PAGE_READONLY);
            Check(service.Renew(runtime.generation.load()), "renew owner for a fresh actual CWorld raw-float observation");
            caught = false;
            fixture.invokeTimer();
            Check(!caught && bits(value) == raw && !runtime.stopping.load(),
                  "actual CWorld getter returns fresh signed-zero/nonfinite/finite bits without a constant fallback or new range policy");
        }
        value = refused;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            result = access.ReadJetpackMaxHeight(value);
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        };
        Check(service.Renew(runtime.generation.load()), "renew current callback before exact native GC-owner refusal");
        fixture.invokeTimer();
        Check(result == ClockResult::Busy && bits(value) == bits(refused) && !runtime.stopping.load(),
              "busy native GC scope refuses before pure copy without retiring the current owner");
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadJetpackMaxHeight(value); };
        DWORD previous = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + P::LimitRva), sizeof(value), PAGE_NOACCESS, &previous) != FALSE,
              "deny only the owned native scalar page before a guarded read refusal");
        Check(service.Renew(runtime.generation.load()), "renew owner before independent unreadable-scalar refusal");
        fixture.invokeTimer();
        Check(result == ClockResult::ReadFailure && bits(value) == bits(refused) && !runtime.stopping.load(),
              "unreadable native scalar preserves caller output and does not claim an entered native effect or Stop");
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                value = sdk.GetJetpackMaxHeight();
            }
            catch (const std::runtime_error& error)
            {
                caught = std::string(error.what()).find("GetJetpackMaxHeight") != std::string::npos;
            }
        };
        caught = false;
        Check(service.Renew(runtime.generation.load()), "renew owner before actual SDK refused scalar observation");
        fixture.invokeTimer();
        Check(caught && bits(value) == bits(refused) && !runtime.stopping.load(),
              "actual SDK reports refusal without manufacturing height100 or publishing a partial value");
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + P::LimitRva), sizeof(value), previous, &ignored) != FALSE,
              "restore the owned scalar page before current owner recovery");
        writes.Global(P::LimitRva, std::uint32_t{0x42c80001}, PAGE_READONLY);
        caught = false;
        Check(service.Renew(runtime.generation.load()), "same owner remains usable after a side-effect-free getter refusal");
        fixture.invokeTimer();
        Check(!caught && bits(value) == 0x42c80001 && !runtime.stopping.load(), "later deliberate getter reads fresh native bits on the unchanged owner");
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(ClockWorldGroundOwnerTest::calls == groundCalls && ClockWorldLineOwnerTest::calls == lineCalls && !runtime.nativeTouched.load(),
              "getter observation and retirement enter no native queries or setters and create no native mutation debt");
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        std::puts("Production native jetpack-height guarded callback owner fixture completed");
        std::puts("Production actual CWorld jetpack-height SDK callback fixture completed");
    }
}

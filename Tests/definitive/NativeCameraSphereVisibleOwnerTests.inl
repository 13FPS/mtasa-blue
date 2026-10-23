/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read actual CCamera sphere visibility through the existing owner
 *****************************************************************************/
namespace CameraSphereVisibleOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativeCameraSphereVisibleProfile;
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
        NativeCameraSphereVisibleFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        const auto priorReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "camera sphere scope selects the existing owned image before native thread/world observation");
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        namespace Host = ClockCameraSphereOwnerTest;
        Host::image = fixture.image;
        Host::calls = 0;
        Host::scenario = 0;
        Host::callerCenter = nullptr;
        Host::reenter = nullptr;
        // A direct camera query needs neither a registered ped nor initialized
        // model/sector/matrix allocator roots. Keep the actual ped bank absent.
        writes.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
        const auto boundsCalls = ClockPedOnScreenOwnerTest::calls;
        const auto groundCalls = ClockWorldGroundOwnerTest::calls, lineCalls = ClockWorldLineOwnerTest::calls;
        const auto bits = &WorldGroundOwnerFixture::Bits;
        CVector    center(1.0f, -2.0f, 3.0f);
        float      radius = -4.0f;
        bool       value = true;
        struct DefaultCameraAccess final : NativeCameraSphereVisibleAccess
        {
        };
        DefaultCameraAccess absent;
        Check(absent.ReadCameraSphereVisible(center, radius, value) == ClockResult::NotInitialized && value,
              "older camera owner preserves output instead of guessing native visibility");
        WorldGroundOwnerFixture::Callback callback;
        ClockResult                       result = ClockResult::NotInitialized;
        {
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadCameraSphereVisible(center, radius, value); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            Check(runtime.access.ReadCameraSphereVisible(center, radius, value) != ClockResult::Ok && value && !Host::calls,
                  "retained camera service outside its native callback cannot manufacture a sphere result");
            std::thread foreign([&] { result = runtime.access.ReadCameraSphereVisible(center, radius, value); });
            foreign.join();
            Check(result != ClockResult::Ok && value && !Host::calls && !runtime.stopping.load(),
                  "foreign camera observation cannot inherit native entry or Stop the actual owner");
            Check(NativeStreamingCompletionContext::TryEnter(), "hold existing effect gate before direct native camera entry");
            fixture.invokeTimer();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && value && !Host::calls && !runtime.stopping.load(),
                  "one shared gate refuses camera entry without a fallback bool, ped bounds or extra native call");
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            Check(service.Renew(runtime.generation.load()), "renew direct camera GC refusal intent");
            fixture.invokeTimer();
            Check(result == ClockResult::Busy && value && !Host::calls && !runtime.stopping.load(),
                  "native GC excludes camera entry without entering or revoking an unrelated ped relation");
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            camera_sphere_visible_fixture::CameraSphereVisibleFixture camera(runtime.access);
            const CCamera&                                            sdk = camera;
            bool                                                      caught = false;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    value = sdk.IsSphereVisible(&center, radius);
                }
                catch (const std::exception& error)
                {
                    caught = true;
                    std::fprintf(stderr, "Actual camera SDK refused: %s\n", error.what());
                }
            };
            const std::array<std::uint32_t, 7> patterns{{0, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x7fc12345u, 0x7f800000u, 0xff800000u}};
            const std::array<std::uint32_t, 3> xyz{{0x80000000u, 0x7fc54321u, 0x3f800001u}};
            CVector                            expectedCenter;
            std::memcpy(&expectedCenter, xyz.data(), sizeof(expectedCenter));
            for (const bool nativeValue : {false, true})
                for (const auto pattern : patterns)
                {
                    std::memcpy(&center, &expectedCenter, sizeof(center));
                    std::memcpy(&radius, &pattern, sizeof(radius));
                    Host::next = nativeValue;
                    Host::callerCenter = &center;
                    const auto before = Host::calls;
                    caught = false;
                    value = !nativeValue;
                    Check(service.Renew(runtime.generation.load()), "renew actual const CCamera sphere query with raw caller inputs");
                    fixture.invokeTimer();
                    Check(!caught && value == nativeValue && Host::calls == before + 1 && Host::entry == fixture.image + P::EntryRva &&
                              Host::camera == fixture.image + P::CameraRva && Host::centerAddress != reinterpret_cast<std::uintptr_t>(&center) &&
                              !std::memcmp(&Host::receivedCenter, &expectedCenter, sizeof(expectedCenter)) && bits(Host::receivedRadius) == pattern &&
                              center.fX == 100 && center.fY == 200 && center.fZ == 300 && !runtime.stopping.load(),
                          "actual const CCamera dispatch forwards copied XYZ/raw radius and one genuine camera AL bool without a CPU clamp or cached false");
                }
            static ClockResult nestedCamera = ClockResult::NotInitialized, nestedGround = ClockResult::NotInitialized;
            static bool        nestedValue = true, nestedStop = true;
            static float       nestedZ = 1;
            Host::reenter = +[]() noexcept
            {
                nestedValue = true;
                nestedZ = -173.25f;
                nestedCamera = runtime.access.ReadCameraSphereVisible(Host::receivedCenter, Host::receivedRadius, nestedValue);
                nestedGround = runtime.access.FindWorldGroundZ(Host::receivedCenter, nestedZ);
                nestedStop = runtime.stopping.load();
            };
            const auto reentryBefore = Host::calls;
            Check(service.Renew(runtime.generation.load()), "renew direct camera shared-entry reentry observation");
            fixture.invokeTimer();
            Host::reenter = nullptr;
            Check(!caught && Host::calls == reentryBefore + 1 && nestedCamera == ClockResult::Busy && nestedGround == ClockResult::Busy && nestedValue &&
                      bits(nestedZ) == bits(-173.25f) && !nestedStop && !runtime.stopping.load(),
                  "direct camera shares the existing native-operation gate with camera and ground reentry while preserving both outputs");
            Check(!runtime.pedReferences.IsRetained() && ClockPedOnScreenOwnerTest::calls == boundsCalls && ClockWorldGroundOwnerTest::calls == groundCalls &&
                      ClockWorldLineOwnerTest::calls == lineCalls,
                  "camera observation creates no ped reference/model requirement and never substitutes a ped-bound or world query");
            WorldGroundOwnerFixture::Retire(service, fixture);
        }
        // Each entered failure gets its own real callback owner. With no
        // pointer/state write debt, the timer tail may already be clean at End.
        for (unsigned scenario = 1; scenario != 3; ++scenario)
        {
            graph.Reset();
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            Host::scenario = scenario;
            Host::next = scenario == 2;
            Host::callerCenter = nullptr;
            value = !Host::next;
            const auto         prior = value;
            const auto         callsBefore = Host::calls;
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadCameraSphereVisible(center, radius, value); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            // State0 is outside the established native semantic domain,
            // so the shared owner reports binding loss rather than frontend.
            const auto expected = scenario == 1 ? ClockResult::OriginalException : ClockResult::BindingChanged;
            Check(result == expected && runtime.failure.load() == expected && value == prior && runtime.stopping.load() && Host::calls == callsBefore + 1 &&
                      !runtime.pedReferences.IsRetained(),
                  "entered camera exception or lost final world acknowledgement Stops once and preserves caller bool without false fallback");
            Host::scenario = 0;
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(Host::calls == callsBefore + 1, "camera owner retirement never replays or rolls back the native sphere operation");
        }
        Host::image = 0;
        Host::callerCenter = nullptr;
        Host::reenter = nullptr;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        std::puts("Production native camera sphere callback owner fixture completed");
        std::puts("Production actual CCamera sphere SDK owner fixture completed");
    }
}

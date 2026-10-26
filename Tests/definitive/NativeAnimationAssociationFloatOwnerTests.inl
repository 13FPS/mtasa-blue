/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Focused current float getters through real owned native lifetime
 ****************************************************************************/
namespace AnimationAssociationFloatOwnerFixture
{
    namespace Hook = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using SourceBank = PrivateAnimationAssociationOwnerFixture::SourceBank;
    using Token = NativeAnimationAssociationReferenceToken;
    using Values = NativeAnimationAssociationValues;
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    template <class Action>
    void Throws(Action action, const char* why)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, why);
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        admit(NativeStreamingLoadAllProfile::Code);
        for (const auto& pin : NativeStartupOwnerProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeStartupWorldProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeUObjectMembershipProfile::pins);
        for (const auto& pin : NativeGameStateProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &saved);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::active = &host;
        // The direct fixture does not run the old AnimationBlocks wrapper.
        // Its read-only bank foundation still validates these exact pool pins.
        admit(NativePedPoolProfile::Code);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        admit(NativeAnimationBlockProfile::Code);
        // These genuine dispatch cells share executable text pages. Match the
        // established bank fixture preparation without replaying its cases.
        saved.Global(NativeAnimationBlockProfile::RemapRva + NativeAnimationBlockProfile::LoadedOpcodeIndex,
                     static_cast<unsigned char>(NativeAnimationBlockProfile::LoadedSelector), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::DispatchRva + NativeAnimationBlockProfile::LoadedSelector * 4,
                     static_cast<std::uint32_t>(NativeAnimationBlockProfile::LoadedBranchRva), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::GroupSlotRva, fixture.image + NativeAnimationBlockProfile::GroupRva, PAGE_READONLY);
        admit(NativeAnimationBlockRefProfile::Code);
        admit(NativeAnimationGroupArrayProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::ReadCode);
        admit(NativeAnimationAssociationProfile::Code);
        // Length has its own genuine field/profile prerequisites. Installing
        // them here never adds those dependencies to the old scalar reader.
        admit(NativeAnimationHierarchyReadProfile::Code);
        saved.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        SourceBank source(fixture, saved);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Hook::image = fixture.image;
        Hook::constructorCalls = Hook::destructorCalls = Hook::exchangeCalls = Hook::heapAllocations = Hook::heapFrees = 0;
        Hook::targetsValid = Hook::snapshotsCopied = true;
        Hook::heapFailed = false;
        Hook::mode = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "association float scope pins the actual callback module once for this process");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew a bounded real callback before each single float semantic operation");
            fixture.invokeTimer();
        };
        auto& access = static_cast<NativeAnimationAssociationAccess&>(runtime.access);

        const auto execute = [&](auto action)
        {
            callback.action = [&](NativeGameTimingAccess&) { action(); };
            invoke();
            // Do not retain the local action parameter between callbacks.
            callback.action = [](NativeGameTimingAccess&) {};
        };
        Token                                                token;
        std::uintptr_t                                       receiver = 0;
        std::unique_ptr<PrivateAnimationAssociationReceiver> observer;
        execute(
            [&]
            {
                Check(access.CreatePrivateAnimationAssociation(generation, token) == ClockResult::Ok && token.ownerGeneration == generation && token.serial,
                      "float scope issues one genuine private unconstructed104 record");
            });
        execute(
            [&]
            {
                observer = std::make_unique<PrivateAnimationAssociationReceiver>(access, token);
                receiver = reinterpret_cast<std::uintptr_t>(observer->Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(source.children)));
                Check(receiver && host.Contains(receiver) && Hook::constructorCalls == 1 && Hook::Load<std::uint16_t>(source.counter) == 18,
                      "float scope constructs through the exact native body and current bank-H ref journal");
            });
        const CAnimBlendAssociation& sdk = *observer;
        for (const std::uint32_t bits : {std::uint32_t{0}, std::uint32_t{0x80000000u}, std::uint32_t{0x7fc12345u}, std::uint32_t{0x42c80000u}})
        {
            // These controlled native producers alter only the already
            // qualified synthetic object's source fields, never its origin.
            Hook::Store(receiver + 0x38, bits);
            Hook::Store(receiver + 0x3c, bits);
            saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x18, bits);
            execute(
                [&]
                {
                    Values value;
                    Check(access.ReadPrivateAnimationAssociation(token, value) == ClockResult::Ok && value.currentTimeBits == bits && value.speedBits == bits,
                          "existing native snapshot returns exact current absolute-time and speed bits after hierarchy duration changes");
                });
            execute(
                [&]
                {
                    std::uint32_t value = 0xaabbccddu;
                    Check(access.ReadPrivateAnimationAssociationLength(token, value) == ClockResult::Ok && value == bits,
                          "separate joint native duration read preserves zero signedzero NaN and normal bits");
                });
            execute([&] { Check(Bits(sdk.GetCurrentProgress()) == bits, "actual const SDK progress preserves absolute time without normalization"); });
            execute([&] { Check(Bits(sdk.GetCurrentSpeed()) == bits, "actual const SDK speed preserves every current source bit"); });
            execute([&] { Check(Bits(sdk.GetLength()) == bits, "actual const SDK length returns current joint bank-H time without a cache or clamp"); });
        }
        // A duration-only profile failure must not silently turn unrelated
        // metadata into a new profile-dependent service or fabricate length0.
        const auto& durationPin = NativeAnimationHierarchyReadProfile::Code[0];
        saved.Global(durationPin.rva, static_cast<unsigned char>(durationPin.bytes[0] ^ 1u), PAGE_EXECUTE_READ);
        execute(
            [&]
            {
                std::uint32_t unchanged = 0x8899aabbu;
                Check(access.ReadPrivateAnimationAssociationLength(token, unchanged) == ClockResult::BindingChanged && unchanged == 0x8899aabbu,
                      "missing exact duration producer refuses with its caller output unchanged");
            });
        execute(
            [&]
            {
                Values value;
                Check(access.ReadPrivateAnimationAssociation(token, value) == ClockResult::Ok && value.currentTimeBits == 0x42c80000u,
                      "ordinary native metadata still works without the independent duration profile");
            });
        execute([&] { Throws([&] { (void)sdk.GetLength(); }, "actual SDK reports refused duration through its deliberately throwing contract"); });
        saved.Global(durationPin.rva, durationPin.bytes[0], PAGE_EXECUTE_READ);
        execute(
            [&]
            {
                auto stale = token;
                ++stale.ownerGeneration;
                std::uint32_t unchanged = 0x11223344u;
                Check(access.ReadPrivateAnimationAssociationLength(stale, unchanged) == ClockResult::OwnerConflict && unchanged == 0x11223344u,
                      "foreign generation refuses without publishing a duration");
            });
        execute(
            [&]
            {
                const auto scope = timingCallbackGeneration;
                timingCallbackGeneration = 0;
                std::uint32_t unchanged = 0x55667788u;
                const auto    result = access.ReadPrivateAnimationAssociationLength(token, unchanged);
                timingCallbackGeneration = scope;
                Check(result != ClockResult::Ok && unchanged == 0x55667788u, "an OS thread alone cannot replace the current natural callback owner");
            });
        execute(
            [&]
            {
                saved.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                std::uint32_t unchanged = 0x12345678u;
                const auto    result = access.ReadPrivateAnimationAssociationLength(token, unchanged);
                saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
                Check(result != ClockResult::Ok && unchanged == 0x12345678u, "current GC exclusion refuses without inventing a float result");
            });
        execute(
            [&]
            {
                const auto highwater = Hook::Load<std::int32_t>(fixture.image + NativeAnimationManagerProfile::AnimationCountRva);
                saved.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{0});
                std::uint32_t unchanged = 0xa1b2c3d4u;
                const auto    result = access.ReadPrivateAnimationAssociationLength(token, unchanged);
                saved.Global(NativeAnimationManagerProfile::AnimationCountRva, highwater);
                Check(result != ClockResult::Ok && unchanged == 0xa1b2c3d4u, "duration requires this hierarchy's current admitted bank extent");
            });
        Check(service.Renew(generation), "renew before a separate wrong-thread duration request");
        ClockResult   workerResult = ClockResult::Ok;
        std::uint32_t workerValue = 0xfeedfaceu;
        std::thread   worker([&] { workerResult = access.ReadPrivateAnimationAssociationLength(token, workerValue); });
        worker.join();
        // A foreign thread has no timingTimerScope TLS, so CheckTimingOwner
        // follows legacy CheckOwner's NotInitialized precedence before the
        // independent GameThread check. Preserve that production guard order.
        Check(workerResult == ClockResult::NotInitialized && workerValue == 0xfeedfaceu,
              "foreign thread duration requests preserve TLS-first refusal and never publish an output");
        execute(
            [&]
            {
                Check(!runtime.stopping.load() && !runtime.privateAssociations.RecoveryPending() && Bits(sdk.GetLength()) == 0x42c80000u,
                      "restored pure read prerequisites remain usable without mutation debt or a fabricated earlier value");
            });
        const auto beforeFree = host.freeCalls;
        observer.reset();
        Check(host.freeCalls == beforeFree && access.HasPendingPrivateAnimationAssociations(),
              "destroying the CPU float observer never acknowledges native retirement");
        execute(
            [&]
            {
                Check(access.RetirePrivateAnimationAssociation(token) == ClockResult::Ok && !access.HasPendingPrivateAnimationAssociations() &&
                          Hook::Load<std::uint16_t>(source.counter) == 17,
                      "genuine explicit deleting-destructor ACK retires only the issued dynamic node and outer resources");
            });
        Check(Hook::constructorCalls == 1 && Hook::destructorCalls == 1 && Hook::heapAllocations == 1 && Hook::heapFrees == 2 && Hook::targetsValid &&
                  Hook::snapshotsCopied && !Hook::heapFailed,
              "the focused float process executes one genuine constructor and one matching destructor without replay");
        Check(!service.End(), "producer End only requests hook retirement after acknowledged native resource cleanup");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.privateAssociations.IsRetained() && runtime.modulePinned.load() &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "natural callback cleanup releases admission while preserving the actual process-lifetime module pin");
        ClockReaderTestHost::image = previousImage;
        Hook::image = 0;
        Host::active = nullptr;
        saved.Restore();
        std::puts("Production native animation association float owner fixture completed");
        std::puts("Production actual animation association float SDK owner fixture completed");
    }
}

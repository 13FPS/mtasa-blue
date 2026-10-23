/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw visibility through the sole registered-ped callback owner
 ****************************************************************************/
{
    namespace Visible = NativePedVisibilityProfile;
    namespace Host = ClockPedVisibilityOwnerTest;
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Visible::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const auto visibleGroup = fixture.image + Visible::GroupRva;
    const auto visibleBranch = static_cast<std::uint32_t>(Visible::CaseRva);
    const auto visibleSelector = static_cast<unsigned char>(Visible::Selector);
    admit(Visible::GroupSlotRva, &visibleGroup, sizeof(visibleGroup), PAGE_READONLY);
    admit(Visible::RemapRva + Visible::CaseIndex, &visibleSelector, sizeof(visibleSelector), PAGE_EXECUTE_READ);
    admit(Visible::TableRva + Visible::Selector * sizeof(visibleBranch), &visibleBranch, sizeof(visibleBranch), PAGE_EXECUTE_READ);
    // The named script also writes this byte. The actual SDK property must
    // never acquire that current-player effect merely because it shares a bit.
    constexpr std::uintptr_t scriptVisibleRva = 0x50315a1;
    const unsigned char      scriptVisibleSentinel = 0xa5;
    admit(scriptVisibleRva, &scriptVisibleSentinel, sizeof(scriptVisibleSentinel), PAGE_READWRITE);
    struct VisibilityCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<VisibilityCallback*>(opaque)->action(access); }
    } visibilityCall;
    struct DefaultVisibilityOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } visibilityAbsent;
    for (const bool prior : {false, true})
    {
        bool output = prior;
        Check(visibilityAbsent.ReadPedVisible({}, output) == ClockResult::NotInitialized && output == prior &&
                  visibilityAbsent.SetPedVisible({}, prior) == ClockResult::NotInitialized,
              "older owners refuse visibility without fabricated bool or mutation success");
    }
    ped_visibility_fixture::Game         visibilityGame;
    ped_visibility_fixture::TaskScenario visibilityScenario;
    const auto                           visibilityAddress = reinterpret_cast<std::uintptr_t>(pool.objects) + Visible::FlagsOffset;
    constexpr std::uint32_t              visibilityOriginal = 0xa1a55a25u;
    static_assert((visibilityOriginal & Visible::VisibleMask) == 0);
    const auto putVisibility = [&](std::uint32_t flags) { std::memcpy(reinterpret_cast<void*>(visibilityAddress), &flags, sizeof(flags)); };
    const auto getVisibility = [&]
    {
        std::uint32_t flags = 0;
        std::memcpy(&flags, reinterpret_cast<void*>(visibilityAddress), sizeof(flags));
        return flags;
    };
    const auto resetVisibility = [&]
    {
        Host::watched = visibilityAddress;
        Host::cell = 0;
        Host::replacement = 0;
        Host::reads = Host::stores = Host::changeAfter = 0;
        Host::failNextRead = Host::loseAfterRead = Host::neighborRace = Host::failAfter = Host::loseAfterWrite = false;
        Host::callerReadToken = Host::callerWriteToken = nullptr;
        putVisibility(visibilityOriginal);
    };
    const auto makeVisibilityPed = [&](const NativePedReferenceToken& selectedReference)
    {
        auto& access = runtime.access;
        return std::make_unique<ped_visibility_fixture::PedVisibilityFixture>(
            visibilityGame, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access,
            access, access, access, access, access, access, selectedReference, 0x12345678, visibilityScenario);
    };
    std::unique_ptr<ped_visibility_fixture::PedVisibilityFixture> oldVisibilitySdk;
    ClockResult                                                   visibilityResult = ClockResult::NotInitialized;
    bool                                                          visibilityOutput = true;
    resetVisibility();
    {
        NativeClockService      visibilityService;
        NativePedReferenceToken visibilityToken;
        visibilityCall.action = [&](NativeGameTimingAccess& access) { visibilityResult = access.AcquirePedReference(1, visibilityToken); };
        BeginOwned(visibilityService, fixture, false, true, true, &VisibilityCallback::Invoke, &visibilityCall);
        fixture.invokeTimer();
        Check(visibilityResult == ClockResult::Ok && visibilityToken.version && runtime.pedReferences.IsRetained(),
              "visibility borrows the sole actual registered-ped controller");
        oldVisibilitySdk = makeVisibilityPed(visibilityToken);
        Check(!Host::reads && !Host::stores && visibilityScenario.eventCount == 0,
              "visibility SDK construction copies borrowed identity without task lookup or native work");
        Check(runtime.access.ReadPedVisible(visibilityToken, visibilityOutput) != ClockResult::Ok && visibilityOutput &&
                  runtime.access.SetPedVisible(visibilityToken, true) != ClockResult::Ok && !Host::reads && !Host::stores,
              "retained token provides no visibility read or write authority between callbacks");
        std::thread foreignVisibility(
            [&]
            {
                visibilityResult = runtime.access.ReadPedVisible(visibilityToken, visibilityOutput);
                Check(runtime.access.SetPedVisible(visibilityToken, true) != ClockResult::Ok, "foreign visibility setter refuses");
            });
        foreignVisibility.join();
        Check(visibilityResult != ClockResult::Ok && visibilityOutput && !runtime.stopping.load() && !Host::reads && !Host::stores,
              "foreign visibility access preserves output and cannot Stop the genuine owner");
        const auto invokeVisibility = [&]
        {
            Check(visibilityService.Renew(runtime.generation.load()), "renew actual visibility callback");
            fixture.invokeTimer();
        };
        visibilityCall.action = [&](NativeGameTimingAccess& access)
        {
            auto stale = visibilityToken;
            ++stale.version;
            Check(access.ReadPedVisible(stale, visibilityOutput) != ClockResult::Ok && visibilityOutput &&
                      access.SetPedVisible(stale, true) != ClockResult::Ok && !Host::stores && !runtime.stopping.load(),
                  "initial stale visibility token refuses without output, entry or Stop");
        };
        invokeVisibility();
        const auto&                visibilityPin = Visible::Code[0];
        std::vector<unsigned char> changedVisibilityCode(visibilityPin.bytes, visibilityPin.bytes + visibilityPin.size);
        changedVisibilityCode[0] ^= 1;
        fixture.Patch(visibilityPin.rva, changedVisibilityCode.data(), changedVisibilityCode.size());
        visibilityCall.action = [&](NativeGameTimingAccess& access) { visibilityResult = access.SetPedVisible(visibilityToken, true); };
        invokeVisibility();
        Check(visibilityResult != ClockResult::Ok && !Host::stores && !runtime.stopping.load(),
              "unknown visibility code refuses before mutation without stopping valid Runtime");
        fixture.Patch(visibilityPin.rva, visibilityPin.bytes, visibilityPin.size);
        for (const bool requested : {false, true, true, false})
        {
            const auto beforeStores = Host::stores;
            visibilityCall.action = [&](NativeGameTimingAccess&)
            {
                CPed& sdk = *oldVisibilitySdk;
                sdk.SetVisible(requested);
                visibilityOutput = sdk.IsVisible();
                visibilityResult = ClockResult::Ok;
            };
            invokeVisibility();
            unsigned char scriptAfter = 0;
            std::memcpy(&scriptAfter, reinterpret_cast<void*>(fixture.image + scriptVisibleRva), sizeof(scriptAfter));
            Check(visibilityResult == ClockResult::Ok && visibilityOutput == requested &&
                      getVisibility() == (visibilityOriginal | (requested ? Visible::VisibleMask : 0u)) && Host::stores == beforeStores + 1 &&
                      scriptAfter == scriptVisibleSentinel && visibilityScenario.eventCount == 0,
                  "actual SDK reads/writes one visible bit with one CAS even for same value, preserving neighbors and script global");
        }
        visibilityCall.action = [&](NativeGameTimingAccess& access) { visibilityResult = access.ReadPedVisible(visibilityToken, visibilityOutput); };
        for (const bool prior : {false, true})
        {
            Host::failNextRead = true;
            visibilityOutput = prior;
            invokeVisibility();
            Check(visibilityResult == ClockResult::ReadFailure && visibilityOutput == prior && !runtime.stopping.load(),
                  "failed visibility read preserves either caller bool without Stop");
        }
        Host::reads = 0;
        Host::changeAfter = 1;
        Host::replacement = visibilityOriginal | Visible::VisibleMask;
        putVisibility(visibilityOriginal);
        visibilityOutput = true;
        invokeVisibility();
        Check(visibilityResult == ClockResult::OwnerConflict && visibilityOutput && !runtime.stopping.load(),
              "incoherent visible bit is refused without publishing a fabricated observation");
        Host::reads = 0;
        Host::replacement = visibilityOriginal ^ 2u;
        putVisibility(visibilityOriginal);
        invokeVisibility();
        Check(visibilityResult == ClockResult::Ok && !visibilityOutput && !runtime.stopping.load(),
              "neighbor-only word changes preserve the named bool coherence contract");
        Host::changeAfter = 0;
        const auto selectedVisibility = visibilityToken;
        Host::callerReadToken = &visibilityToken;
        invokeVisibility();
        Check(visibilityResult == ClockResult::Ok && visibilityToken.version == selectedVisibility.version + 1 && !runtime.stopping.load(),
              "visibility reader fixes caller token before any native callbacks");
        visibilityToken = selectedVisibility;
        Host::callerWriteToken = &visibilityToken;
        visibilityCall.action = [&](NativeGameTimingAccess& access) { visibilityResult = access.SetPedVisible(visibilityToken, true); };
        invokeVisibility();
        Check(visibilityResult == ClockResult::Ok && visibilityToken.version == selectedVisibility.version + 1 && !runtime.stopping.load(),
              "visibility setter acknowledges its original selected token after caller token mutation");
        visibilityToken = selectedVisibility;
        retire(visibilityService);
    }
    resetVisibility();
    {
        NativeClockService      visibilityService;
        NativePedReferenceToken freshToken;
        visibilityCall.action = [&](NativeGameTimingAccess& access) { visibilityResult = access.AcquirePedReference(1, freshToken); };
        BeginOwned(visibilityService, fixture, false, true, true, &VisibilityCallback::Invoke, &visibilityCall);
        fixture.invokeTimer();
        Check(visibilityResult == ClockResult::Ok, "new visibility generation acquires its own registered relation");
        bool refusedRead = false, refusedWrite = false;
        visibilityCall.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                (void)oldVisibilitySdk->IsVisible();
            }
            catch (const std::runtime_error& error)
            {
                refusedRead = std::string(error.what()).find("IsVisible") != std::string::npos;
            }
            try
            {
                oldVisibilitySdk->SetVisible(true);
            }
            catch (const std::runtime_error& error)
            {
                refusedWrite = std::string(error.what()).find("SetVisible") != std::string::npos;
            }
        };
        Check(visibilityService.Renew(runtime.generation.load()), "renew stale visibility SDK check");
        fixture.invokeTimer();
        Check(refusedRead && refusedWrite && !Host::reads && !Host::stores && !runtime.stopping.load(),
              "old SDK rejects after End/newBegin despite reused native pointer, without touching current owner");
        auto freshSdk = makeVisibilityPed(freshToken);
        visibilityCall.action = [&](NativeGameTimingAccess&)
        {
            freshSdk->SetVisible(true);
            visibilityOutput = freshSdk->IsVisible();
        };
        Check(visibilityService.Renew(runtime.generation.load()), "renew current visibility SDK check");
        fixture.invokeTimer();
        Check(visibilityOutput && Host::stores == 1 && !runtime.stopping.load(), "current SDK uses the new registered generation");
        retire(visibilityService);
    }
    // Each entered failure gets its own genuine owner. A failed CAS is not a
    // license to retry a racing word; accepted/unknown effects remain intact.
    for (const unsigned failure : {1u, 2u, 3u, 4u})
    {
        resetVisibility();
        NativeClockService      visibilityService;
        NativePedReferenceToken token;
        visibilityCall.action = [&](NativeGameTimingAccess& access) { visibilityResult = access.AcquirePedReference(1, token); };
        BeginOwned(visibilityService, fixture, false, true, true, &VisibilityCallback::Invoke, &visibilityCall);
        fixture.invokeTimer();
        Check(visibilityResult == ClockResult::Ok, "fresh visibility fault owner uses genuine registered relation");
        const auto visibilityNode = ClockPedReferenceOwnerTest::node;
        const auto visibilityUnregisters = ClockPedReferenceOwnerTest::unregisters;
        Host::cell = ClockPedReferenceOwnerTest::cell;
        Host::neighborRace = failure == 1;
        Host::failAfter = failure == 2;
        Host::loseAfterWrite = failure == 3;
        Host::loseAfterRead = failure == 4;
        visibilityOutput = true;
        visibilityCall.action = [&](NativeGameTimingAccess& access)
        { visibilityResult = failure == 4 ? access.ReadPedVisible(token, visibilityOutput) : access.SetPedVisible(token, true); };
        Check(visibilityService.Renew(runtime.generation.load()), "renew visibility fault callback");
        fixture.invokeTimer();
        const auto expectedFlags = failure == 1 ? (visibilityOriginal ^ 2u) : failure == 4 ? visibilityOriginal : (visibilityOriginal | Visible::VisibleMask);
        const unsigned expectedStores = failure == 4 ? 0u : 1u;
        Check(visibilityResult != ClockResult::Ok && runtime.stopping.load() && Host::stores == expectedStores && getVisibility() == expectedFlags &&
                  visibilityOutput,
              "entered visibility failure or successful read losing token Stops without replay, rollback or caller output");
        if (failure >= 3)
        {
            Check(!visibilityService.End() && runtime.pedReferences.IsRetained(), "zero weak word cannot free a still-named native reference cell");
            fixture.invokeTimer();
            Check(ClockPedReferenceOwnerTest::unregisters == visibilityUnregisters && runtime.pedReferences.IsRetained() && Host::stores == expectedStores,
                  "stopping visibility retains cell debt without unsafe unregister or effect replay");
            // Supply the already-proved native address-retirement outcome in
            // this owned fixture, never a production force-release operation.
            ClockPedReferenceOwnerTest::Put(visibilityNode + 8, std::uintptr_t{0});
            const auto freeNode = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(visibilityNode, freeNode);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, visibilityNode);
            ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
            fixture.invokeTimer();
        }
        retire(visibilityService);
        Check(Host::stores == expectedStores && getVisibility() == expectedFlags, "visibility retirement cannot replay or restore accepted flags");
    }
    Host::watched = Host::cell = 0;
    Host::callerReadToken = Host::callerWriteToken = nullptr;
    Host::failNextRead = Host::loseAfterRead = Host::neighborRace = Host::failAfter = Host::loseAfterWrite = false;
    std::puts("Production native registered visibility callback owner fixture completed");
    std::puts("Production actual ped visibility SDK callback fixture completed");
}

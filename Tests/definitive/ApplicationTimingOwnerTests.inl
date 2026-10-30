/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual post-frame timing ownership and natural-frame restoration
 ****************************************************************************/
namespace ApplicationTimingOwnerFixture
{
    struct Context
    {
        NativeClockService* service = nullptr;
        std::uint64_t       generation = 0;
        unsigned            calls = 0, originals = 0;
        bool                originalReturned = false, throwAfterWrite = false;
    };
    Context*                 current = nullptr;
    ApplicationFrameFunction original = nullptr;

    void Original(void* receiver, float delta)
    {
        ++current->originals;
        current->originalReturned = false;
        Check(!timingCallbackGeneration && !timingTimerScope && !frameCallbackGeneration && !frameCallbackReceiver && !frameCallbackSerial &&
                  runtime.access.SetTimeScale(8.0f) != ClockResult::Ok,
              "the whole genuine original frame has no borrowed timing/frame write scope");
        original(receiver, delta);
        current->originalReturned = true;
    }

    void Handle(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<Context*>(opaque);
        ++context.calls;
        Check(context.originalReturned && context.originals >= context.calls && timingCallbackGeneration == context.generation &&
                  frameCallbackGeneration == context.generation && frameCallbackReceiver && frameCallbackSerial == runtime.counters.qualified.load() &&
                  !timingTimerScope && CheckTimingOwner() == ClockResult::Ok,
              "the actual timing handler receives only this qualified post-original frame scope");
        Check(runtime.frameMode && runtime.timingMode && runtime.frameHooked.load() && !runtime.hooked.load() && !runtime.timerHooked.load() &&
                  !runtime.timerReady.load() && !runtime.captured.load(),
              "frame timing installs no clock/timer hook and invents no timer readiness or CClock capture");
        const auto writes = ClockTestFaults::clockDataWrites.load();
        ++callbackDepth;
        Check(access.SetTimeScale(3.0f) != ClockResult::Ok, "nested scope cannot borrow the outer post-frame writer");
        --callbackDepth;
        const auto generation = timingCallbackGeneration;
        --timingCallbackGeneration;
        Check(access.SetTimeScale(3.0f) != ClockResult::Ok, "stale timing generation cannot borrow this frame");
        timingCallbackGeneration = generation;
        const auto serial = frameCallbackSerial;
        --frameCallbackSerial;
        Check(access.SetTimeScale(3.0f) != ClockResult::Ok, "a previous frame serial cannot borrow the current timing owner");
        frameCallbackSerial = serial;
        bool        foreign = false;
        std::thread other([&] { foreign = access.SetTimeScale(3.0f) != ClockResult::Ok; });
        other.join();
        Check(foreign && ClockTestFaults::clockDataWrites.load() == writes && !runtime.timingDirty.load(),
              "foreign and mismatched scopes perform no native writes or dirty capture");
        Check(access.SetTimeScale(0.75f) == ClockResult::Ok && access.SetMinuteDuration(2500) == ClockResult::Ok && access.Pause(true) == ClockResult::Ok,
              "same existing native timing/pause writers work through the admitted post-frame owner");
        if (context.throwAfterWrite)
            throw std::runtime_error("owned post-frame handler fault");
    }

    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.Seed();
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        fixture.Put(NativeApplicationFrameProfile::stateRva, std::uint32_t{9});
        const auto oldReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        NativeClockService service;
        Context            context;
        context.service = &service;
        context.throwAfterWrite = scenario == 2;
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.Begin(request, rejected) && !service.BeginApplicationFrames(request, rejected, nullptr) &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "ordinary clock and observer Begin retain strict rejection of the timing-only option");
        auto invalid = request;
        invalid.reserved[0] |= 2;
        Check(!service.BeginGameTiming(invalid, rejected, &Handle, &context) && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "unknown timing option bits refuse before any admission or hook");
        Check(!service.BeginGameTiming(request, rejected, &Handle, &context) && rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound &&
                  !rejected.hooked && !rejected.modulePinned,
              "public frame-timing Begin retains the shipping wrong-executable refusal");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &Handle, &context);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        if (!began)
        {
            // Binding diagnostics read this owned image only. They never
            // replace a failed public admission with fixture runtime flags.
            std::fprintf(stderr, "frame Begin failure=%u imageMatch=%u image=%u binding=%u timing=%u applicationTiming=%u modes=%u/%u\n",
                         static_cast<unsigned>(admitted.failure), runtime.image == fixture.image, CheckImage(), CheckBinding(true), CheckTimingBinding(),
                         CheckApplicationTimingBinding(), runtime.timingMode, runtime.frameMode);
            for (const auto& pin : NativeApplicationFrameProfile::pins)
            {
                std::array<unsigned char, NativeApplicationFrameProfile::maximumPinBytes> bytes{};
                if (!NativeImageAccess::ReadImage(fixture.image, fixture.image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    std::fprintf(stderr, "frame pin mismatch=%llx\n", static_cast<unsigned long long>(pin.rva));
            }
            std::fprintf(stderr, "pause pins entry=%u tail=%u reads=%u branch=%u frame=%u return=%u normal=%u alternate=%u gameplay=%u rw=%u/%u\n",
                         CheckCode(NativeGamePauseProfile::entryRva, NativeGamePauseProfile::entry),
                         CheckCode(NativeGamePauseProfile::tailRva, NativeGamePauseProfile::tail),
                         CheckCode(NativeGamePauseProfile::pauseReadsRva, NativeGamePauseProfile::pauseReads),
                         CheckCode(NativeGamePauseProfile::pauseBranchRva, NativeGamePauseProfile::pauseBranch),
                         CheckCode(NativeGamePauseProfile::frameRva, NativeGamePauseProfile::frame),
                         CheckCode(NativeGamePauseProfile::returnRva, NativeGamePauseProfile::returnCode),
                         CheckCode(NativeGamePauseProfile::normalCallRva, NativeGamePauseProfile::normalCall),
                         CheckCode(NativeGamePauseProfile::alternateCallRva, NativeGamePauseProfile::alternateCall),
                         CheckCode(NativeGamePauseProfile::gameplayTailRva, NativeGamePauseProfile::gameplayTail),
                         NativeImageAccess::ImagePages(fixture.image, fixture.image + NativeGamePauseProfile::userPauseRva, 1, true),
                         NativeImageAccess::ImagePages(fixture.image, fixture.image + NativeClockProfile::minuteDurationRva, 4, true));
        }
        Check(began && admitted.generation > rejected.generation && admitted.bound && admitted.hooked && admitted.modulePinned && runtime.frameHooked.load() &&
                  !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.timerReady.load(),
              "actual public Begin admits the owned native image and installs only the application-frame hook");
        context.generation = admitted.generation;
        current = &context;
        original = runtime.originalFrame;
        runtime.originalFrame = &Original;
        const auto pulse = [&](bool loading)
        {
            Check(service.Renew(context.generation), "post-frame test producer renews the admitted owner");
            StaticAssociationInitializationOwnerFixture::ProducerPulse producer(service, context.generation);
            if (loading)
                fixture.invokeLoadingFrame(fixture.frameOwner.data(), 0.25f);
            else
                fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
            producer.Stop();
            Check(!producer.unexpectedFailures.load(), "independent producer retains the actual native frame handoff");
        };
        pulse(true);
        Check(!context.calls && !runtime.timingDirty.load() && !runtime.timerReady.load(),
              "a genuine loading-loop caller forwards original work without lending timing access");
        pulse(false);
        Check(context.calls == 1 && fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 0.75f &&
                  fixture.Get<std::uint32_t>(NativeClockProfile::minuteDurationRva) == 2500 &&
                  fixture.Get<unsigned char>(NativeGamePauseProfile::userPauseRva) == 1 && runtime.timingDirty.load() && !runtime.captured.load() &&
                  !timingCallbackGeneration && !timingTimerScope && !frameCallbackGeneration && !frameCallbackReceiver && !frameCallbackSerial,
              "real frame writes preserve their restoration journal and retire every callback scope afterward");
        Check(runtime.access.SetTimeScale(5.0f) != ClockResult::Ok, "the same OS thread has no timing write access outside its callback");
        // Detours must retain its actual trampoline pointer for eventual
        // detach; the assertion wrapper is only a temporary test observer.
        runtime.originalFrame = original;
        Check(!service.End() && service.IsRetained() && runtime.frameHooked.load(),
              "producer End retains dirty timing until a natural qualified frame restores it");
        if (scenario == 1)
        {
            ClockTestFaults::failEnumeration.store(true);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
            ClockTestFaults::failEnumeration.store(false);
            Check(service.IsRetained() && runtime.timingDirty.load() && fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 0.75f &&
                      context.calls == 1 && runtime.frameHooked.load(),
                  "refused restoration retains the real frame hook and dirty journal without replaying the client");
        }
        fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
        Check(context.calls == 1 && !runtime.timingDirty.load() && !runtime.timingRecovery.load() &&
                  fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f &&
                  fixture.Get<std::uint32_t>(NativeClockProfile::minuteDurationRva) == 1000 &&
                  fixture.Get<unsigned char>(NativeGamePauseProfile::userPauseRva) == 0 && !runtime.timerReady.load(),
              "stopped qualified frame restores all original timing/pause values without invoking the client again");
        Check(service.End() && !service.IsRetained() && !runtime.frameHooked.load() && !runtime.hooked.load() && !runtime.timerHooked.load() &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && !timingCallbackGeneration && !frameCallbackGeneration &&
                  !frameCallbackReceiver && !frameCallbackSerial,
              "actual End detaches the frame owner and retires its admission only after restoration");
        current = nullptr;
        original = nullptr;
        ClockReaderTestHost::image = oldReaderImage;
        saved.Restore();
        std::puts(scenario == 0   ? "Production application timing actual owner and restoration fixture completed"
                  : scenario == 1 ? "Production application timing refused restoration retry fixture completed"
                                  : "Production application timing throwing handler cleanup fixture completed");
    }
}

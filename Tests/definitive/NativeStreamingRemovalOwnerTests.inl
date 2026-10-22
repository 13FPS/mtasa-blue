/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep direct streaming removal distinct from model-reference policy
 ****************************************************************************/
{
    namespace D = NativeStreamingRemoveProfile;
    for (const auto& pin : D::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    ClockStreamingRemoveOwnerTest::image = fixture.image;
    Check(absent.RemoveStreamingModel(3) == ClockResult::NotInitialized, "older owners cannot fake direct streaming removal");
    context.operation = ModelOwnershipOperation::Remove;
    context.index = 3;
    for (unsigned scenario = 0; scenario != 3; ++scenario)
    {
        seed(3, 0);
        reference.count = 2;
        ClockStreamingRemoveOwnerTest::scenario = scenario;
        const auto         calls = ClockStreamingRemoveOwnerTest::calls;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        Check(runtime.access.RemoveStreamingModel(3) != ClockResult::Ok && ClockStreamingRemoveOwnerTest::calls == calls,
              "direct state0 removal requires actual callback rather than retention alone");
        if (!scenario)
        {
            context.index = 0xffffffff;
            fixture.invokeTimer();
            Check(context.result == ClockResult::InvalidArgument && !runtime.stopping.load() && ClockStreamingRemoveOwnerTest::calls == calls,
                  "full DWORD invalid removal refuses before entry without retiring current owner");
            context.index = 3;
            seed(3, 2);
            Check(service.Renew(runtime.generation.load()), "renew before pending-removal refusal");
            fixture.invokeTimer();
            Check(context.result == ClockResult::UnsupportedBuild && !runtime.stopping.load() && ClockStreamingRemoveOwnerTest::calls == calls,
                  "pending cancellation cannot substitute an unqualified unload protocol");
            seed(3, 0);
            Check(service.Renew(runtime.generation.load()), "renew after clean direct-removal refusal");
        }
        fixture.invokeTimer();
        Check(ClockStreamingRemoveOwnerTest::calls == calls + 1 && ClockStreamingRemoveOwnerTest::entry == D::RemoveRva &&
                  ClockStreamingRemoveOwnerTest::key == 3 && reference.count == 2,
              "actual owner calls genuine native RemoveModel once without decrementing separate model reference count");
        Check(!scenario ? context.result == ClockResult::Ok && !runtime.stopping.load() : context.result != ClockResult::Ok && runtime.stopping.load(),
              "native no-op is acknowledged but entered exception or changed return row stops same owner");
        const auto flags = fixture.Get<unsigned char>(L::ArrayRva + 3 * L::RowStride + L::FlagsOffset);
        retire(service);
        Check(ClockStreamingRemoveOwnerTest::calls == calls + 1 && reference.count == 2 &&
                  fixture.Get<unsigned char>(L::ArrayRva + 3 * L::RowStride + L::FlagsOffset) == flags,
              "removal retirement never repeats entry, decrements a model count or rewinds accepted/foreign native row effects");
    }
    seed(3, 1);
    ClockStreamingRemoveOwnerTest::scenario = 0;
    ClockStreamingRemoveOwnerTest::image = 0;
    std::puts("Production native streaming removal callback owner fixture completed");
}

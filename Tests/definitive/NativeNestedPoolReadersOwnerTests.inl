/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise nested SDK pool readers in one genuine current Runtime
 ****************************************************************************/
#include "NativeNestedPoolReadersFixture.h"
#include "PoolUsageFixture.generated.h"
namespace NativeNestedPoolReadersOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
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
        NativeWorldIplFixture::State         world(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Check(world.Ready(), "nested pool readers reuse the genuine current native heap and world fixture");
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "nested pool readers preserve shipping executable refusal before explicit owned-image admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && runtime.frameHooked.load(), "nested pool readers borrow the existing actual post-application Runtime owner");
        const auto generation = state.generation;
        // CPools keeps a fixed native-capacity ped registry. Keep that owned
        // CPU object off the producer's stack while Begin admits the frame
        // hook; the nested receivers borrow this same object until End.
        auto                                      pools = std::make_unique<pool_usage_fixture::Pools>(runtime.access);
        NativeNestedPoolReadersFixture::Buildings buildings(*pools);
        NativeNestedPoolReadersFixture::Singles   singles(*pools);
        NativePoolUsageFixture::State             allocations(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Check(allocations.Ready(), "nested SDK readers use the existing seven-family owned native allocation fixture");
        const auto check = [](bool condition, const char* message) { Check(condition, message); };
        NativeNestedPoolReadersFixture::Refused(buildings, singles, check, "producer-thread nested status calls preserve both outputs");
        unsigned called = 0;
        callback.action = [&](NativeGameTimingAccess& owner)
        {
            ++called;
            Check(&owner == &runtime.access && GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                  "nested SDK pool virtuals execute in one genuine engine callback using the same actual owner as CPools");
            NativeNestedPoolReadersFixture::Run(allocations, buildings, singles, generation, &CheckTimingOwner, &CheckApplicationTimingOwner, check);
        };
        engine.Pulse(service, generation);
        callback.action = [](NativeGameTimingAccess&) {};
        Check(called == 1 && !runtime.stopping.load(), "both nested SDK read surfaces return from one admitted native frame");
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation), "nested pool readers retire with the existing Runtime and retain no native intent");
        NativeNestedPoolReadersFixture::Refused(buildings, singles, check, "retired-owner nested status calls cannot renew the earlier native borrow");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production nested pool readers owner completed");
    }
}

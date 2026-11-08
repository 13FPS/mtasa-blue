/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the real pool usage adapter in one admitted Runtime owner
 ****************************************************************************/
#include "NativePoolUsageFixture.h"
#include "PoolUsageFixture.generated.h"
namespace NativePoolUsageOwnerFixture
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
        Check(world.Ready(), "pool usage uses the existing genuine current native heap and world fixture");
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
              "pool census preserves shipping executable refusal before explicit owned-image admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && runtime.frameHooked.load(), "pool usage borrows the actual post-application Runtime owner");
        const auto                   generation = state.generation;
        NativePoolUsageAccess::Value refused{0xabcde, 0xfedcb};
        Check(runtime.access.ReadPoolUsage(NativePoolUsageAccess::Kind::Building, refused) != ClockResult::Ok && refused.capacity == 0xabcde &&
                  refused.used == 0xfedcb,
              "the producer thread cannot borrow native pool allocation counts");
        unsigned called = 0;
        callback.action = [&](NativeGameTimingAccess& owner)
        {
            ++called;
            Check(&owner == &runtime.access && GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                  "all native pool usage and SDK dispatch execute in one genuine engine callback with the same actual Runtime owner");
            // Only the finite owned source graph is prepared or corrupted.
            // Production executes complete flag reads and current owner seals;
            // no native allocator/count entry or write adapter is substituted.
            pool_usage_fixture::Pools sdk(owner);
            NativePoolUsageFixture::Run(runtime.image, &WorldGroundOwnerFixture::Writes::Apply, &writes, generation, &CheckTimingOwner,
                                        &CheckApplicationTimingOwner, owner, sdk, [](bool condition, const char* message) { Check(condition, message); });
        };
        engine.Pulse(service, generation);
        callback.action = [](NativeGameTimingAccess&) {};
        Check(called == 1 && !runtime.stopping.load(), "all seven pool allocation families return from one admitted native frame");
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation), "native allocation census retires with the current Runtime and retains no native intent");
        refused = {0xabcde, 0xfedcb};
        Check(runtime.access.ReadPoolUsage(NativePoolUsageAccess::Kind::Building, refused) != ClockResult::Ok && refused.capacity == 0xabcde &&
                  refused.used == 0xfedcb,
              "a retired Runtime cannot turn the earlier native census into a renewed borrow");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production native pool usage owner completed");
    }
}

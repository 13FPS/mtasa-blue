/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current-facing reads/writes borrow the actual registered ped owner
 ****************************************************************************/
// Reuse the already initialized real reference fixture. No fake CPed wrapper,
// current-player row or second native ownership bank is manufactured here.
{
    namespace Rotation = NativePedRotationProfile;
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Rotation::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    admit(Rotation::ScriptRadiansScaleRva, &Rotation::ScriptRadiansScaleBits, sizeof(Rotation::ScriptRadiansScaleBits), PAGE_READONLY);
    ClockPedRotationOwnerTest::stores = 0;
    ClockPedRotationOwnerTest::cell = 0;
    ClockPedRotationOwnerTest::loseToken = false;
    store(Rotation::CurrentOffset, 0x3f800001);
    store(Rotation::CurrentOffset + 4, 0x41200000);
    for (unsigned scenario = 0; scenario != 2; ++scenario)
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && context.token.version, "rotation owner independently acquires a real registered native reference");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew same callback owner for an independent current-facing intent");
            fixture.invokeTimer();
        };
        if (!scenario)
        {
            float      sentinel = 123;
            const auto stores = ClockPedRotationOwnerTest::stores;
            Check(runtime.access.ReadPedCurrentRotation(context.token, sentinel) != ClockResult::Ok && sentinel == 123 &&
                      runtime.access.SetPedCurrentRotation(context.token, 2) != ClockResult::Ok && ClockPedRotationOwnerTest::stores == stores,
                  "retained rotation token grants no output or mutation outside its admitted callback");
            ClockResult foreign = ClockResult::Ok;
            std::thread other([&] { foreign = runtime.access.SetPedCurrentRotation(context.token, 2); });
            other.join();
            Check(foreign != ClockResult::Ok && ClockPedRotationOwnerTest::stores == stores, "foreign thread cannot borrow native current-facing authority");
            context.operation = 3;
            invoke();
            Check(context.result == ClockResult::Ok && floatBits(context.output) == 0x3f800001 && !runtime.playerInfo.IsBound(),
                  "actual current rotation reads raw native radians on a generic ped without a current-player substitution");
            context.operation = 4;
            context.target = fromBits(0x7fc12345);
            invoke();
            Check(context.result == ClockResult::Ok && raw(Rotation::CurrentOffset) == 0x7fc12345 && raw(Rotation::CurrentOffset + 4) == 0x41200000 &&
                      ClockPedRotationOwnerTest::stores == stores + 1,
                  "actual owner writes only current float payload and leaves independently unqualified target untouched");
            const auto valid = context.token;
            ++context.token.version;
            context.operation = 3;
            context.output = 123;
            invoke();
            Check(context.result != ClockResult::Ok && context.output == 123 && !runtime.stopping.load(),
                  "stale rotation token refuses publication while preserving the unrelated admitted owner");
            context.token = valid;
            context.operation = 4;
            context.target = fromBits(0x80000000);
            invoke();
            context.operation = 3;
            invoke();
            Check(context.result == ClockResult::Ok && floatBits(context.output) == 0x80000000 && raw(Rotation::CurrentOffset + 4) == 0x41200000,
                  "same registered lifetime reads back signed-zero current rotation without target/placement effects");
            retire(service);
            Check(raw(Rotation::CurrentOffset) == 0x80000000, "normal retirement preserves persistent current-facing intent");
        }
        else
        {
            const auto stores = ClockPedRotationOwnerTest::stores, unlinks = ClockPedReferenceOwnerTest::unregisters;
            const auto node = ClockPedReferenceOwnerTest::node;
            ClockPedRotationOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
            ClockPedRotationOwnerTest::loseToken = true;
            context.operation = 4;
            context.target = fromBits(0xffc07654);
            invoke();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && raw(Rotation::CurrentOffset) == 0xffc07654 &&
                      ClockPedRotationOwnerTest::stores == stores + 1 && !ClockPedRotationOwnerTest::loseToken,
                  "accepted current-facing CAS followed by token loss stops same owner and preserves raw effect");
            Check(runtime.pedReferences.IsRetained() && !service.End(), "native node still naming registered cell retains address retirement debt");
            fixture.invokeTimer();
            Check(ClockPedRotationOwnerTest::stores == stores + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
                  "stopping callback never replays rotation CAS or reclaims still-named native reference storage");
            // Supply only the independently proved native ResolveReferences
            // address-retirement postcondition. Rotation effects stay intact.
            ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
            const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(node, head);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
            ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
            fixture.invokeTimer();
            retire(service);
            Check(
                ClockPedRotationOwnerTest::stores == stores + 1 && raw(Rotation::CurrentOffset) == 0xffc07654 && raw(Rotation::CurrentOffset + 4) == 0x41200000,
                "resolved reference debt permits retirement without rollback or another current-facing action");
        }
    }
    ClockPedRotationOwnerTest::cell = 0;
    ClockPedRotationOwnerTest::loseToken = false;
    std::puts("Production native current ped rotation callback owner fixture completed");
}

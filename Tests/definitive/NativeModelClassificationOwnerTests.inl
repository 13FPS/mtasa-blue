/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute genuine classifier and pure type facts through the actual owner
 ****************************************************************************/
{
    namespace M = NativeModelClassificationProfile;
    for (const auto& pin : M::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    constexpr std::uintptr_t           leafRva = 0x1100030;
    const auto                         leaf = fixture.image + leafRva;
    const std::array<unsigned char, 6> pure{0xb8, 6, 0, 0, 0, 0xc3};
    admit(leafRva, pure.data(), pure.size(), PAGE_EXECUTE_READ);
    admit(modelTableRva + P::TypeMethodSlot, &leaf, sizeof(leaf), PAGE_READONLY);
    ClockModelClassificationOwnerTest::image = fixture.image;
    ClockModelClassificationOwnerTest::replacementModel = modelAddress + 0x80;
    ClockModelClassificationOwnerTest::scenario = 0;
    const auto restore = [&]
    {
        fixture.Put(P::RootRva, root);
        fixture.Put(NativeModelBankProfile::BankRva + 3 * sizeof(std::uintptr_t), modelAddress);
    };
    bool                       preserved = true;
    NativeModelTypeObservation type{true, 77};
    Check(absent.ReadModelVehiclePredicate(3, NativeModelVehiclePredicate::Boat, preserved) == ClockResult::NotInitialized && preserved &&
              absent.ReadModelType(3, type) == ClockResult::NotInitialized && type.hasModel && type.nativeType == 77,
          "older owners refuse model facts without manufacturing false/Unknown");
    {
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        const auto calls = ClockModelClassificationOwnerTest::calls;
        Check(runtime.access.ReadModelVehiclePredicate(3, NativeModelVehiclePredicate::Boat, preserved) != ClockResult::Ok && preserved &&
                  runtime.access.ReadModelType(3, type) != ClockResult::Ok && type.hasModel && type.nativeType == 77 &&
                  ClockModelClassificationOwnerTest::calls == calls,
              "retained owner outside its actual callback grants no model predicate or type observation");
        context.index = 3;
        context.operation = ModelOwnershipOperation::Classification;
        for (const auto& entry : M::Entries)
            for (unsigned char value : std::array<unsigned char, 2>{0, 1})
            {
                context.predicate = entry.predicate;
                context.loaded = value == 0;
                ClockModelClassificationOwnerTest::value = value;
                const auto before = ClockModelClassificationOwnerTest::calls;
                Check(service.Renew(runtime.generation.load()), "renew same genuine model predicate callback");
                fixture.invokeTimer();
                Check(context.result == ClockResult::Ok && context.loaded == (value != 0) && ClockModelClassificationOwnerTest::calls == before + 1 &&
                          ClockModelClassificationOwnerTest::entry == entry.rva && ClockModelClassificationOwnerTest::key == 3 && !runtime.stopping.load(),
                      "actual callback invokes exact named native predicate once and preserves both native boolean outcomes");
            }
        context.operation = ModelOwnershipOperation::ModelType;
        for (unsigned char value : std::array<unsigned char, 5>{0, 6, 7, 8, 255})
        {
            const std::array<unsigned char, 6> code{0xb8, value, 0, 0, 0, 0xc3};
            fixture.Patch(leafRva, code.data(), code.size());
            context.typeObservation = {true, 77};
            const auto before = ClockModelClassificationOwnerTest::calls;
            Check(service.Renew(runtime.generation.load()), "renew current native pure type observer");
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && context.typeObservation.hasModel && context.typeObservation.nativeType == value &&
                      ClockModelClassificationOwnerTest::calls == before && !runtime.stopping.load(),
                  "actual owner publishes only whole-leaf native AL facts without invoking an arbitrary virtual or classifier");
        }
        const std::array<unsigned char, 6> impure{0x8b, 0x41, 0x20, 0xc3, 0xcc, 0xcc};
        fixture.Patch(leafRva, impure.data(), impure.size());
        context.typeObservation = {true, 77};
        Check(service.Renew(runtime.generation.load()), "renew pure-type clean refusal check");
        fixture.invokeTimer();
        Check(context.result != ClockResult::Ok && context.typeObservation.hasModel && context.typeObservation.nativeType == 77 && !runtime.stopping.load(),
              "impure getter cannot be emulated as a raw type or retire a clean read-only owner");
        fixture.Put(NativeModelBankProfile::BankRva + 3 * sizeof(std::uintptr_t), std::uintptr_t{0});
        context.typeObservation = {true, 77};
        Check(service.Renew(runtime.generation.load()), "renew qualified absent-model type observer");
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && !context.typeObservation.hasModel && context.typeObservation.nativeType == 0 && !runtime.stopping.load(),
              "actual callback represents current null bank separately from owner refusal and native AL values");
        restore();
        retire(service);
    }
    for (unsigned scenario = 1; scenario != 4; ++scenario)
    {
        restore();
        fixture.Patch(leafRva, pure.data(), pure.size());
        ClockModelClassificationOwnerTest::scenario = scenario;
        context.operation = ModelOwnershipOperation::Classification;
        context.index = 3;
        context.predicate = NativeModelVehiclePredicate::Boat;
        context.loaded = false;
        const auto         calls = ClockModelClassificationOwnerTest::calls;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result != ClockResult::Ok && !context.loaded && runtime.stopping.load() && ClockModelClassificationOwnerTest::calls == calls + 1,
              "entered classifier fault or lost bank/root stops exact owner while preserving the caller's previous boolean");
        retire(service);
        Check(ClockModelClassificationOwnerTest::calls == calls + 1, "predicate retirement never replays the native function");
    }
    restore();
    fixture.Patch(modelTableRva + P::TypeMethodSlot, &method, sizeof(method));
    ClockModelClassificationOwnerTest::scenario = 0;
    ClockModelClassificationOwnerTest::image = 0;
    std::puts("Production native model classification callback owner fixture completed");
    std::puts("Production native model type callback owner fixture completed");
}

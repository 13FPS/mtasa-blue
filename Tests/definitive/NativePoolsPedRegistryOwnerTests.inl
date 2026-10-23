/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK links through the sole native registered-cell owner
 ****************************************************************************/
{
    struct RegistryContext
    {
        NativePedReferenceToken            token;
        pool_registry_fixture::Pools*      registry = nullptr;
        pool_registry_fixture::Ped*        wrapper = nullptr;
        CPoolsDE::BorrowedPedRegistration* registration = nullptr;
        ClockResult                        result = ClockResult::NotInitialized;
        unsigned                           operation = 0;
        std::uint32_t                      handle = 1;
        CPed*                              output = nullptr;
        bool                               present = true;
        NativePedPoolReader::Value         value;
        static void                        Handle(NativeGameTimingAccess& access, void* opaque)
        {
            auto& current = *static_cast<RegistryContext*>(opaque);
            switch (current.operation)
            {
                case 0:
                    current.result = access.AcquirePedReference(1, current.token);
                    break;
                case 1:
                    current.result = current.registry->BindBorrowedPed(*current.wrapper, nullptr, *current.registration);
                    break;
                case 2:
                    current.result = current.registry->ReadPedFromRef(current.handle, current.output);
                    break;
                case 3:
                    current.result = access.ProbePedPoolHandle(current.handle, current.present, current.value);
                    break;
                case 4:
                    current.result = access.ReleasePedReference(current.token);
                    break;
                default:
                    std::abort();
            }
        }
    } registryContext;
    NativeClockService registryService;
    const auto         registers = ClockPedReferenceOwnerTest::registers;
    const auto         unregisters = ClockPedReferenceOwnerTest::unregisters;
    BeginOwned(registryService, fixture, false, true, true, &RegistryContext::Handle, &registryContext);
    fixture.invokeTimer();
    Check(registryContext.result == ClockResult::Ok && registryContext.token.version && runtime.pedReferences.IsRetained() &&
              ClockPedReferenceOwnerTest::registers == registers + 1,
          "existing genuine callback controller alone acquires the wrapper's native reference cell");
    pool_registry_fixture::Pools      registry(runtime.access);
    CPoolsDE::BorrowedPedRegistration registration;
    unsigned                          destroyed = 0;
    auto                              wrapper =
        std::make_unique<pool_registry_fixture::Ped>(runtime.access, registryContext.token,
                                                     [&]
                                                     {
                                                         ++destroyed;
                                                         Check(!registration.IsBound() && registry.GetPedCount() == 0,
                                                               "actual owner fixture withdraws the borrowed pool link before concrete SDK wrapper destruction");
                                                     });
    registryContext.registry = &registry;
    registryContext.wrapper = wrapper.get();
    registryContext.registration = &registration;
    registryContext.operation = 1;
    const auto invokeRegistry = [&]
    {
        Check(registryService.Renew(runtime.generation.load()), "renew the same native callback for a registry operation");
        fixture.invokeTimer();
    };
    Check(registry.BindBorrowedPed(*wrapper, nullptr, registration) != ClockResult::Ok && !registration.IsBound() && !registry.GetPedCount(),
          "even an actual issued token and SDK object cannot register between admitted callbacks");
    invokeRegistry();
    Check(registryContext.result == ClockResult::Ok && registration.IsBound() && registry.GetPedCount() == 1 &&
              ClockPedReferenceOwnerTest::registers == registers + 1 && !runtime.playerInfo.IsBound(),
          "factory-only actual SDK association reuses the same token/controller without another native acquire or current-player substitution");
    registryContext.operation = 2;
    Check(registry.ReadPedFromRef(1, registryContext.output) != ClockResult::Ok && !registryContext.output,
          "matching registered wrapper/native token still supplies no producer-side pointer publication");
    invokeRegistry();
    Check(registryContext.result == ClockResult::Ok && registryContext.output == static_cast<CPed*>(wrapper.get()),
          "actual qualified native resolver and existing reference-cell token publish the genuine registered SDK wrapper");
    pool.flags[8] = 127;
    std::memcpy(pool.objects + 8 * Ped::ObjectStride, &vtable, sizeof(vtable));
    registryContext.handle = 0x87f;
    registryContext.operation = 3;
    invokeRegistry();
    Check(registryContext.result == ClockResult::Ok && registryContext.present && registryContext.value.slot == 8,
          "real pool presence independently observes another occupied native index without manufacturing its SDK wrapper");
    registryContext.operation = 2;
    registryContext.output = wrapper.get();
    invokeRegistry();
    Check(registryContext.result == ClockResult::Ok && !registryContext.output && registry.GetPedCount() == 1,
          "actual GetPedFromRef returns qualified null for an occupied native index absent from the SDK registry");
    pool.flags[8] = 0x80;
    registryContext.operation = 3;
    registryContext.value.handle = 0xdeadbeef;
    registryContext.value.nativePed = 123;
    registryContext.present = true;
    invokeRegistry();
    Check(registryContext.result == ClockResult::Ok && !registryContext.present && registryContext.value.handle == 0xdeadbeef &&
              registryContext.value.nativePed == 123,
          "same callback exposes true native absence separately from refusal while preserving the opaque value output");
    registryContext.operation = 2;
    registryContext.output = wrapper.get();
    invokeRegistry();
    Check(registryContext.result == ClockResult::Ok && !registryContext.output && registry.GetPedCount() == 1,
          "free native generation cannot fall back to the CPU wrapper count or another ped's registered identity");
    registryContext.operation = 4;
    invokeRegistry();
    Check(registryContext.result == ClockResult::Ok && ClockPedReferenceOwnerTest::unregisters == unregisters + 1 && !runtime.pedReferences.IsRetained() &&
              registration.IsBound() && registry.GetPedCount() == 1,
          "only the original controller retires native cell lifetime; CPU wrapper registration remains a separate explicit borrow");
    registryContext.operation = 2;
    registryContext.handle = 1;
    registryContext.output = nullptr;
    invokeRegistry();
    Check(registryContext.result != ClockResult::Ok && !registryContext.output && !runtime.stopping.load(),
          "raw native presence after registered-token revocation cannot publish a cached SDK wrapper");
    Check(registration.Reset() == ClockResult::Ok && !registration.IsBound() && !registry.GetPedCount() &&
              ClockPedReferenceOwnerTest::unregisters == unregisters + 1,
          "CPU guard withdrawal never releases another native cell or duplicates native cleanup");
    wrapper.reset();
    Check(destroyed == 1, "factory-owned complete test wrapper is destroyed once after its link is withdrawn");
    retire(registryService);
    std::puts("Production actual native borrowed ped pool registry callback fixture completed");
}

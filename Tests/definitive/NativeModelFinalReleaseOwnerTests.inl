/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep explicit final-reference policy on the existing callback owner
 ****************************************************************************/
{
    using ZeroPolicy = NativeModelReferenceRelease::ZeroPolicy;
    // This existing graph qualifies count storage, not full loaded removal.
    // RemoveAtZero must refuse before CAS; its accepted guarded native path is
    // exercised by the primitive's separate complete loaded-removal fixture.
    const std::uintptr_t missingHeap = 0;
    admit(NativeModelReferenceTransactionProfile::HeapRva, &missingHeap, sizeof(missingHeap), PAGE_READWRITE);
    admit(NativeModelReferenceTransactionProfile::HeapAuthorityRva, &missingHeap, sizeof(missingHeap), PAGE_READWRITE);
    Check(absent.ReleaseFinalModelReference(3, ZeroPolicy::KeepResident) == ClockResult::NotInitialized,
          "older owners cannot acknowledge an explicit final-reference transition");
    context.operation = ModelOwnershipOperation::FinalReference;
    context.index = 3;
    context.zeroPolicy = ZeroPolicy::KeepResident;
    for (unsigned scenario = 0; scenario != 4; ++scenario)
    {
        fixture.Put(P::RootRva, root);
        reference.count = 1;
        seed(3, 1);
        ClockModelReferenceOwnerTest::result = scenario == 1 ? ClockResult::OriginalException : ClockResult::Ok;
        ClockModelReferenceOwnerTest::conflict = scenario == 2;
        ClockModelReferenceOwnerTest::loseRoot = scenario == 3;
        const auto         calls = ClockModelReferenceOwnerTest::calls, removals = ClockStreamingRemoveOwnerTest::calls;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        Check(runtime.access.ReleaseFinalModelReference(3, ZeroPolicy::KeepResident) != ClockResult::Ok && ClockModelReferenceOwnerTest::calls == calls,
              "retained generation does not grant final-reference mutation outside actual native callback");
        if (!scenario)
        {
            ClockResult foreign = ClockResult::Ok;
            std::thread other([&] { foreign = runtime.access.ReleaseFinalModelReference(3, ZeroPolicy::KeepResident); });
            other.join();
            Check(foreign != ClockResult::Ok && ClockModelReferenceOwnerTest::calls == calls,
                  "foreign thread cannot borrow explicit final-reference intent from a retained owner");
            context.operation = ModelOwnershipOperation::Reference;
            context.delta = -1;
            fixture.invokeTimer();
            Check(context.result == ClockResult::UnsupportedBuild && reference.count == 1 && ClockModelReferenceOwnerTest::calls == calls,
                  "ordinary interior decrement still refuses1-to0 without the explicit new policy");
            context.operation = ModelOwnershipOperation::FinalReference;
            for (const auto count : std::array<std::uint32_t, 2>{0, 2})
            {
                reference.count = count;
                Check(service.Renew(runtime.generation.load()), "renew exact-final count refusal");
                fixture.invokeTimer();
                Check(context.result == ClockResult::InvalidArgument && reference.count == count && ClockModelReferenceOwnerTest::calls == calls &&
                          !runtime.stopping.load(),
                      "explicit final operation requires exactly one current reference and never substitutes an interior decrement");
            }
            reference.count = 1;
            context.index = 0xffffffff;
            Check(service.Renew(runtime.generation.load()), "renew full-DWORD final key refusal");
            fixture.invokeTimer();
            Check(context.result == ClockResult::InvalidArgument && reference.count == 1 && ClockModelReferenceOwnerTest::calls == calls,
                  "full model key is not narrowed into a final-reference target");
            context.index = 3;
            context.zeroPolicy = static_cast<ZeroPolicy>(2);
            Check(service.Renew(runtime.generation.load()), "renew explicit final policy validation");
            fixture.invokeTimer();
            Check(context.result == ClockResult::InvalidArgument && reference.count == 1 && ClockModelReferenceOwnerTest::calls == calls,
                  "unknown zero policy cannot silently become keep-resident or removal");
            context.zeroPolicy = ZeroPolicy::RemoveAtZero;
            Check(service.Renew(runtime.generation.load()), "renew honest loaded-removal prerequisite refusal");
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && reference.count == 1 && ClockModelReferenceOwnerTest::calls == calls &&
                      ClockStreamingRemoveOwnerTest::calls == removals && !runtime.stopping.load(),
                  "incomplete loaded-removal prerequisites refuse before final CAS without assuming custom-clump/col absence");
            context.zeroPolicy = ZeroPolicy::KeepResident;
            Check(service.Renew(runtime.generation.load()), "renew explicit resident policy after clean removal refusal");
        }
        fixture.invokeTimer();
        Check(ClockModelReferenceOwnerTest::calls == calls + 1 && ClockModelReferenceOwnerTest::before == 1 && ClockModelReferenceOwnerTest::target == 0 &&
                  ClockModelReferenceOwnerTest::address == reinterpret_cast<std::uintptr_t>(&reference.count) &&
                  ClockStreamingRemoveOwnerTest::calls == removals,
              "actual owner performs one exact selected-key1-to0 CAS and keep-resident never calls streaming removal");
        if (!scenario)
        {
            std::uint32_t allocationBits = 0;
            std::memcpy(&allocationBits, map + 0x10, sizeof(allocationBits));
            Check(context.result == ClockResult::Ok && reference.count == 0 && reference.key == 3 && reference.next == -1 && allocationBits == 1 &&
                      !runtime.stopping.load(),
                  "accepted zero remains the same occupied keyed row without freeing it or manufacturing a model lease");
        }
        else
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && reference.count == (scenario == 2 ? 8u : 0u),
                  "entered uncertainty, conflicting count or lost root stops exact current owner and preserves native/foreign effects");
        retire(service);
        Check(ClockModelReferenceOwnerTest::calls == calls + 1 && ClockStreamingRemoveOwnerTest::calls == removals &&
                  reference.count == (scenario == 2 ? 8u : 0u),
              "retirement never repeats final CAS/removal or rolls an accepted zero back to a previous owner count");
    }
    fixture.Put(P::RootRva, root);
    reference.count = 2;
    context.zeroPolicy = ZeroPolicy::KeepResident;
    ClockModelReferenceOwnerTest::result = ClockResult::Ok;
    ClockModelReferenceOwnerTest::conflict = ClockModelReferenceOwnerTest::loseRoot = false;
    std::puts("Production native final model-reference callback owner fixture completed");
}

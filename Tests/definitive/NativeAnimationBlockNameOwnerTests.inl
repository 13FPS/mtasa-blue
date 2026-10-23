/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate mutable native names through the actual callback owner
 ****************************************************************************/
namespace
{
    struct AnimationNameOwnerContext
    {
        NativeAnimationBlockIdentity identity;
        char*                        output = reinterpret_cast<char*>(std::uintptr_t{0x12345678});
        char*                        afterEdit = reinterpret_cast<char*>(std::uintptr_t{0x87654321});
        CAnimBlock*                  sdk = nullptr;
        ClockResult                  result = ClockResult::NotInitialized, editedResult = ClockResult::Ok;
        unsigned                     operation = 0, refusal = 0;
        char                         firstByte = 0;
        bool                         threw = false, nativeImage = false;
    };
    void AnimationNameOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto&      context = *static_cast<AnimationNameOwnerContext*>(opaque);
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            ++timingCallbackGeneration;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        context.threw = context.nativeImage = false;
        try
        {
            if (context.operation == 1)
            {
                context.output = context.sdk->GetName();
                context.result = ClockResult::Ok;
            }
            else
                context.result = access.ReadAnimationBlockName(context.identity, context.output);
            if (context.result == ClockResult::Ok)
            {
                // Consume the borrow immediately, within this callback and
                // before another native operation. Never dereference it later.
                MEMORY_BASIC_INFORMATION page{};
                context.nativeImage = VirtualQuery(context.output, &page, sizeof(page)) == sizeof(page) && page.Type == MEM_IMAGE &&
                                      (page.Protect == PAGE_READWRITE || page.Protect == PAGE_WRITECOPY);
                context.firstByte = context.output[0];
                if (context.operation == 2)
                {
                    context.output[0] = 'X';
                    context.editedResult = access.ReadAnimationBlockName(context.identity, context.afterEdit);
                }
            }
        }
        catch (const std::runtime_error&)
        {
            context.threw = true;
            context.result = ClockResult::ReadFailure;
        }
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }
    void NativeAnimationBlockNamesOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativeAnimationBlockProfile;
        const auto                     sentinel = reinterpret_cast<char*>(std::uintptr_t{0x12345678});
        const auto                     afterEditSentinel = reinterpret_cast<char*>(std::uintptr_t{0x87654321});
        const NativeAnimationBlockName name{'p', 'e', 'd', 0};
        AnimationNameOwnerContext      context;
        context.identity.name = name;
        const auto nativeName = reinterpret_cast<char*>(fixture.image + P::BankRva);
        struct NameOwnerSdk final : CAnimBlockDE
        {
            NameOwnerSdk(NativeAnimationBlockAccess& access, const NativeAnimationBlockIdentity& identity) : CAnimBlockDE(access, identity) {}
            // Only the actual native name and metadata overrides are supplied
            // by production. This concrete test leaf rejects all other calls.
            [[noreturn]] static void Unsupported()
            {
                std::fputs("Unexpected unqualified animation name SDK call\n", stderr);
                std::abort();
            }
            CAnimBlockSAInterface*          GetInterface() override { Unsupported(); }
            void                            AddRef() override { Unsupported(); }
            unsigned short                  GetRefs() override { Unsupported(); }
            void                            Request(EModelRequestType, bool) override { Unsupported(); }
            CAnimBlendHierarchySAInterface* GetAnimationHierarchyInterface(std::size_t) override { Unsupported(); }
        };
        struct DefaultNameOwner final : NativeGameTimingAccess
        {
            ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
            ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
            ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        } defaultOwner;
        Check(defaultOwner.ReadAnimationBlockName(context.identity, context.output) == ClockResult::NotInitialized && context.output == sentinel,
              "older timing owners preserve a name output instead of manufacturing a copy or null success");
        Check(runtime.access.ReadAnimationBlockName(context.identity, context.output) != ClockResult::Ok && context.output == sentinel,
              "unbound native image/name identity does not grant a mutable pointer");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "natural callback retires the immediate name owner");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !runtime.pedReferences.IsRetained() &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "name borrow retirement creates no block lease or native reference cell");
        };
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &AnimationNameOwnerHandler, &context);
        context.identity.ownerGeneration = runtime.generation.load();
        const auto   originalGeneration = context.identity.ownerGeneration;
        NameOwnerSdk sdk(runtime.access, context.identity);
        context.sdk = &sdk;
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew only the current immediate-name owner");
            ClockAnimationOwnerTest::reads = 0;
            fixture.invokeTimer();
        };
        invoke();
        Check(context.result == ClockResult::Ok && context.output == nativeName && context.nativeImage && context.firstByte == 'p' &&
                  ClockAnimationOwnerTest::reads == 2 && !runtime.pedReferences.IsRetained(),
              "actual runtime commits the selected writable MEM_IMAGE name address after complete native row qualification");
        context.operation = 1;
        invoke();
        Check(!context.threw && context.output == nativeName && context.nativeImage && context.firstByte == 'p' && ClockAnimationOwnerTest::reads == 2,
              "actual SDK GetName performs one fresh name dispatch, with no wrapper copy or extra metadata dispatch");
        context.output = sentinel;
        Check(runtime.access.ReadAnimationBlockName(context.identity, context.output) != ClockResult::Ok && context.output == sentinel,
              "the mutable result is no authority for a second read between callbacks");
        char*       foreignOutput = sentinel;
        ClockResult foreign = ClockResult::Ok;
        std::thread other([&] { foreign = runtime.access.ReadAnimationBlockName(context.identity, foreignOutput); });
        other.join();
        Check(foreign != ClockResult::Ok && foreignOutput == sentinel, "foreign thread cannot obtain an immediate native name borrow");
        context.operation = 0;
        for (unsigned refusal = 1; refusal != 5; ++refusal)
        {
            context.refusal = refusal;
            context.output = sentinel;
            invoke();
            Check(context.result != ClockResult::Ok && context.output == sentinel && !ClockAnimationOwnerTest::reads && !runtime.stopping.load(),
                  "callback/admission/depth/lease refusal preserves pointer and never stops a valid unrelated owner");
        }
        context.refusal = 0;
        for (const auto affinity : {std::uint64_t{0}, originalGeneration + 1})
        {
            context.identity.ownerGeneration = affinity;
            invoke();
            Check(context.result == ClockResult::OwnerConflict && context.output == sentinel && !ClockAnimationOwnerTest::reads && !runtime.stopping.load(),
                  "missing or foreign CPU generation cannot silently adopt an identical native name");
        }
        context.identity.ownerGeneration = originalGeneration;
        for (unsigned action = 0; action != 6; ++action)
        {
            context.output = sentinel;
            ClockAnimationOwnerTest::identity = &context.identity;
            if (action == 0)
                ClockAnimationOwnerTest::failNext = true;
            if (action == 1)
                ClockAnimationOwnerTest::afterRead = &ClockAnimationOwnerTest::ChangeAffinity;
            if (action == 2)
                ClockAnimationOwnerTest::afterRead = &ClockAnimationOwnerTest::ChangeIndex;
            if (action == 3)
                ClockAnimationOwnerTest::afterRead = &ClockAnimationOwnerTest::ChangeName;
            if (action == 4)
                ClockAnimationOwnerTest::afterRead = &RevokeAnimationAdmission;
            if (action == 5)
            {
                ClockAnimationOwnerTest::changedAddress = fixture.image + P::CountRva;
                ClockAnimationOwnerTest::changedValue = 0;
                ClockAnimationOwnerTest::afterRead = &ClockAnimationOwnerTest::StoreWord;
            }
            invoke();
            Check(context.result != ClockResult::Ok && context.output == sentinel && !runtime.stopping.load(),
                  "failed native read, late CPU/callback identity or native count loss never publishes a name pointer or Stops the runtime");
            context.identity.ownerGeneration = originalGeneration;
            context.identity.index = 0;
            context.identity.name = name;
            fixture.Put(P::CountRva, static_cast<std::int32_t>(P::ObservationLimit));
        }
        context.operation = 2;
        invoke();
        Check(context.result == ClockResult::Ok && context.output == nativeName && context.firstByte == 'p' && fixture.Get<unsigned char>(P::BankRva) == 'X' &&
                  context.editedResult == ClockResult::BindingChanged && context.afterEdit == afterEditSentinel && !runtime.stopping.load(),
              "immediate SDK-compatible mutation changes genuine native storage; the old copied identity then refuses instead of hiding the edit");
        fixture.Put(P::BankRva, name);
        context.operation = 1;
        invoke();
        Check(!context.threw && context.output == nativeName, "a newly qualified immediate call can observe the restored current name without replay");
        retire(service);
        context.output = sentinel;
        Check(runtime.access.ReadAnimationBlockName(context.identity, context.output) != ClockResult::Ok && context.output == sentinel,
              "End revokes callback reads and grants no promise that an earlier pointer remains a valid borrow");

        BeginOwned(service, fixture, false, true, true, &AnimationNameOwnerHandler, &context);
        Check(runtime.generation.load() != originalGeneration, "new name owner generation is distinct");
        context.output = sentinel;
        invoke();
        Check(context.threw && context.output == sentinel && !ClockAnimationOwnerTest::reads && !runtime.stopping.load(),
              "actual old SDK object refuses across End/new Begin despite identical index and name");
        context.identity.ownerGeneration = runtime.generation.load();
        NameOwnerSdk fresh(runtime.access, context.identity);
        context.sdk = &fresh;
        invoke();
        Check(!context.threw && context.output == nativeName && context.nativeImage && ClockAnimationOwnerTest::reads == 2,
              "fresh same-owner SDK name receiver succeeds without reviving the stale receiver or creating a registry");
        retire(service);
        std::puts("Production native immediate animation name callback owner fixture completed");
        std::puts("Production actual mutable animation name SDK callback fixture completed");
    }
}

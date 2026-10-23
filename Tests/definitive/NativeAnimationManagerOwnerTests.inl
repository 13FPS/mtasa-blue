/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual signed counters and copied-name lookup on one runtime owner
 ****************************************************************************/
namespace
{
    struct AnimationManagerOwnerContext
    {
        NativeAnimationManagerCounts output{-12345, -23456};
        NativeAnimationBlockName     query{'P', 'E', 'D', 0};
        std::uint64_t                generation = 0;
        CAnimManager*                sdk = nullptr;
        ClockResult                  result = ClockResult::NotInitialized;
        std::int32_t                 index = -123456;
        int                          blocks = 0, animations = 0;
        unsigned                     operation = 0, refusal = 0;
        bool                         threw = false;
    };
    void AnimationManagerOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto&      context = *static_cast<AnimationManagerOwnerContext*>(opaque);
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
        context.threw = false;
        try
        {
            switch (context.operation)
            {
                case 0:
                    context.result = access.ReadAnimationManagerCounts(context.generation, context.output);
                    break;
                case 1:
                    context.result = access.FindAnimationBlockIndex(context.generation, context.query, context.index);
                    break;
                case 2:
                    context.blocks = context.sdk->GetNumAnimBlocks();
                    context.result = ClockResult::Ok;
                    break;
                case 3:
                    context.animations = context.sdk->GetNumAnimations();
                    context.result = ClockResult::Ok;
                    break;
                case 4:
                    context.index = context.sdk->GetAnimationBlockIndex(reinterpret_cast<const char*>(context.query.data()));
                    context.result = ClockResult::Ok;
                    break;
                default:
                    std::abort();
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
    void NativeAnimationManagerOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativeAnimationManagerProfile;
        namespace B = NativeAnimationBlockProfile;
        struct Saved
        {
            std::uintptr_t             rva;
            std::vector<unsigned char> bytes;
            DWORD                      protection;
        };
        std::vector<Saved> saved;
        const auto         admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            MEMORY_BASIC_INFORMATION page{};
            Check(VirtualQuery(reinterpret_cast<void*>(fixture.image + rva), &page, sizeof(page)) == sizeof(page),
                  "query additional owned animation-manager protection");
            Saved  old{rva, std::vector<unsigned char>(size), page.Protect};
            SIZE_T copied = 0;
            Check(ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), old.bytes.data(), size, &copied) && copied == size,
                  "save only extra manager pins/fold/counters/names, not another animation registry");
            saved.push_back(std::move(old));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "retain actual manager code/table/data protections");
        };
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(P::NameFoldRva, P::NameFold.data(), P::NameFold.size(), PAGE_READONLY);
        const std::int32_t blocks = 3, animations = 65537;
        admit(B::CountRva, &blocks, sizeof(blocks), PAGE_READWRITE);
        admit(P::AnimationCountRva, &animations, sizeof(animations), PAGE_READWRITE);
        const NativeAnimationBlockName duplicate{'P', 'e', 'D', 0}, last{'l', 'a', 's', 't', 0}, query{'P', 'E', 'D', 0};
        admit(B::BankRva + B::RowBytes, duplicate.data(), duplicate.size(), PAGE_READWRITE);
        admit(B::BankRva + 2 * B::RowBytes, last.data(), last.size(), PAGE_READWRITE);
        ClockAnimationManagerOwnerTest::watched = fixture.image + B::CountRva;
        struct DefaultManagerOwner final : NativeGameTimingAccess
        {
            ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
            ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
            ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        } defaultOwner;
        AnimationManagerOwnerContext context;
        const auto                   sentinel = context.output;
        const auto                   untouched = [&] { return context.output.blocks == sentinel.blocks && context.output.animations == sentinel.animations; };
        Check(defaultOwner.ReadAnimationManagerCounts(0, context.output) == ClockResult::NotInitialized && untouched() &&
                  defaultOwner.FindAnimationBlockIndex(0, context.query, context.index) == ClockResult::NotInitialized && context.index == -123456,
              "older timing owner refuses both manager observations without fabricating zero counters or missing-name -1");
        Check(runtime.access.ReadAnimationManagerCounts(0, context.output) != ClockResult::Ok && untouched(),
              "unbound animation manager is no count authority");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "natural callback retires animation manager reads");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !runtime.pedReferences.IsRetained() &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "manager observations retire real admission without a block registry or hierarchy factory");
        };
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &AnimationManagerOwnerHandler, &context);
        context.generation = runtime.generation.load();
        const auto               originalGeneration = context.generation;
        AnimationManagerOwnerSdk sdk(runtime.access, context.generation);
        context.sdk = &sdk;
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the real manager observation owner");
            ClockAnimationManagerOwnerTest::reads = 0;
            fixture.invokeTimer();
        };
        invoke();
        Check(context.result == ClockResult::Ok && context.output.blocks == blocks && context.output.animations == animations &&
                  ClockAnimationManagerOwnerTest::reads == 2,
              "runtime publishes two coherent signed native counters without inferring a loaded sum");
        for (const unsigned operation : {2u, 3u, 4u})
        {
            context.operation = operation;
            invoke();
            Check(!context.threw && context.result == ClockResult::Ok && ClockAnimationManagerOwnerTest::reads != 0,
                  "actual SDK counts and folded index methods freshly dispatch through this runtime");
        }
        Check(context.blocks == blocks && context.animations == animations && context.index == 0,
              "SDK folded duplicate lookup chooses first current row and keeps hierarchy extent independent from block count");
        context.query = last;
        invoke();
        Check(!context.threw && context.index == 2, "SDK lookup uses actual copied native name rather than a loaded/model mapping");
        context.query = NativeAnimationBlockName{'m', 'i', 's', 's', 0};
        invoke();
        Check(!context.threw && context.index == -1 && !runtime.stopping.load(),
              "genuinely missing copied name yields qualified -1, distinct from owner refusal");
        for (const auto nativeBlocks : {std::int32_t{0}, std::int32_t{-1}, (std::numeric_limits<std::int32_t>::min)()})
        {
            fixture.Put(B::CountRva, nativeBlocks);
            fixture.Put(P::AnimationCountRva, (std::numeric_limits<std::int32_t>::max)());
            context.operation = 0;
            invoke();
            Check(context.result == ClockResult::Ok && context.output.blocks == nativeBlocks &&
                      context.output.animations == (std::numeric_limits<std::int32_t>::max)(),
                  "real counter API preserves full signed storage values, independently from bounded lookup work");
            context.operation = 1;
            context.index = -123456;
            invoke();
            Check(context.result == ClockResult::Ok && context.index == -1, "genuine nonpositive native count is stable qualified missing-name state");
        }
        fixture.Put(B::CountRva, static_cast<std::int32_t>(B::ObservationLimit + 1));
        context.index = -123456;
        invoke();
        Check(context.result != ClockResult::Ok && context.index == -123456 && !runtime.stopping.load(),
              "unsupported positive lookup extent remains refusal rather than false missing-name -1");
        fixture.Put(B::CountRva, blocks);
        fixture.Put(P::AnimationCountRva, animations);
        context.query = query;
        context.operation = 0;
        context.output = sentinel;
        Check(runtime.access.ReadAnimationManagerCounts(context.generation, context.output) != ClockResult::Ok && untouched(),
              "copied manager affinity cannot read between callbacks");
        ClockResult                  foreign = ClockResult::Ok;
        NativeAnimationManagerCounts foreignOutput = sentinel;
        std::thread                  other([&] { foreign = runtime.access.ReadAnimationManagerCounts(context.generation, foreignOutput); });
        other.join();
        Check(foreign != ClockResult::Ok && foreignOutput.blocks == sentinel.blocks && foreignOutput.animations == sentinel.animations,
              "foreign thread cannot borrow current counters");
        for (unsigned operation = 0; operation != 2; ++operation)
            for (unsigned refusal = 1; refusal != 5; ++refusal)
            {
                context.operation = operation;
                context.refusal = refusal;
                context.output = sentinel;
                context.index = -123456;
                invoke();
                Check(context.result != ClockResult::Ok && untouched() && context.index == -123456 && !ClockAnimationManagerOwnerTest::reads &&
                          !runtime.stopping.load(),
                      "both manager methods preserve outputs under callback/admission/depth/lease refusal");
            }
        context.refusal = 0;
        for (const auto affinity : {std::uint64_t{0}, originalGeneration + 1})
        {
            context.generation = affinity;
            invoke();
            Check(
                context.result == ClockResult::OwnerConflict && context.index == -123456 && !ClockAnimationManagerOwnerTest::reads && !runtime.stopping.load(),
                "zero/stale manager affinity refuses before native lookup");
        }
        context.generation = originalGeneration;
        for (unsigned action = 0; action != 4; ++action)
        {
            context.operation = action == 2 ? 1u : 0u;
            context.output = sentinel;
            context.index = -123456;
            if (action == 0)
                ClockAnimationManagerOwnerTest::failNext = true;
            if (action == 1)
                ClockAnimationManagerOwnerTest::afterRead = &RevokeAnimationAdmission;
            if (action == 2)
            {
                ClockAnimationManagerOwnerTest::query = &context.query;
                ClockAnimationManagerOwnerTest::afterRead = &ClockAnimationManagerOwnerTest::ChangeQuery;
            }
            if (action == 3)
            {
                ClockAnimationManagerOwnerTest::watched = fixture.image + P::AnimationCountRva;
                ClockAnimationManagerOwnerTest::changedAddress = fixture.image + P::AnimationCountRva;
                ClockAnimationManagerOwnerTest::changedValue = 17;
                ClockAnimationManagerOwnerTest::afterRead = &ClockAnimationManagerOwnerTest::StoreWord;
            }
            invoke();
            Check(context.result != ClockResult::Ok && untouched() && context.index == -123456 && !runtime.stopping.load(),
                  "read fault, late admission/query mutation and counter change preserve all manager outputs without Stop or replay");
            context.query = query;
            fixture.Put(P::AnimationCountRva, animations);
            ClockAnimationManagerOwnerTest::watched = fixture.image + B::CountRva;
        }
        context.operation = 0;
        invoke();
        Check(context.result == ClockResult::Ok && !runtime.stopping.load(),
              "later separately qualified native observation succeeds without resetting a failed read");
        retire(service);
        BeginOwned(service, fixture, false, true, true, &AnimationManagerOwnerHandler, &context);
        Check(runtime.generation.load() != originalGeneration, "next runtime manager has a distinct generation");
        context.operation = 2;
        invoke();
        Check(context.threw && !ClockAnimationManagerOwnerTest::reads && !runtime.stopping.load(),
              "old actual manager SDK receiver cannot revive across End/new Begin");
        context.generation = runtime.generation.load();
        AnimationManagerOwnerSdk fresh(runtime.access, context.generation);
        context.sdk = &fresh;
        invoke();
        Check(!context.threw && context.blocks == blocks && ClockAnimationManagerOwnerTest::reads == 2,
              "fresh explicitly affiliated manager receiver reads same native bank without reviving old wrapper");
        retire(service);
        ClockAnimationManagerOwnerTest::watched = ClockAnimationManagerOwnerTest::changedAddress = 0;
        ClockAnimationManagerOwnerTest::query = nullptr;
        ClockAnimationManagerOwnerTest::afterRead = nullptr;
        ClockAnimationManagerOwnerTest::failNext = false;
        for (auto it = saved.rbegin(); it != saved.rend(); ++it)
        {
            fixture.Patch(it->rva, it->bytes.data(), it->bytes.size());
            DWORD ignored = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + it->rva), it->bytes.size(), it->protection, &ignored) != FALSE,
                  "restore only manager-specific owned fixture bytes/pages before common profile restoration");
        }
        std::puts("Production native animation manager callback owner fixture completed");
        std::puts("Production actual animation manager SDK callback fixture completed");
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real animation metadata SDK reads on the retained callback owner
 ****************************************************************************/
namespace
{
    bool SameAnimationValues(const NativeAnimationBlockValues& left, const NativeAnimationBlockValues& right)
    {
        return left.index == right.index && left.idOffset == right.idOffset && left.animationCount == right.animationCount && left.loaded == right.loaded;
    }
    void RevokeAnimationAdmission() noexcept
    {
        runtime.admissionGeneration.fetch_add(1);
    }
    struct AnimationBlockOwnerContext
    {
        NativeAnimationBlockIdentity identity;
        NativeAnimationBlockValues   output{0xe7e7e7e7u, -12345, 0xdeadbeefu, 0xe7};
        CAnimBlock*                  sdk = nullptr;
        ClockResult                  result = ClockResult::NotInitialized;
        unsigned                     operation = 0, refusal = 0;
        int                          index = -999, id = -999;
        std::size_t                  count = 0;
        bool                         loaded = false, threw = false;
    };
    void AnimationBlockOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto&      context = *static_cast<AnimationBlockOwnerContext*>(opaque);
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
                    context.result = access.ReadAnimationBlock(context.identity, context.output);
                    break;
                case 1:
                    context.index = context.sdk->GetIndex();
                    context.result = ClockResult::Ok;
                    break;
                case 2:
                    context.loaded = context.sdk->IsLoaded();
                    context.result = ClockResult::Ok;
                    break;
                case 3:
                    context.id = context.sdk->GetIDOffset();
                    context.result = ClockResult::Ok;
                    break;
                case 4:
                    context.count = context.sdk->GetAnimationCount();
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

    void NativeAnimationBlocksOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativeAnimationBlockProfile;
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
            Check(::VirtualQuery(reinterpret_cast<void*>(fixture.image + rva), &page, sizeof(page)) == sizeof(page),
                  "query only bounded owned animation profile/data protection");
            Saved  old{rva, std::vector<unsigned char>(size), page.Protect};
            SIZE_T copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), old.bytes.data(), size, &copied) && copied == size,
                  "preserve task-owned animation profile/data bytes");
            saved.push_back(std::move(old));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "admit actual code RX, immutable group slot RO and native data RW");
        };
        for (const auto& pin : NativePedPoolProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        const auto group = fixture.image + P::GroupRva;
        const auto selector = static_cast<unsigned char>(P::LoadedSelector);
        const auto branch = static_cast<std::uint32_t>(P::LoadedBranchRva);
        admit(P::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
        // These genuine dispatch entries share executable .text pages. Giving
        // a table entry RO protection would invalidate adjacent pinned code.
        admit(P::RemapRva + P::LoadedOpcodeIndex, &selector, sizeof(selector), PAGE_EXECUTE_READ);
        admit(P::DispatchRva + P::LoadedSelector * 4, &branch, sizeof(branch), PAGE_EXECUTE_READ);
        const std::uintptr_t zero = 0;
        const std::uint32_t  playing = 9, gc = 0;
        const unsigned char  initialized = 1;
        const auto           thread = GetCurrentThreadId();
        const std::int32_t   blockCount = P::ObservationLimit;
        admit(NativePedPoolProfile::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &playing, sizeof(playing), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &gc, sizeof(gc), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, sizeof(initialized), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(P::CountRva, &blockCount, sizeof(blockCount), PAGE_READWRITE);
        const NativeAnimationBlockName         name{'p', 'e', 'd', 0}, lastName{'l', 'a', 's', 't', 0};
        std::array<unsigned char, P::RowBytes> row{};
        const std::int32_t                     id = (std::numeric_limits<std::int32_t>::min)();
        const std::uint32_t                    animations = 0xffffffffu;
        std::memcpy(row.data(), name.data(), name.size());
        row[P::LoadedOffset] = 0x80;
        std::memcpy(row.data() + P::IdOffset, &id, sizeof(id));
        std::memcpy(row.data() + P::AnimationCountOffset, &animations, sizeof(animations));
        admit(P::BankRva, row.data(), row.size(), PAGE_READWRITE);
        auto lastRow = row;
        std::memcpy(lastRow.data(), lastName.data(), lastName.size());
        admit(P::BankRva + (P::ObservationLimit - 1) * P::RowBytes, lastRow.data(), lastRow.size(), PAGE_READWRITE);
        ClockReaderTestHost::image = fixture.image;
        ClockAnimationOwnerTest::row = fixture.image + P::BankRva;

        struct AnimationOwnerSdk final : CAnimBlockDE
        {
            AnimationOwnerSdk(NativeAnimationBlockAccess& access, const NativeAnimationBlockIdentity& identity) : CAnimBlockDE(access, identity) {}
            // A test-only complete leaf diagnoses unexpected dispatch. It
            // provides no successful native pointer, request or ref service.
            [[noreturn]] static void Unsupported()
            {
                std::fputs("Unexpected unqualified animation SDK method\n", stderr);
                std::abort();
            }
            CAnimBlockSAInterface*          GetInterface() override { Unsupported(); }
            char*                           GetName() override { Unsupported(); }
            void                            AddRef() override { Unsupported(); }
            unsigned short                  GetRefs() override { Unsupported(); }
            void                            Request(EModelRequestType, bool) override { Unsupported(); }
            CAnimBlendHierarchySAInterface* GetAnimationHierarchyInterface(std::size_t) override { Unsupported(); }
        };
        struct DefaultAnimationOwner final : NativeGameTimingAccess
        {
            ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
            ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
            ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        } defaultOwner;
        AnimationBlockOwnerContext context;
        context.identity.name = name;
        const auto sentinel = context.output;
        Check(defaultOwner.ReadAnimationBlock(context.identity, context.output) == ClockResult::NotInitialized && SameAnimationValues(context.output, sentinel),
              "older timing-only owners refuse the additional read without fabricating an absent block");
        Check(runtime.access.ReadAnimationBlock(context.identity, context.output) != ClockResult::Ok && SameAnimationValues(context.output, sentinel),
              "unbound animation identity is not callback authority");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "natural owner callback acknowledges read-only animation retirement");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !runtime.pedReferences.IsRetained() &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "animation observations retire real callback admission without ped cells or a block registry");
        };
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &AnimationBlockOwnerHandler, &context);
        context.identity.ownerGeneration = runtime.generation.load();
        const auto        firstGeneration = context.identity.ownerGeneration;
        AnimationOwnerSdk sdk(runtime.access, context.identity);
        context.sdk = &sdk;
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the sole real animation owner");
            ClockAnimationOwnerTest::reads = 0;
            fixture.invokeTimer();
        };
        invoke();
        Check(context.result == ClockResult::Ok && context.output.index == 0 && context.output.idOffset == id && context.output.animationCount == animations &&
                  context.output.loaded == 0x80 && ClockAnimationOwnerTest::reads == 2,
              "actual owner returns current signed ID/full unsigned count/raw loaded byte after two coherent row copies");
        for (unsigned operation = 1; operation != 5; ++operation)
        {
            context.operation = operation;
            invoke();
            Check(context.result == ClockResult::Ok && !context.threw && ClockAnimationOwnerTest::reads == 2,
                  "each actual SDK virtual performs one fresh native observation rather than returning cached scalar metadata");
        }
        Check(context.index == 0 && context.loaded && context.id == id && context.count == static_cast<std::size_t>(animations),
              "actual SDK preserves loaded !=0, signed ID and full DWORD animation count on x64");
        for (const auto rawLoaded : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{255}})
        {
            fixture.Put(P::BankRva + P::LoadedOffset, rawLoaded);
            context.operation = 2;
            invoke();
            Check(!context.threw && context.loaded == (rawLoaded != 0), "loaded SDK predicate follows the observed raw byte");
        }
        fixture.Put(P::BankRva, row);
        context.operation = 0;
        context.output = sentinel;
        Check(runtime.access.ReadAnimationBlock(context.identity, context.output) != ClockResult::Ok && SameAnimationValues(context.output, sentinel),
              "a retained SDK identity cannot read between callbacks");
        ClockResult                foreign = ClockResult::Ok;
        NativeAnimationBlockValues foreignOutput = sentinel;
        std::thread                other([&] { foreign = runtime.access.ReadAnimationBlock(context.identity, foreignOutput); });
        other.join();
        Check(foreign != ClockResult::Ok && SameAnimationValues(foreignOutput, sentinel), "foreign thread refuses without changing caller output");
        for (unsigned refusal = 1; refusal != 5; ++refusal)
        {
            context.refusal = refusal;
            context.output = sentinel;
            invoke();
            Check(context.result != ClockResult::Ok && SameAnimationValues(context.output, sentinel) && !runtime.stopping.load() &&
                      !ClockAnimationOwnerTest::reads,
                  "wrong callback/admission generation, nested depth and stale lease cannot borrow a native block read");
        }
        context.refusal = 0;
        for (const auto affinity : {std::uint64_t{0}, firstGeneration + 1})
        {
            context.identity.ownerGeneration = affinity;
            invoke();
            Check(context.result == ClockResult::OwnerConflict && SameAnimationValues(context.output, sentinel) && !ClockAnimationOwnerTest::reads &&
                      !runtime.stopping.load(),
                  "absent or foreign CPU affinity refuses before native observation without stopping the valid owner");
        }
        context.identity.ownerGeneration = firstGeneration;
        context.identity.index = P::ObservationLimit - 1;
        context.identity.name = lastName;
        ClockAnimationOwnerTest::row = fixture.image + P::BankRva + context.identity.index * P::RowBytes;
        invoke();
        Check(context.result == ClockResult::Ok && context.output.index == P::ObservationLimit - 1,
              "last supported currently counted block is an explicit index/name observation, not an inferred bank capacity");
        context.identity.index = 0;
        context.identity.name = name;
        ClockAnimationOwnerTest::row = fixture.image + P::BankRva;
        for (unsigned invalid = 0; invalid != 4; ++invalid)
        {
            context.output = sentinel;
            if (invalid == 0)
                context.identity.name[0] ^= 1;
            if (invalid == 1)
                fixture.Put(P::CountRva, std::int32_t{0});
            if (invalid == 2)
                fixture.Put(P::CountRva, std::int32_t{P::ObservationLimit + 1});
            if (invalid == 3)
                context.identity.index = P::ObservationLimit;
            invoke();
            Check(context.result != ClockResult::Ok && SameAnimationValues(context.output, sentinel) && !runtime.stopping.load(),
                  "name loss, current absence, unsupported native count and invalid index remain refusal, never zero metadata");
            context.identity.index = 0;
            context.identity.name = name;
            fixture.Put(P::CountRva, blockCount);
        }
        for (unsigned nativeRefusal = 0; nativeRefusal != 3; ++nativeRefusal)
        {
            context.output = sentinel;
            if (nativeRefusal == 0)
                fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{8});
            if (nativeRefusal == 1)
                fixture.Put(NativeBorrowedGcProfile::markerRva, std::uint32_t{1});
            if (nativeRefusal == 2)
                fixture.Put(NativeStartGameProfile::ThreadIdRva, DWORD{0});
            invoke();
            Check(context.result != ClockResult::Ok && SameAnimationValues(context.output, sentinel) && !runtime.stopping.load() &&
                      !ClockAnimationOwnerTest::reads,
                  "native loading state, GC activity and missing initialized native thread refuse before row access");
            fixture.Put(NativeGameStateProfile::stateRva, playing);
            fixture.Put(NativeBorrowedGcProfile::markerRva, gc);
            fixture.Put(NativeStartGameProfile::ThreadIdRva, thread);
        }
        for (unsigned action = 0; action != 8; ++action)
        {
            context.output = sentinel;
            if (action == 0)
                ClockAnimationOwnerTest::failNext = true;
            if (action == 1 || action == 2 || action == 4)
            {
                ClockAnimationOwnerTest::changedAddress = fixture.image + (action == 1   ? P::BankRva
                                                                           : action == 2 ? P::CountRva
                                                                                         : NativeBorrowedGcProfile::markerRva);
                ClockAnimationOwnerTest::changedValue = action == 1 ? 0x00646578u : action == 2 ? 0 : 1;
                ClockAnimationOwnerTest::afterRead = &ClockAnimationOwnerTest::StoreWord;
            }
            if (action == 3 || action == 5 || action == 6)
            {
                ClockAnimationOwnerTest::identity = &context.identity;
                ClockAnimationOwnerTest::afterRead = action == 3   ? &ClockAnimationOwnerTest::ChangeAffinity
                                                     : action == 5 ? &ClockAnimationOwnerTest::ChangeIndex
                                                                   : &ClockAnimationOwnerTest::ChangeName;
            }
            if (action == 7)
                ClockAnimationOwnerTest::afterRead = &RevokeAnimationAdmission;
            invoke();
            Check(context.result != ClockResult::Ok && SameAnimationValues(context.output, sentinel) && !runtime.stopping.load(),
                  "read fault, late native/callback loss and mutated CPU identity preserve the entire output with no Stop or replay");
            context.identity.ownerGeneration = firstGeneration;
            context.identity.index = 0;
            context.identity.name = name;
            fixture.Put(P::BankRva, row);
            fixture.Put(P::CountRva, blockCount);
            fixture.Put(NativeBorrowedGcProfile::markerRva, gc);
        }
        invoke();
        Check(context.result == ClockResult::Ok && !runtime.stopping.load() && !runtime.pedReferences.IsRetained(),
              "later independently qualified observation succeeds without cleanup debt, native writes or reference registration");
        retire(service);

        // Keep the actual typed SDK receiver alive across End/new Begin. Its
        // immutable CPU generation must not revive just because name/index did.
        BeginOwned(service, fixture, false, true, true, &AnimationBlockOwnerHandler, &context);
        Check(runtime.generation.load() != firstGeneration, "the new retained runtime has a distinct generation");
        context.operation = 1;
        context.sdk = &sdk;
        invoke();
        Check(context.threw && !ClockAnimationOwnerTest::reads && !runtime.stopping.load(),
              "old actual SDK receiver refuses across a new runtime even at the identical native index/name");
        context.identity.ownerGeneration = runtime.generation.load();
        AnimationOwnerSdk fresh(runtime.access, context.identity);
        context.sdk = &fresh;
        invoke();
        Check(!context.threw && context.index == 0 && ClockAnimationOwnerTest::reads == 2,
              "new explicitly affiliated SDK receiver uses the same genuine block without reviving the old receiver");
        retire(service);
        context.output = sentinel;
        Check(runtime.access.ReadAnimationBlock(context.identity, context.output) != ClockResult::Ok && SameAnimationValues(context.output, sentinel),
              "End removes callback authority while leaving caller-owned immutable identities untouched");
        ClockAnimationOwnerTest::row = ClockAnimationOwnerTest::changedAddress = 0;
        ClockAnimationOwnerTest::identity = nullptr;
        ClockAnimationOwnerTest::afterRead = nullptr;
        ClockAnimationOwnerTest::failNext = false;
        for (auto it = saved.rbegin(); it != saved.rend(); ++it)
        {
            fixture.Patch(it->rva, it->bytes.data(), it->bytes.size());
            DWORD ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + it->rva), it->bytes.size(), it->protection, &ignored) != FALSE,
                  "restore only this task's bounded fixture bytes/pages after actual owner retirement");
        }
        std::puts("Production native animation block callback owner fixture completed");
        std::puts("Production actual animation block SDK callback fixture completed");
    }
}

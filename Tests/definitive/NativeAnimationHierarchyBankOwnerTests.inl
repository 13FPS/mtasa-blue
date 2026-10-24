/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual hierarchy getter facade over one current native bank owner
 ****************************************************************************/
namespace AnimationHierarchyBankOwnerFixture
{
    namespace Host = ClockAnimationHierarchyBankOwnerTest;
    using Access = NativeAnimationHierarchyAccess;
    void ChangeScalar() noexcept
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, reinterpret_cast<void*>(Host::locator + 0x18), sizeof(bits));
        bits ^= 1;
        SIZE_T written = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(Host::locator + 0x18), &bits, sizeof(bits), &written) || written != sizeof(bits))
            std::abort();
    }
    void ChangePointer() noexcept
    {
        const std::uintptr_t zero = 0;
        SIZE_T               written = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(Host::locator + 8), &zero, sizeof(zero), &written) || written != sizeof(zero))
            std::abort();
    }
    void LoseGeneration() noexcept
    {
        ++timingCallbackGeneration;
    }
    NativeAnimationHierarchyReferenceToken* caller = nullptr;
    void                                    ChangeCaller() noexcept
    {
        caller->ownerGeneration = 0;
        caller->locator = 1;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace P = NativeAnimationBlockProfile;
        namespace M = NativeAnimationManagerProfile;
        namespace H = NativeAnimationBlockHierarchyProfile;
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        writes.Global(M::AnimationCountRva, std::int32_t{12});
        writes.Global(P::BankRva + P::IdOffset, std::int32_t{5});
        writes.Global(P::BankRva + P::AnimationCountOffset, std::uint32_t{3});
        writes.Global(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(1));
        NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes, 6, 0);
        NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes, 7, 0);
        const auto locator = fixture.image + H::BankRva + 6 * H::RowBytes;
        Host::locator = locator;
        Host::Reset();
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const std::uint64_t capacity = 65537;
        void*               children = VirtualAlloc(nullptr, 8 + static_cast<std::size_t>(capacity) * 80, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(children != nullptr, "own only the synthetic native child allocation used by this read fixture");
        std::memcpy(children, &capacity, sizeof(capacity));
        const auto childPointer = reinterpret_cast<std::uintptr_t>(children) + 8;
        Host::cookie = reinterpret_cast<std::uintptr_t>(children);
        const auto setChildren = [&](std::uintptr_t pointer)
        {
            fixture.Put(H::BankRva + 6 * H::RowBytes + 8, pointer);
            fixture.Put(H::BankRva + 6 * H::RowBytes + 0x10, std::uint16_t{0});
            fixture.Put(H::BankRva + 6 * H::RowBytes + 0x12, std::uint8_t{0x80});
        };
        setChildren(childPointer);
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        Access*                           access = nullptr;
        callback.action = [&](NativeGameTimingAccess& value) { access = dynamic_cast<Access*>(&value); };
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        fixture.invokeTimer();
        Check(access == static_cast<Access*>(&runtime.access), "actual callback cross-casts the same complete owner to optional hierarchy reads");
        NativeAnimationHierarchyReferenceToken token{generation, locator};
        auto                                   sdk = std::make_unique<animation_hierarchy_fixture::AnimationHierarchyReceiver>(*access, token);
        const auto                             invoke = [&]
        {
            Check(service.Renew(generation), "renew only the current hierarchy observation owner");
            fixture.invokeTimer();
        };
        NativeAnimationHierarchyValues values{1, 2, 3, 4, 5, 6};
        const auto                     sentinel = values;
        ClockResult                    result = ClockResult::NotInitialized;
        Check(access->ReadAnimationHierarchy(token, values) != ClockResult::Ok && values.interfaceAddress == sentinel.interfaceAddress && !Host::scalarReads,
              "native bank locator and facade do not authorize reads between callbacks");
        for (const std::uint32_t bits : {0u, 0x80000000u, 0x7fc01234u, 0x7f800001u, 0xffffffffu})
        {
            fixture.Put(H::BankRva + 6 * H::RowBytes + 0x18, bits);
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto    actual = sdk->GetTotalTime();
                std::uint32_t observed = 0;
                std::memcpy(&observed, &actual, sizeof(observed));
                Check(observed == bits && sdk->GetNameHashKey() == 0x13579bdf && !sdk->GetNumSequences() && sdk->IsRunningCompressed() && !sdk->IsCustom() &&
                          sdk->GetAnimBlockID() == 0 && reinterpret_cast<std::uintptr_t>(sdk->GetInterface()) == locator,
                      "actual scalar getters preserve raw float bits and source predicates with independent count and no custom-kind cache");
            };
            Host::Reset();
            invoke();
        }
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(reinterpret_cast<std::uintptr_t>(sdk->GetSequences()) == childPointer &&
                      reinterpret_cast<std::uintptr_t>(sdk->GetSequence(65536)) == childPointer + std::uintptr_t{65536} * 80,
                  "actual nested getters use the genuine full capacity cookie rather than the zero logical WORD count");
        };
        Host::Reset();
        invoke();
        NativeAnimationBlockName rawName;
        rawName.fill(0x5a);
        writes.Global(P::BankRva, rawName);
        writes.Global(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(0));
        Host::Reset();
        invoke();
        Check(!runtime.stopping.load(), "unloaded current owner and nonterminated native block name do not add an unrelated nested-read predicate");
        setChildren(1);
        bool childRefused = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(sdk->GetNameHashKey() == 0x13579bdf, "scalar getter does not traverse an unqualified sequence pointer");
            try
            {
                (void)sdk->GetSequences();
            }
            catch (const std::runtime_error&)
            {
                childRefused = true;
            }
        };
        Host::Reset();
        invoke();
        Check(childRefused && !runtime.stopping.load(), "invalid dynamic child is a typed nested refusal while scalar owner remains usable");
        setChildren(0);
        callback.action = [&](NativeGameTimingAccess&)
        { Check(!sdk->GetSequences() && !sdk->GetSequence(0), "native null pointer keeps original null-zero-index behavior"); };
        Host::Reset();
        invoke();
        setChildren(childPointer);
        callback.action = [&](NativeGameTimingAccess&) { result = access->ReadAnimationHierarchy(token, values); };
        Host::Reset();
        Host::failScalar = 1;
        values = sentinel;
        invoke();
        Check(result != ClockResult::Ok && values.interfaceAddress == sentinel.interfaceAddress && values.hash == sentinel.hash &&
                  values.totalTimeBits == sentinel.totalTimeBits && values.sequenceCount == sentinel.sequenceCount &&
                  values.runningCompressed == sentinel.runningCompressed && values.blockId == sentinel.blockId && !runtime.stopping.load(),
              "actual scalar reader failure preserves every output field without stopping a read-only service");
        Host::Reset();
        Host::afterScalar = &ChangeScalar;
        values = sentinel;
        invoke();
        Check(result != ClockResult::Ok && values.interfaceAddress == sentinel.interfaceAddress && !runtime.stopping.load(),
              "actual native scalar bookends reject a mixed time snapshot");
        std::uintptr_t pointer = 0x12345;
        callback.action = [&](NativeGameTimingAccess&) { result = access->ReadAnimationHierarchySequences(token, pointer); };
        Host::Reset();
        Host::afterChild = &ChangePointer;
        invoke();
        Check(result != ClockResult::Ok && pointer == 0x12345 && !runtime.stopping.load(),
              "cookie-qualified child callback cannot publish a pointer after its real native parent changed");
        setChildren(childPointer);
        auto mutableToken = token;
        caller = &mutableToken;
        callback.action = [&](NativeGameTimingAccess&) { result = access->ReadAnimationHierarchy(mutableToken, values); };
        Host::Reset();
        Host::afterScalar = &ChangeCaller;
        invoke();
        Check(result == ClockResult::Ok && !mutableToken.ownerGeneration && mutableToken.locator == 1,
              "actual service captures locator and generation before callbacks mutate caller intent");
        callback.action = [&](NativeGameTimingAccess&) { result = access->ReadAnimationHierarchySequences(token, pointer); };
        pointer = 0x12345;
        Host::Reset();
        Host::afterChild = &LoseGeneration;
        invoke();
        Check(result == ClockResult::OwnerConflict && pointer == 0x12345 && !runtime.stopping.load(),
              "post-child callback-generation loss refuses the pointer without native debt or owner stop");
        Host::Reset();
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(!runtime.customSequences.IsRetained(), "bank reads never adopt the child span into the private allocation/free journal");
        NativeClockService next;
        bool               staleRefused = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                (void)sdk->IsCustom();
            }
            catch (const std::runtime_error&)
            {
                staleRefused = true;
            }
        };
        BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(staleRefused && !Host::scalarReads && !runtime.stopping.load(), "even IsCustom remains owner-qualified and refuses an old callback generation");
        WorldGroundOwnerFixture::Retire(next, fixture);
        sdk.reset();
        caller = nullptr;
        Host::locator = Host::cookie = 0;
        Host::Reset();
        Check(VirtualFree(children, 0, MEM_RELEASE) != FALSE, "release only fixture-owned child backing after readers retire");
        writes.Restore();
        std::puts("Production native animation hierarchy bank owner fixture completed");
        std::puts("Production actual animation hierarchy bank SDK owner fixture completed");
    }
}

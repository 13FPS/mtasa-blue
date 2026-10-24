/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager signed counters and without-delete owner dispatch
 ****************************************************************************/
namespace AnimationManagerRefsOwnerFixture
{
    void LoseGc() noexcept
    {
        const LONG value = 1;
        SIZE_T     written = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(ClockReaderTestHost::image + NativeBorrowedGcProfile::markerRva), &value,
                                  sizeof(value), &written) ||
            written != sizeof(value))
            std::abort();
    }
    void LoseGeneration() noexcept
    {
        ++timingCallbackGeneration;
    }
    void ChangeCaller() noexcept
    {
        auto* caller = ClockAnimationBlockRefOwnerTest::callerIdentity;
        caller->name.fill(0x55);
        caller->ownerGeneration = 0;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace P = NativeAnimationBlockProfile;
        namespace R = NativeAnimationBlockRefProfile;
        namespace Host = ClockAnimationBlockRefOwnerTest;
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        NativeAnimationBlockRefFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Host::address = fixture.image + P::BankRva + R::RefOffset;
        Host::Reset();
        const NativeAnimationBlockName name{'p', 'e', 'd', 0};
        fixture.Put(P::BankRva, name);
        fixture.Put(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(0));
        const auto refs = [&]
        {
            std::uint16_t value = 0;
            std::memcpy(&value, reinterpret_cast<const void*>(Host::address), sizeof(value));
            return value;
        };
        const auto                        setRefs = [&](std::uint16_t value) { fixture.Put(P::BankRva + R::RefOffset, value); };
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockResult                       result = ClockResult::NotInitialized;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto                   generation = runtime.generation.load();
        auto                         sdk = std::make_unique<AnimationManagerRefsReceiver>(runtime.access, runtime.access, generation);
        NativeAnimationBlockIdentity identity;
        identity.index = 0;
        identity.name = name;
        identity.ownerGeneration = generation;
        animation_block_refs_fixture::RefsReceiver block(runtime.access, identity);
        fixture.invokeTimer();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the current manager-counter callback owner");
            fixture.invokeTimer();
        };
        bool threw = false;
        try
        {
            sdk->RemoveAnimBlockRefWithoutDelete(0);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && !Host::exchanges && !runtime.stopping.load(), "actual manager counter-only CPU association grants no native mutation between callbacks");
        std::thread foreign([&] { result = runtime.access.RemoveAnimationBlockRefWithoutDelete(identity); });
        foreign.join();
        Check(result != ClockResult::Ok && !Host::exchanges && !runtime.stopping.load(),
              "foreign without-delete call refuses before touching the current native reference word");
        int            managerRefs = 123456;
        unsigned short blockRefs = 0;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                managerRefs = sdk->GetNumRefsToAnimBlock(0);
                blockRefs = block.GetRefs();
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        for (const std::uint16_t value : {std::uint16_t{0}, std::uint16_t{0x7fff}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
        {
            Host::Reset();
            setRefs(value);
            invoke();
            std::int16_t signedValue = 0;
            std::memcpy(&signedValue, &value, sizeof(signedValue));
            Check(!threw && managerRefs == static_cast<int>(signedValue) && blockRefs == value && !Host::exchanges && !runtime.stopping.load(),
                  "actual manager getter sign-extends the source int16 while actual block GetRefs preserves unsigned16 bits");
        }
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                sdk->RemoveAnimBlockRefWithoutDelete(0);
                managerRefs = sdk->GetNumRefsToAnimBlock(0);
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        for (const std::uint16_t value : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
        {
            Host::Reset();
            setRefs(value);
            std::array<unsigned char, P::RowBytes> expected{};
            std::memcpy(expected.data(), reinterpret_cast<void*>(fixture.image + P::BankRva), expected.size());
            const auto target = static_cast<std::uint16_t>(static_cast<std::uint32_t>(value) - 1u);
            std::memcpy(expected.data() + R::RefOffset, &target, sizeof(target));
            invoke();
            std::int16_t signedValue = 0;
            std::memcpy(&signedValue, &target, sizeof(signedValue));
            Check(!threw && managerRefs == signedValue && refs() == target && Host::exchanges == 1 &&
                      !std::memcmp(expected.data(), reinterpret_cast<void*>(fixture.image + P::BankRva), expected.size()),
                  "actual manager without-delete unconditionally decrements one unloaded native WORD, wraps zero and preserves every neighbouring field");
        }
        Host::Reset();
        setRefs(0xffff);
        callback.action = [&](NativeGameTimingAccess&)
        {
            sdk->AddAnimBlockRef(0);
            managerRefs = sdk->GetNumRefsToAnimBlock(0);
        };
        invoke();
        Check(managerRefs == 0 && refs() == 0 && Host::exchanges == 1,
              "actual manager AddAnimBlockRef uses the new raw counter channel with original WORD modulo behavior");
        NativeAnimationBlockName rawName;
        rawName.fill(0x5a);
        fixture.Put(P::BankRva, rawName);
        Host::Reset();
        setRefs(0x8000);
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                managerRefs = sdk->GetNumRefsToAnimBlock(0);
                sdk->RemoveAnimBlockRefWithoutDelete(0);
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        invoke();
        Check(!threw && managerRefs == -32768 && refs() == 0x7fff && Host::exchanges == 1,
              "integer manager counters admit full raw16 non-NUL affinity without creating a string factory or block-name facade");
        fixture.Put(P::BankRva, name);
        Host::Reset();
        setRefs(5);
        for (const int index : {-1, static_cast<int>(P::ObservationLimit)})
        {
            callback.action = [&, index](NativeGameTimingAccess&)
            {
                threw = false;
                try
                {
                    sdk->RemoveAnimBlockRefWithoutDelete(index);
                }
                catch (const std::runtime_error&)
                {
                    threw = true;
                }
            };
            invoke();
            Check(threw && !Host::exchanges && refs() == 5 && !runtime.stopping.load(),
                  "negative and outside-current-bank integer IDs cannot select a native counter");
        }
        auto changed = identity;
        ++changed.ownerGeneration;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.RemoveAnimationBlockRefWithoutDelete(changed); };
        invoke();
        Check(result == ClockResult::OwnerConflict && !Host::exchanges && refs() == 5,
              "stale or foreign generation refuses the new optional decrement before its native observation");
        changed = identity;
        changed.name[0] ^= 1;
        invoke();
        Check(result != ClockResult::Ok && !Host::exchanges && refs() == 5 && !runtime.stopping.load(),
              "foreign raw name affinity never redirects the without-delete counter target");
        changed = identity;
        Host::Reset();
        Host::callerIdentity = &changed;
        Host::changeCaller = true;
        Host::afterAccepted = &ChangeCaller;
        invoke();
        Check(result == ClockResult::Ok && refs() == 4 && Host::exchanges == 1 && changed.index == 1 && !changed.ownerGeneration && changed.name[0] == 0x55 &&
                  !runtime.stopping.load(),
              "actual runtime copies complete decrement identity before callbacks and ignores later caller-object mutation");
        Host::Reset();
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(refs() == 4, "normal End leaves the accepted native decrement persistent with no counter rollback journal");

        for (const unsigned scenario : {1u, 2u, 3u})
        {
            Host::Reset();
            fixture.Put(P::BankRva, name);
            fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
            setRefs(20);
            NativeClockService next;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.RemoveAnimationBlockRefWithoutDelete(identity); };
            BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            identity.ownerGeneration = runtime.generation.load();
            if (scenario == 1)
                Host::scenario = 1;
            if (scenario == 2)
                Host::afterAccepted = &LoseGc;
            if (scenario == 3)
                Host::afterAccepted = &LoseGeneration;
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && runtime.stopping.load() && Host::exchanges == 1 && refs() == 19,
                  "accepted decrement followed by native fault, GC loss or generation loss stops the actual owner without rewinding the native word");
            Check(runtime.access.RemoveAnimationBlockRefWithoutDelete(identity) != ClockResult::Ok && Host::exchanges == 1,
                  "stopped callback owner cannot replay the accepted without-delete operation");
            fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
            Host::afterAccepted = nullptr;
            WorldGroundOwnerFixture::Retire(next, fixture);
            Check(refs() == 19 && !runtime.customSequences.IsRetained() && !runtime.pedReferences.IsRetained(),
                  "counter effect remains after End and creates no private native-allocation or ped-reference cleanup debt");
        }
        Host::Reset();
        NativeClockService last;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                (void)sdk->GetNumRefsToAnimBlock(0);
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        BeginOwned(last, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(threw && !Host::wordReads && !Host::exchanges && !runtime.stopping.load(),
              "older counter-only SDK association cannot revive itself in a newer callback generation");
        WorldGroundOwnerFixture::Retire(last, fixture);
        sdk.reset();
        Host::address = 0;
        Host::Reset();
        writes.Restore();
        std::puts("Production native animation manager reference owner fixture completed");
        std::puts("Production actual animation manager reference SDK owner fixture completed");
    }
}

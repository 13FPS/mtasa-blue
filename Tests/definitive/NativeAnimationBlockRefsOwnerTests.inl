/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine block reference words on the existing callback owner
 ****************************************************************************/
namespace AnimationBlockRefsOwnerFixture
{
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace P = NativeAnimationBlockProfile;
        namespace R = NativeAnimationBlockRefProfile;
        namespace Host = ClockAnimationBlockRefOwnerTest;
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        NativeAnimationBlockRefFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        const auto row = fixture.image + P::BankRva;
        Host::address = row + R::RefOffset;
        Host::Reset();
        const NativeAnimationBlockName name{'p', 'e', 'd', 0};
        NativeAnimationBlockIdentity   identity;
        identity.name = name;
        const auto nativeRefs = [&]
        {
            std::uint16_t value = 0;
            std::memcpy(&value, reinterpret_cast<const void*>(row + R::RefOffset), sizeof(value));
            return value;
        };
        const auto setRefs = [&](std::uint16_t value) { fixture.Put(P::BankRva + R::RefOffset, value); };
        const auto setUnloaded = [&] { fixture.Put(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(0)); };
        setUnloaded();
        struct DefaultAccess final : NativeAnimationBlockAccess
        {
        } absent;
        std::uint16_t output = 0x9a7b;
        Check(absent.ReadAnimationBlockRefs(identity, output) == ClockResult::NotInitialized && output == 0x9a7b &&
                  absent.AddAnimationBlockRef(identity) == ClockResult::NotInitialized,
              "older optional owner preserves the ref output and cannot invent an AddRef effect");
        using Sdk = animation_block_refs_fixture::RefsReceiver;
        static_assert(!std::is_abstract_v<Sdk>);
        WorldGroundOwnerFixture::Callback callback;
        ClockResult                       result = ClockResult::NotInitialized;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        identity.ownerGeneration = runtime.generation.load();
        const auto originalIdentity = identity;
        auto       sdk = std::make_unique<Sdk>(runtime.access, identity);
        fixture.invokeTimer();
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the sole current animation reference owner");
            fixture.invokeTimer();
        };
        Check(runtime.access.ReadAnimationBlockRefs(identity, output) != ClockResult::Ok && output == 0x9a7b &&
                  runtime.access.AddAnimationBlockRef(identity) != ClockResult::Ok && !Host::exchanges && !Host::wordReads,
              "a retained block identity supplies no reference authority between callbacks");
        std::thread foreign([&] { result = runtime.access.AddAnimationBlockRef(identity); });
        foreign.join();
        Check(result != ClockResult::Ok && !Host::exchanges && !runtime.stopping.load(),
              "foreign thread refuses AddRef before atomic native work without stopping the valid owner");
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadAnimationBlockRefs(identity, output); };
        for (const std::uint16_t value : {std::uint16_t{0}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
        {
            Host::Reset();
            setRefs(value);
            output = 0x9a7b;
            invoke();
            Check(result == ClockResult::Ok && output == value && !Host::exchanges && !runtime.stopping.load(),
                  "current unloaded block reports the entire unsigned native WORD without a cached global counter");
        }
        Host::Reset();
        Host::failWordAt = 1;
        output = 0x9a7b;
        invoke();
        Check(result == ClockResult::ReadFailure && output == 0x9a7b && !Host::exchanges && !runtime.stopping.load(),
              "native word read failure preserves caller output and leaves the read-only owner usable");
        Host::Reset();
        ++identity.ownerGeneration;
        output = 0x9a7b;
        invoke();
        Check(result == ClockResult::OwnerConflict && output == 0x9a7b && !Host::wordReads && !Host::exchanges,
              "foreign CPU generation refuses before an immediate native reference observation");
        identity = originalIdentity;
        identity.name[0] ^= 1;
        output = 0x9a7b;
        invoke();
        Check(result != ClockResult::Ok && output == 0x9a7b && !Host::exchanges && !runtime.stopping.load(),
              "changed native name identity cannot redirect a ref read to another current block");
        identity = originalIdentity;
        Host::Reset();
        setRefs(0x8001);
        ClockAnimationOwnerTest::identity = &identity;
        ClockAnimationOwnerTest::afterRead = &ClockAnimationOwnerTest::ChangeIndex;
        output = 0x9a7b;
        invoke();
        Check(result == ClockResult::Ok && output == 0x8001 && identity.index == originalIdentity.index + 1,
              "read snapshots the complete caller identity before a synchronous callback can mutate its original argument");
        identity = originalIdentity;
        ClockAnimationOwnerTest::identity = nullptr;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.AddAnimationBlockRef(identity); };
        for (const std::uint16_t value : {std::uint16_t{0}, std::uint16_t{0x7fff}, std::uint16_t{0xffff}})
        {
            Host::Reset();
            setRefs(value);
            fixture.Put(P::BankRva + R::RefOffset - 1, static_cast<unsigned char>(0xa5));
            std::array<unsigned char, P::RowBytes> before{};
            std::memcpy(before.data(), reinterpret_cast<const void*>(row), before.size());
            invoke();
            auto       expected = before;
            const auto target = static_cast<std::uint16_t>(static_cast<std::uint32_t>(value) + 1u);
            std::memcpy(expected.data() + R::RefOffset, &target, sizeof(target));
            Check(result == ClockResult::Ok && Host::exchanges == 1 && nativeRefs() == target &&
                      !std::memcmp(expected.data(), reinterpret_cast<const void*>(row), expected.size()) && !runtime.stopping.load(),
                  "unloaded AddRef makes one native WORD increment including unsigned wrap while preserving all neighbouring row bits");
        }
        Host::Reset();
        Host::callerIdentity = &identity;
        Host::changeCaller = true;
        setRefs(5);
        invoke();
        Check(
            result == ClockResult::Ok && nativeRefs() == 6 && Host::exchanges == 1 && identity.index == originalIdentity.index + 1 && !runtime.stopping.load(),
            "accepted AddRef stays on its copied identity when the original caller argument changes during atomic acknowledgement");
        identity = originalIdentity;
        Host::Reset();
        bool           threw = false;
        unsigned short sdkRefs = 0;
        setRefs(0xffff);
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                CAnimBlock& block = *sdk;
                block.AddRef();
                sdkRefs = block.GetRefs();
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        invoke();
        Check(!threw && sdkRefs == 0 && nativeRefs() == 0 && Host::exchanges == 1,
              "actual CAnimBlock AddRef and GetRefs virtuals share the current native word instead of substituting CPU bookkeeping");
        std::puts("Production actual animation block reference SDK callback fixture completed");
        Host::Reset();
        setRefs(5);
        setUnloaded();
        std::vector<unsigned> itinerary;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                CAnimBlock& block = *sdk;
                // Observe only the shared itinerary's service boundaries. No
                // positive CGame/model/streaming factory is fabricated here.
                MtaGame::CAnimBlockRequestPolicy::Request(
                    static_cast<std::uint32_t>(BLOCKING), true, [&] { return block.IsLoaded(); }, [&] { return block.GetIndex(); }, [] { return 25575u; },
                    [&](std::uint32_t model) -> CModelInfo*
                    {
                        Check(model == 25575u, "request policy retains its selected IFP model");
                        itinerary.push_back(1);
                        return nullptr;
                    },
                    [&](std::uint32_t model, std::uint32_t flags)
                    {
                        Check(model == 25575u && flags == 0x14, "blocking request keeps the actual MTA request flags");
                        itinerary.push_back(2);
                    },
                    [&]
                    {
                        block.AddRef();
                        itinerary.push_back(3);
                    },
                    [&](bool priority)
                    {
                        Check(priority && block.GetRefs() == 6, "real native AddRef is visible before the shared priority-load boundary");
                        itinerary.push_back(4);
                    },
                    [](std::uint32_t) { return false; });
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        threw = false;
        invoke();
        Check(!threw && itinerary == std::vector<unsigned>({1, 2, 3, 4}) && nativeRefs() == 6 && Host::exchanges == 1,
              "actual shared Request policy orders nullable lookup/request then real SDK AddRef before load, without claiming a complete Request factory");
        std::puts("Production shared animation request policy native reference owner ordering completed");
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(nativeRefs() == 6 && !runtime.pedReferences.IsRetained(),
              "native block ref effects remain after End and create no private allocation or registered-ped cleanup debt");

        for (unsigned scenario = 1; scenario != 4; ++scenario)
        {
            Host::Reset();
            Host::scenario = scenario;
            setRefs(20);
            fixture.Put(P::BankRva, name);
            NativeClockService next;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AddAnimationBlockRef(identity); };
            BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            identity = originalIdentity;
            identity.ownerGeneration = runtime.generation.load();
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && Host::exchanges == 1 && nativeRefs() == 21 && runtime.stopping.load(),
                  "accepted atomic fault, native name loss or postwrite read failure stops the same owner while retaining the real increment");
            Check(runtime.access.AddAnimationBlockRef(identity) != ClockResult::Ok && Host::exchanges == 1,
                  "stopped owner does not replay an accepted native reference effect");
            ClockAnimationOwnerTest::failNext = false;
            fixture.Put(P::BankRva, name);
            WorldGroundOwnerFixture::Retire(next, fixture);
            Check(nativeRefs() == 21 && !runtime.pedReferences.IsRetained(),
                  "entered ref failure retires the existing callback without rolling back native references or inventing allocation debt");
        }
        Host::Reset();
        NativeClockService last;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                (void)sdk->GetRefs();
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        BeginOwned(last, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(threw && !Host::wordReads && !Host::exchanges && !runtime.stopping.load(),
              "old SDK generation cannot revive a block reference view in a newer callback generation");
        WorldGroundOwnerFixture::Retire(last, fixture);
        sdk.reset();
        Host::address = 0;
        Host::Reset();
        ClockAnimationOwnerTest::identity = nullptr;
        ClockAnimationOwnerTest::afterRead = nullptr;
        writes.Restore();
        std::puts("Production native animation block reference owner fixture completed");
    }
}

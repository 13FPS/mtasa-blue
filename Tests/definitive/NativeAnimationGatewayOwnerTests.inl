/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA gateway hash policy over a freshly qualified bank cell
 ****************************************************************************/
namespace AnimationGatewayOwnerFixture
{
    namespace Host = ClockAnimationHierarchyHashOwnerTest;
    std::uintptr_t* caller = nullptr;
    std::uintptr_t  replacement = 0;
    void            ChangeCaller() noexcept
    {
        *caller = replacement;
    }
    void ChangeHash() noexcept
    {
        std::uint32_t changed = 0;
        std::memcpy(&changed, reinterpret_cast<const void*>(Host::address), sizeof(changed));
        changed ^= 0x01020304;
        SIZE_T written = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(Host::address), &changed, sizeof(changed), &written) ||
            written != sizeof(changed))
            std::abort();
    }
    void LoseGeneration() noexcept
    {
        ++timingCallbackGeneration;
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
        Host::address = locator;
        Host::Reset();
        const auto setHash = [&](std::uint32_t value) { fixture.Put(H::BankRva + 6 * H::RowBytes, value); };
        CKeyGenDE  keygen;
        const auto expectedGatewayHash = keygen.GetUppercaseKey(MtaGame::CAnimGatewayNames::GetAnimationName());
        const auto lowerCaseHash = keygen.GetKey(MtaGame::CAnimGatewayNames::GetAnimationName());
        Check(expectedGatewayHash != lowerCaseHash, "the authored lowercase gateway name distinguishes the actual uppercase-key policy");
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        auto       sdk = std::make_unique<AnimationGatewayReceiver>(runtime.access, generation);
        fixture.invokeTimer();
        ClockResult   result = ClockResult::NotInitialized;
        std::uint32_t output = 0xdeadbeef;
        const auto    invoke = [&]
        {
            Check(service.Renew(generation), "renew only the current gateway hash observation owner");
            fixture.invokeTimer();
        };
        Check(runtime.access.ReadAnimationHierarchyHash(generation, locator, output) != ClockResult::Ok && output == 0xdeadbeef && !Host::reads,
              "retained raw hierarchy locator grants no hash observation between callbacks");
        std::thread foreign([&] { result = runtime.access.ReadAnimationHierarchyHash(generation, locator, output); });
        foreign.join();
        Check(result != ClockResult::Ok && output == 0xdeadbeef && !Host::reads && !runtime.stopping.load(),
              "foreign producer cannot borrow native hierarchy hash access or stop a valid owner");
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadAnimationHierarchyHash(generation, locator, output); };
        for (const std::uint32_t value : {0u, 0x80000000u, 0xffffffffu})
        {
            Host::Reset();
            setHash(value);
            invoke();
            Check(result == ClockResult::Ok && output == value && !runtime.stopping.load(),
                  "actual owner publishes the entire native hash DWORD, including zero and all high bits");
        }
        bool classified = false, threw = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                classified = sdk->isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(locator));
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        for (const auto value : {expectedGatewayHash, lowerCaseHash, std::uint32_t{0}, expectedGatewayHash})
        {
            Host::Reset();
            setHash(value);
            invoke();
            Check(!threw && classified == (value == expectedGatewayHash),
                  "actual CAnimManagerDE gateway operation reads the current cell every call and compares MTA's authored uppercase key");
        }
        NativeAnimationBlockName rawName;
        rawName.fill(0x5a);
        writes.Global(P::BankRva, rawName);
        writes.Global(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(0));
        setHash(expectedGatewayHash);
        Host::Reset();
        invoke();
        Check(!threw && classified && !runtime.stopping.load(),
              "source-preserved hash remains observable on an unloaded current row without imposing a block-name string filter");
        for (const auto bad : {std::uintptr_t{0}, locator + 4, fixture.image + H::BankRva + 12 * H::RowBytes, locator + (std::uintptr_t{1} << 32)})
        {
            callback.action = [&, bad](NativeGameTimingAccess&)
            {
                classified = true;
                threw = false;
                try
                {
                    classified = sdk->isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(bad));
                }
                catch (const std::runtime_error&)
                {
                    threw = true;
                }
            };
            Host::Reset();
            invoke();
            Check(threw && classified && !runtime.stopping.load(),
                  "invalid full-width hierarchy locator is an owner refusal, never a false gateway classification or a fabricated hierarchy product");
        }
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadAnimationHierarchyHash(generation + 1, locator, output); };
        Host::Reset();
        output = 0xdeadbeef;
        invoke();
        Check(result == ClockResult::OwnerConflict && output == 0xdeadbeef && !Host::reads,
              "foreign CPU generation refuses before observing the native hash cell");
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadAnimationHierarchyHash(generation, locator, output); };
        Host::Reset();
        Host::failAt = 1;
        invoke();
        Check(result != ClockResult::Ok && output == 0xdeadbeef && !runtime.stopping.load(),
              "lost hash observation preserves the caller DWORD and leaves the read-only owner usable");
        Host::Reset();
        Host::afterRead = &ChangeHash;
        output = 0xdeadbeef;
        invoke();
        Check(result != ClockResult::Ok && output == 0xdeadbeef && !runtime.stopping.load(),
              "hash transition between actual native snapshots cannot publish an inconsistent classification input");
        Host::Reset();
        setHash(expectedGatewayHash);
        auto mutableLocator = locator;
        caller = &mutableLocator;
        replacement = fixture.image + H::BankRva + 7 * H::RowBytes;
        Host::afterRead = &ChangeCaller;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadAnimationHierarchyHash(generation, mutableLocator, output); };
        invoke();
        Check(result == ClockResult::Ok && output == expectedGatewayHash && mutableLocator == replacement,
              "actual service keeps its copied full-width locator through synchronous caller mutation");
        Host::Reset();
        output = 0xdeadbeef;
        Host::afterRead = &LoseGeneration;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadAnimationHierarchyHash(generation, locator, output); };
        invoke();
        Check(result == ClockResult::OwnerConflict && output == 0xdeadbeef && !runtime.stopping.load(),
              "post-read callback-generation loss preserves the output without stopping a read-only owner or creating native debt");
        Host::Reset();
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(!runtime.customSequences.IsRetained() && !runtime.pedReferences.IsRetained(),
              "hash-only observation needs no hierarchy allocation, reference lease or native End cleanup");
        NativeClockService next;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                (void)sdk->isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(locator));
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(threw && !Host::reads && !runtime.stopping.load(), "old observer-only manager cannot revive its gateway read in a new callback generation");
        WorldGroundOwnerFixture::Retire(next, fixture);
        sdk.reset();
        caller = nullptr;
        Host::address = 0;
        Host::Reset();
        writes.Restore();
        std::puts("Production native animation hierarchy hash owner fixture completed");
        std::puts("Production actual animation gateway SDK owner fixture completed");
    }
}

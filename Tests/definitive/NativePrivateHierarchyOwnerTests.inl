/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Empty module hierarchies preserve unknown fields and retain debt
 ****************************************************************************/
namespace mta::game_de::PrivateAnimationHierarchyDetail
{
    ClockResult InitializeFields(std::uintptr_t, std::uint32_t&) noexcept;
}
namespace PrivateHierarchyOwnerFixture
{
    namespace Calls = NativePrivateAnimationHierarchyCalls;
    using Storage = NativePrivateAnimationHierarchyStorage;
    using Controller = NativePrivateAnimationHierarchyController;
    using Token = NativeAnimationHierarchyReferenceToken;
    unsigned    writes = 0, mode = 0, scenario = 0;
    ClockResult forcedOwner = ClockResult::Ok;
    Token*      mutableToken = nullptr;
    ClockResult Owner() noexcept
    {
        const auto result = CheckTimingOwner();
        if (result != ClockResult::Ok)
            return result;
        if (mutableToken)
        {
            mutableToken->ownerGeneration = 0;
            mutableToken->locator = 1;
            mutableToken = nullptr;
        }
        return forcedOwner;
    }
    ClockResult Write(std::uintptr_t address, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = PrivateAnimationHierarchyDetail::InitializeFields(address, exception);
        if (result != ClockResult::Ok)
            return result;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 2)
            forcedOwner = ClockResult::OwnerConflict;
        return result;
    }
    void Prepared(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the current private CPU hierarchy owner");
            fixture.invokeTimer();
        };
        if (!scenario)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Storage                       raw(generation);
                std::array<unsigned char, 40> expected;
                expected.fill(0xa5);
                std::memcpy(reinterpret_cast<void*>(raw.Address()), expected.data(), expected.size());
                const std::uintptr_t zeroPointer = 0;
                const std::uint16_t  zeroCount = 0;
                const std::uint8_t   zeroByte = 0;
                const std::int32_t   custom = -1;
                const std::uint32_t  zeroTime = 0;
                std::memcpy(expected.data() + 8, &zeroPointer, 8);
                std::memcpy(expected.data() + 0x10, &zeroCount, 2);
                std::memcpy(expected.data() + 0x12, &zeroByte, 1);
                std::memcpy(expected.data() + 0x13, &zeroByte, 1);
                std::memcpy(expected.data() + 0x14, &custom, 4);
                std::memcpy(expected.data() + 0x18, &zeroTime, 4);
                std::memcpy(expected.data() + 0x20, &zeroPointer, 8);
                Calls::Receipt receipt;
                Check(Calls::Initialize(fixture.image, raw, generation, &Owner, &receipt, &Write) == ClockResult::Ok && receipt.accepted && receipt.verified &&
                          !std::memcmp(reinterpret_cast<void*>(raw.Address()), expected.data(), 40),
                      "actual CPU Initialize preserves raw hash/alignment while setting only source fields");
                NativeAnimationHierarchyValues values;
                Check(Calls::Read(fixture.image, raw, generation, &Owner, values) == ClockResult::Ok && !values.hashDefined && values.blockId == -1 &&
                          !values.sequenceCount && !values.runningCompressed && !values.totalTimeBits,
                      "private unknown hash is explicit transport metadata, never a fabricated native hash");
                Check(Calls::Retire(fixture.image, raw, generation, &Owner, &receipt) == ClockResult::Ok && receipt.retired && raw.CanDiscard(),
                      "empty neverpublished CPU metadata retires without a native destructor");
                for (const unsigned failure : {1u, 2u})
                {
                    Storage failed(generation);
                    std::memset(reinterpret_cast<void*>(failed.Address()), 0xa5, 40);
                    writes = 0;
                    mode = failure;
                    forcedOwner = ClockResult::Ok;
                    Check(Calls::Initialize(fixture.image, failed, generation, &Owner, &receipt, &Write) != ClockResult::Ok && receipt.entered &&
                              receipt.accepted == (failure == 2) && writes == 1 && !failed.CanDiscard(),
                          "lost initialization acknowledgement retains precise accepted/unknown CPU state");
                    mode = 0;
                    forcedOwner = ClockResult::Ok;
                    Check(Calls::Initialize(fixture.image, failed, generation, &Owner, &receipt, &Write) != ClockResult::Ok && writes == 1,
                          "entered initialization cannot replay");
                    Check(Calls::Retire(fixture.image, failed, generation, &Owner, &receipt) == ClockResult::Ok && failed.CanDiscard(),
                          "actual empty field recheck can retire only known neverpublished failed CPU initialization");
                }
                Controller local;
                Token      token;
                Check(local.Create(fixture.image, generation, &Owner, token) == ClockResult::Ok && local.Owns(token),
                      "actual controller issues initialized module metadata");
                auto caller = token;
                mutableToken = &caller;
                NativeAnimationHierarchyValues values2;
                Check(local.Read(fixture.image, caller, &Owner, values2) == ClockResult::Ok && values2.interfaceAddress == token.locator &&
                          !caller.ownerGeneration && caller.locator == 1,
                      "controller captures its token before owner callbacks mutate caller intent");
                auto foreign = token;
                ++foreign.ownerGeneration;
                values2.interfaceAddress = 1;
                Check(local.Read(fixture.image, foreign, &Owner, values2) != ClockResult::Ok && values2.interfaceAddress == 1,
                      "foreign generation never adopts a module pointer");
                Check(local.Destroy(fixture.image, token, &Owner) == ClockResult::Ok && !local.IsRetained() && !local.Owns(token),
                      "controller deletes only the explicitly retired CPU record");
            };
            invoke();
        }
        Token       token;
        ClockResult result = ClockResult::NotInitialized;
        callback.action = [&](NativeGameTimingAccess& access)
        { result = dynamic_cast<NativeAnimationHierarchyAccess&>(access).CreatePrivateAnimationHierarchy(generation, token); };
        invoke();
        Check(result == ClockResult::Ok && token.ownerGeneration == generation && token.locator && runtime.privateHierarchies.Owns(token),
              "real callback exposes actual private CPU owner on the existing service");
        if (scenario)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                if (scenario == 1)
                    Check(runtime.privateHierarchies.PinPublication(token) == ClockResult::Ok, "mark actual irreversible publication bookkeeping");
                else
                {
                    const std::uintptr_t unknown = 1;
                    std::memcpy(reinterpret_cast<void*>(token.locator + (scenario == 2 ? 8 : 0x20)), &unknown, sizeof(unknown));
                }
                result = runtime.access.DestroyPrivateAnimationHierarchy(token);
            };
            invoke();
            Check(result != ClockResult::Ok && runtime.privateHierarchies.IsRetained() && runtime.privateHierarchies.Owns(token),
                  "publication or unknown external pointer/cache never becomes child-free authority");
            Check(!service.End() && service.IsRetained(), "producer End retains unresolved private hierarchy ownership");
            fixture.invokeTimer();
            if (scenario == 2 || scenario == 3)
            {
                const std::uintptr_t externallyCleared = 0;
                std::memcpy(reinterpret_cast<void*>(token.locator + (scenario == 2 ? 8 : 0x20)), &externallyCleared, sizeof(externallyCleared));
                fixture.invokeTimer();
                Check(!service.End() && runtime.privateHierarchies.Owns(token) && runtime.privateHierarchies.IsRetained(),
                      "observed unknown pointer/cache remains pinned after external zeroing; data equality is not retirement proof");
            }
            Check(!service.End() && service.IsRetained() && runtime.privateHierarchies.IsRetained() && runtime.timerHooked.load(),
                  "natural callback keeps module/hooks and record when retirement is unproved");
            NativeClockService  blocked;
            ClockServiceStateV1 blockedState{};
            Check(!blocked.BeginGameTiming({}, blockedState) && runtime.generation.load() == generation && runtime.privateHierarchies.IsRetained() &&
                      runtime.timerHooked.load(),
                  "another owner cannot replace retained private hierarchy lifetime");
            std::puts("Production private hierarchy unresolved lifetime retained");
            // The isolated process owns this irreversible scenario. Its exit
            // is not a successful production detach or child cleanup.
            return;
        }
        auto sdk = std::make_unique<animation_hierarchy_fixture::AnimationHierarchyReceiver>(runtime.access, token);
        Check(runtime.access.InitializePrivateAnimationHierarchy(token) != ClockResult::Ok, "CPU facade does not authorize private writes between callbacks");
        std::array<unsigned char, 40> expected;
        std::memcpy(expected.data(), reinterpret_cast<void*>(token.locator), 40);
        std::fill(expected.begin(), expected.begin() + 8, static_cast<unsigned char>(0xa5));
        std::fill(expected.begin() + 0x1c, expected.begin() + 0x20, static_cast<unsigned char>(0x5a));
        std::memcpy(reinterpret_cast<void*>(token.locator), expected.data(), 40);
        bool hashRefused = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            sdk->Initialize();
            Check(!std::memcmp(reinterpret_cast<void*>(token.locator), expected.data(), 40) && sdk->IsCustom() && sdk->GetAnimBlockID() == -1 &&
                      !sdk->GetNumSequences() && !sdk->IsRunningCompressed() && !sdk->GetSequences() && !sdk->GetSequence(0) &&
                      reinterpret_cast<std::uintptr_t>(sdk->GetInterface()) == token.locator,
                  "actual SDK Initialize/read routes owned private metadata while preserving unknown raw bytes");
            try
            {
                (void)sdk->GetNameHashKey();
            }
            catch (const std::runtime_error&)
            {
                hashRefused = true;
            }
        };
        invoke();
        Check(hashRefused && !runtime.stopping.load(), "unknown hierarchy hash refusal leaves qualified fields and owner usable");
        callback.action = [&](NativeGameTimingAccess& access)
        { result = dynamic_cast<NativeAnimationHierarchyAccess&>(access).DestroyPrivateAnimationHierarchy(token); };
        invoke();
        Check(result == ClockResult::Ok && !runtime.privateHierarchies.IsRetained(),
              "explicit destroy retires the known empty CPU object without native child work");
        WorldGroundOwnerFixture::Retire(service, fixture);
        NativeClockService next;
        bool               stale = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                sdk->Initialize();
            }
            catch (const std::runtime_error&)
            {
                stale = true;
            }
        };
        BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(stale && !runtime.privateHierarchies.IsRetained() && !runtime.stopping.load(),
              "old private facade cannot revive its token in another callback generation");
        WorldGroundOwnerFixture::Retire(next, fixture);
        sdk.reset();
        saved.Restore();
        std::puts("Production native private hierarchy owner fixture completed");
        std::puts("Production actual private hierarchy SDK owner fixture completed");
    }
}

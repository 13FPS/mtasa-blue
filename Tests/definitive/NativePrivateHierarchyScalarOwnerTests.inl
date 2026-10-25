/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Two private scalar writes preserve raw hierarchy state and lifetime
 ****************************************************************************/
namespace mta::game_de::PrivateAnimationHierarchyDetail
{
    ClockResult WriteScalar(std::uintptr_t, NativePrivateAnimationHierarchyCalls::ScalarOperation, std::uint32_t, std::uint32_t&) noexcept;
}
namespace PrivateHierarchyScalarOwnerFixture
{
    namespace Calls = NativePrivateAnimationHierarchyCalls;
    using Storage = NativePrivateAnimationHierarchyStorage;
    unsigned    writes = 0, mode = 0;
    bool        pinned = false;
    ClockResult forcedOwner = ClockResult::Ok;
    ClockResult Owner() noexcept
    {
        const auto result = CheckTimingOwner();
        return result == ClockResult::Ok ? forcedOwner : result;
    }
    ClockResult Write(std::uintptr_t address, Calls::ScalarOperation operation, std::uint32_t bits, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = PrivateAnimationHierarchyDetail::WriteScalar(address, operation, bits, exception);
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
            Check(service.Renew(generation), "renew only the private scalar owner");
            fixture.invokeTimer();
        };
        if (!pinned)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                for (const unsigned failure : {1u, 2u})
                {
                    Storage raw(generation);
                    std::memset(reinterpret_cast<void*>(raw.Address()), 0xa5, 40);
                    Calls::Receipt receipt;
                    Check(Calls::Initialize(fixture.image, raw, generation, &Owner, &receipt) == ClockResult::Ok, "prepare actual empty scalar receiver");
                    writes = 0;
                    mode = failure;
                    forcedOwner = ClockResult::Ok;
                    Check(Calls::SetAnimationBlockID(fixture.image, raw, generation, (std::numeric_limits<std::int32_t>::min)(), &Owner, &receipt, &Write) !=
                                  ClockResult::Ok &&
                              receipt.entered && receipt.accepted == (failure == 2) && writes == 1 &&
                              *reinterpret_cast<std::int32_t*>(raw.Address() + 0x14) == (std::numeric_limits<std::int32_t>::min)(),
                          "real DWORD writer distinguishes unknown/accepted lost acknowledgement while retaining actual scalar bits");
                    mode = 0;
                    forcedOwner = ClockResult::Ok;
                    Check(Calls::SetRunningCompressed(fixture.image, raw, generation, true, &Owner, &receipt, &Write) != ClockResult::Ok && writes == 1,
                          "entered scalar uncertainty cannot replay through a different operation");
                    Check(Calls::Retire(fixture.image, raw, generation, &Owner, &receipt) == ClockResult::Ok && raw.CanDiscard(),
                          "only actual empty neverpublished scalar debt can retire after requalification");
                }
            };
            invoke();
        }
        NativeAnimationHierarchyReferenceToken token;
        ClockResult                            result = ClockResult::NotInitialized;
        callback.action = [&](NativeGameTimingAccess&) { result = runtime.access.CreatePrivateAnimationHierarchy(generation, token); };
        invoke();
        Check(result == ClockResult::Ok, "current callback creates the real module-owned scalar receiver");
        std::array<unsigned char, 40> expected;
        std::memcpy(expected.data(), reinterpret_cast<void*>(token.locator), 40);
        std::fill(expected.begin(), expected.begin() + 8, static_cast<unsigned char>(0xa5));
        std::fill(expected.begin() + 0x1c, expected.begin() + 0x20, static_cast<unsigned char>(0x5a));
        const std::uint32_t nanBits = 0x7fc01234;
        std::memcpy(expected.data() + 0x18, &nanBits, 4);
        std::memcpy(reinterpret_cast<void*>(token.locator), expected.data(), 40);
        if (pinned)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(runtime.privateHierarchies.PinPublication(token) == ClockResult::Ok, "pin actual private record before setter admission");
                NativePrivateAnimationHierarchyController::Receipt refusal;
                Check(runtime.privateHierarchies.SetAnimationBlockID(fixture.image, token, 7, &Owner, &refusal) != ClockResult::Ok && !refusal.entered,
                      "pinned current receiver rejects the DWORD setter before owner loss or stores");
                result = runtime.access.SetPrivateAnimationHierarchyRunningCompressed(token, true);
            };
            invoke();
            Check(result != ClockResult::Ok && !std::memcmp(reinterpret_cast<void*>(token.locator), expected.data(), 40),
                  "publication-pinned receiver rejects BYTE setter before any store");
            Check(runtime.access.SetPrivateAnimationHierarchyBlockID(token, 7) != ClockResult::Ok && !service.End(),
                  "stopped/pinned record grants no alternate DWORD write or retirement");
            fixture.invokeTimer();
            Check(!service.End() && service.IsRetained() && runtime.privateHierarchies.IsRetained() && runtime.timerHooked.load(),
                  "scalar refusal preserves actual retained module lifetime without unpinning");
            std::puts("Production private hierarchy scalar publication refusal retained");
            return;
        }
        auto sdk = std::make_unique<animation_hierarchy_fixture::AnimationHierarchyReceiver>(runtime.access, token);
        callback.action = [&](NativeGameTimingAccess&)
        {
            for (const bool value : {true, false, true})
            {
                sdk->SetRunningCompressed(value);
                expected[0x12] = static_cast<unsigned char>(value);
                Check(sdk->IsRunningCompressed() == value && !std::memcmp(reinterpret_cast<void*>(token.locator), expected.data(), 40),
                      "actual BYTE setter changes only compression, preserving hash/padding and neighbouring raw fields");
            }
            for (const std::int32_t value :
                 {(std::numeric_limits<std::int32_t>::min)(), std::int32_t{-1}, std::int32_t{0}, (std::numeric_limits<std::int32_t>::max)()})
            {
                sdk->SetAnimationBlockID(value);
                std::memcpy(expected.data() + 0x14, &value, 4);
                Check(sdk->GetAnimBlockID() == value && sdk->IsCustom() == (value == -1) &&
                          !std::memcmp(reinterpret_cast<void*>(token.locator), expected.data(), 40),
                      "actual DWORD setter preserves signed bits and the source IsCustom predicate without a guessed enum filter");
            }
        };
        invoke();
        bool hashRefused = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            const auto    time = sdk->GetTotalTime();
            std::uint32_t bits = 0;
            std::memcpy(&bits, &time, 4);
            Check(bits == nanBits && !sdk->GetSequences() && !sdk->GetNumSequences(), "scalar writes preserve raw NaN time and empty child state");
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
        Check(hashRefused && !runtime.stopping.load(), "scalar assignment never defines an unknown name hash");
        Check(runtime.access.SetPrivateAnimationHierarchyRunningCompressed(token, false) != ClockResult::Ok, "private scalar writes stay callback-bound");
        callback.action = [&](NativeGameTimingAccess&) { result = runtime.access.DestroyPrivateAnimationHierarchy(token); };
        invoke();
        Check(result == ClockResult::Ok && !runtime.privateHierarchies.IsRetained(), "known empty scalar receiver remains plainly CPU-retirable");
        WorldGroundOwnerFixture::Retire(service, fixture);
        sdk.reset();
        saved.Restore();
        std::puts("Production native private hierarchy scalar owner fixture completed");
        std::puts("Production actual private hierarchy scalar SDK owner fixture completed");
    }
}

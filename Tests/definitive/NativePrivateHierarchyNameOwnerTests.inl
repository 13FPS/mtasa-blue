/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-owned hierarchy names define only the private hash DWORD
 ****************************************************************************/
namespace PrivateHierarchyNameOwnerFixture
{
    namespace Calls = NativePrivateAnimationHierarchyCalls;
    using Storage = NativePrivateAnimationHierarchyStorage;
    using Controller = NativePrivateAnimationHierarchyController;
    using Token = NativeAnimationHierarchyReferenceToken;
    unsigned    writes = 0, mode = 0;
    bool        pinned = false;
    ClockResult forcedOwner = ClockResult::Ok;
    Token*      caller = nullptr;
    ClockResult Owner() noexcept
    {
        const auto result = CheckTimingOwner();
        if (result != ClockResult::Ok)
            return result;
        if (caller)
        {
            caller->ownerGeneration = 0;
            caller->locator = 1;
            caller = nullptr;
        }
        return forcedOwner;
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
            Check(service.Renew(generation), "renew only the current private hierarchy name owner");
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
                    Check(Calls::Initialize(fixture.image, raw, generation, &Owner, &receipt) == ClockResult::Ok, "prepare actual empty hash receiver");
                    writes = 0;
                    mode = failure;
                    forcedOwner = ClockResult::Ok;
                    Check(Calls::SetNameHash(fixture.image, raw, generation, 0x80000000u, &Owner, &receipt, &Write) != ClockResult::Ok && receipt.entered &&
                              receipt.accepted == (failure == 2) && writes == 1 && *reinterpret_cast<std::uint32_t*>(raw.Address()) == 0x80000000u,
                          "real hash DWORD completion distinguishes accepted post-ack loss from unknown entered outcome without rewinding bits");
                    mode = 0;
                    forcedOwner = ClockResult::Ok;
                    NativeAnimationHierarchyValues untouched;
                    untouched.interfaceAddress = 1;
                    Check(Calls::Read(fixture.image, raw, generation, &Owner, untouched) != ClockResult::Ok && untouched.interfaceAddress == 1 &&
                              Calls::SetNameHash(fixture.image, raw, generation, 7, &Owner, &receipt, &Write) != ClockResult::Ok && writes == 1,
                          "uncertain hash state is not readable or replayable even after accepted physical stores");
                    Check(Calls::Retire(fixture.image, raw, generation, &Owner, &receipt) == ClockResult::Ok && raw.CanDiscard(),
                          "known empty neverpublished hash debt retires only after actual requalification");
                }
                Controller local;
                Token      token;
                Check(local.Create(fixture.image, generation, &Owner, token) == ClockResult::Ok, "create actual controller name receiver");
                auto mutableToken = token;
                caller = &mutableToken;
                Check(local.SetNameHash(fixture.image, mutableToken, 0x13579bdfu, &Owner) == ClockResult::Ok && !mutableToken.ownerGeneration &&
                          mutableToken.locator == 1,
                      "name controller captures token before owner callbacks mutate caller intent");
                NativeAnimationHierarchyValues values;
                Check(local.Read(fixture.image, token, &Owner, values) == ClockResult::Ok && values.hashDefined && values.hash == 0x13579bdfu &&
                          local.Initialize(fixture.image, token, &Owner) == ClockResult::Ok &&
                          local.Read(fixture.image, token, &Owner, values) == ClockResult::Ok && values.hashDefined && values.hash == 0x13579bdfu,
                      "successful hash definition is shared with reads and survives actual Initialize");
                Check(local.Destroy(fixture.image, token, &Owner) == ClockResult::Ok && !local.IsRetained(),
                      "controller retains ordinary empty CPU destruction authority");
            };
            invoke();
        }
        Token       token;
        ClockResult result = ClockResult::NotInitialized;
        callback.action = [&](NativeGameTimingAccess&) { result = runtime.access.CreatePrivateAnimationHierarchy(generation, token); };
        invoke();
        Check(result == ClockResult::Ok, "actual callback issues private hierarchy name authority");
        std::array<unsigned char, 40> expected;
        std::memcpy(expected.data(), reinterpret_cast<void*>(token.locator), 40);
        std::fill(expected.begin(), expected.begin() + 8, static_cast<unsigned char>(0xa5));
        std::fill(expected.begin() + 0x1c, expected.begin() + 0x20, static_cast<unsigned char>(0x5a));
        std::memcpy(reinterpret_cast<void*>(token.locator), expected.data(), 40);
        if (pinned)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(runtime.privateHierarchies.PinPublication(token) == ClockResult::Ok, "pin actual receiver before private name admission");
                result = runtime.access.SetPrivateAnimationHierarchyNameHash(token, 0x12345678u);
            };
            invoke();
            Check(result != ClockResult::Ok && !std::memcmp(reinterpret_cast<void*>(token.locator), expected.data(), 40) && !service.End(),
                  "publication-pinned name cannot change hash or become an implicit unpin");
            fixture.invokeTimer();
            Check(!service.End() && service.IsRetained() && runtime.privateHierarchies.IsRetained(), "name refusal preserves retained module lifetime");
            std::puts("Production private hierarchy name publication refusal retained");
            return;
        }
        auto        sdk = std::make_unique<animation_hierarchy_fixture::AnimationHierarchyReceiver>(runtime.access, token);
        std::string name(200, 'a');
        name += "miX";
        name.push_back(static_cast<char>(0xe9));
        name.push_back(static_cast<char>(0xff));
        std::string normalized(200, 'A');
        normalized += "MIX";
        normalized.push_back(static_cast<char>(0xe9));
        normalized.push_back(static_cast<char>(0xff));
        CKeyGenDE  keygen;
        const auto hash = keygen.GetKey(normalized.c_str());
        Check(name.size() > 64 && hash != keygen.GetKey(normalized.substr(0, 64).c_str()), "long-name fixture discriminates a fabricated sequence64 cap");
        callback.action = [&](NativeGameTimingAccess&)
        {
            sdk->SetName(name.c_str());
            std::memcpy(expected.data(), &hash, 4);
            Check(sdk->GetNameHashKey() == hash && !std::memcmp(reinterpret_cast<void*>(token.locator), expected.data(), 40),
                  "actual SDK hashes the complete long/high-byte name using classic ASCII folding and writes only DWORD0");
            sdk->Initialize();
            Check(sdk->GetNameHashKey() == hash, "actual SDK Initialize preserves both definedness and name bits");
            sdk->SetName("");
            Check(sdk->GetNameHashKey() == 0xffffffffu, "empty source name remains a defined raw CRC seed");
            result = runtime.access.SetPrivateAnimationHierarchyNameHash(token, 0);
            Check(result == ClockResult::Ok && sdk->GetNameHashKey() == 0, "an explicitly written zero hash remains defined rather than inferred absent");
        };
        invoke();
        bool nullRefused = false;
        try
        {
            sdk->SetName(nullptr);
        }
        catch (const std::invalid_argument&)
        {
            nullRefused = true;
        }
        Check(nullRefused && !runtime.stopping.load(), "undefined null input refuses before native owner dispatch");
        Check(runtime.access.SetPrivateAnimationHierarchyNameHash(token, 7) != ClockResult::Ok, "hash write stays callback-bound");
        callback.action = [&](NativeGameTimingAccess&) { result = runtime.access.DestroyPrivateAnimationHierarchy(token); };
        invoke();
        Check(result == ClockResult::Ok && !runtime.privateHierarchies.IsRetained(), "defined hash introduces no child or retirement debt");
        WorldGroundOwnerFixture::Retire(service, fixture);
        sdk.reset();
        saved.Restore();
        std::puts("Production native private hierarchy name owner fixture completed");
        std::puts("Production actual private hierarchy name SDK owner fixture completed");
    }
}

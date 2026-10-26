/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual private node suppression and same-owner hierarchy borrowing
 ****************************************************************************/
namespace ClockAnimationAssociationBonesTest
{
    unsigned    exchanges = 0;
    bool        loseOwner = false, unknownAck = false;
    ClockResult ExchangeNode(std::uintptr_t address, std::uintptr_t expected, std::uintptr_t target, std::uintptr_t& observed, bool& accepted,
                             std::uint32_t& exception) noexcept
    {
        ++exchanges;
        const auto result = AnimationAssociationStorageDetail::ExchangeNode(address, expected, target, observed, accepted, exception);
        if (result == ClockResult::Ok && accepted && loseOwner)
        {
            loseOwner = false;
            ClockPrivateAnimationAssociationTest::Store(ClockPrivateAnimationAssociationTest::image + NativeBorrowedGcProfile::markerRva, LONG{1});
        }
        if (result == ClockResult::Ok && accepted && unknownAck)
        {
            // The real CAS happened, but its acknowledgement is deliberately
            // unavailable. The owner must not invent an accepted-null bit.
            unknownAck = false;
            accepted = false;
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        return result;
    }
}
namespace AnimationAssociationBonesOwnerFixture
{
    namespace Hook = ClockPrivateAnimationAssociationTest;
    namespace NodeHook = ClockAnimationAssociationBonesTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using SourceBank = PrivateAnimationAssociationOwnerFixture::SourceBank;
    using Token = NativeAnimationAssociationReferenceToken;
    unsigned scenario = 0;
    struct OwnedMap
    {
        std::uintptr_t value = 0;
        ~OwnedMap()
        {
            if (value)
                VirtualFree(reinterpret_cast<void*>(value), 0, MEM_RELEASE);
        }
    };
    void RunPrepared(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        admit(NativeStreamingLoadAllProfile::Code);
        for (const auto& pin : NativeStartupOwnerProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeStartupWorldProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeUObjectMembershipProfile::pins);
        for (const auto& pin : NativeGameStateProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &saved);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::active = &host;
        // The direct fixture does not run the old AnimationBlocks wrapper.
        // Its read-only bank foundation still validates these exact pool pins.
        admit(NativePedPoolProfile::Code);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        admit(NativeAnimationBlockProfile::Code);
        // These genuine dispatch cells share executable text pages. Match the
        // established bank fixture preparation without replaying its cases.
        saved.Global(NativeAnimationBlockProfile::RemapRva + NativeAnimationBlockProfile::LoadedOpcodeIndex,
                     static_cast<unsigned char>(NativeAnimationBlockProfile::LoadedSelector), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::DispatchRva + NativeAnimationBlockProfile::LoadedSelector * 4,
                     static_cast<std::uint32_t>(NativeAnimationBlockProfile::LoadedBranchRva), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::GroupSlotRva, fixture.image + NativeAnimationBlockProfile::GroupRva, PAGE_READONLY);
        admit(NativeAnimationBlockRefProfile::Code);
        admit(NativeAnimationGroupArrayProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::ReadCode);
        admit(NativeAnimationAssociationProfile::Code);
        saved.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        SourceBank source(fixture, saved);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Hook::image = fixture.image;
        Hook::constructorCalls = Hook::destructorCalls = Hook::exchangeCalls = Hook::heapAllocations = Hook::heapFrees = 0;
        Hook::targetsValid = Hook::snapshotsCopied = true;
        Hook::heapFailed = false;
        Hook::mode = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "private association scope pins the actual callback module once for this process");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew a bounded real callback before each private association action");
            fixture.invokeTimer();
        };
        auto& access = static_cast<NativeAnimationAssociationAccess&>(runtime.access);

        // GetAnimHierarchy uses its existing independent scalar read profile.
        admit(NativeAnimationHierarchyReadProfile::Code);
        NodeHook::exchanges = 0;
        NodeHook::loseOwner = false;
        NodeHook::unknownAck = false;
        const auto action = [&](const char* label, const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Bone action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            invoke();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        Token      token;
        const auto create = [&]
        {
            action("create-issued-association",
                   [&]
                   {
                       Check(access.CreatePrivateAnimationAssociation(generation, token) == ClockResult::Ok && token.serial,
                             "bone scope issues an explicit unconstructed104 serial");
                   });
            action("construct-copied-native-bodies",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       Check(observer.Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(source.children)) != nullptr,
                             "bone scope executes the genuine copied constructor and node helper");
                   });
        };
        if (scenario)
        {
            source.ResetRow(5);
            create();
            const auto receiver = Hook::lastReceiver;
            const auto nodes = Hook::Load<std::uintptr_t>(receiver + 0x20);
            const auto constructors = Hook::constructorCalls, destructors = Hook::destructorCalls, frees = host.freeCalls;
            action("accepted-null-post-owner-loss",
                   [&]
                   {
                       NodeHook::loseOwner = true;
                       NodeHook::unknownAck = scenario == 2;
                       const auto result = access.RestrictPrivateAnimationAssociationToBones(token, 0);
                       Check(result == (scenario == 2 ? ClockResult::WriteFailure : ClockResult::Busy),
                             "real node clear distinguishes unknown CAS acknowledgement from accepted GC owner loss");
                   });
            Check(!Hook::Load<std::uintptr_t>(nodes + 0x10) && NodeHook::exchanges == 1 && access.HasPendingPrivateAnimationAssociations() &&
                      runtime.privateAssociations.RecoveryPending() && runtime.modulePinned.load() && service.IsRetained() && host.freeCalls == frees &&
                      Hook::Load<std::uint16_t>(source.counter) == 18,
                  "entered partial null keeps original resource and module debt before qualified acknowledgement");
            Check(!service.End() && host.freeCalls == frees && Hook::destructorCalls == destructors,
                  "GC-active End cannot replay suppression or manufacture a destructor ACK");
            std::puts("Production animation association bones post fault retained");
            saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            fixture.invokeTimer();
            if (scenario == 2)
            {
                Check(!service.End() && service.IsRetained() && runtime.privateAssociations.IsRetained() && runtime.privateAssociations.RecoveryPending() &&
                          host.freeCalls == frees && Hook::destructorCalls == destructors && Hook::constructorCalls == constructors &&
                          NodeHook::exchanges == 1 && Hook::Load<std::uint16_t>(source.counter) == 18 && !Hook::Load<std::uintptr_t>(nodes + 0x10) &&
                          runtime.modulePinned.load() && NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                      "unacknowledged null differs from the retained baseline and pins without stale write or destructor replay");
                std::puts("Production animation association bones unknown write retained");
                // Keep the source fixture and borrowed resources alive with
                // their unresolved owner; no local destructor is a free ACK.
                std::printf("Passed %u production clock service checks\n", checks.load());
                std::_Exit(0);
            }
            Check(service.End() && !service.IsRetained() && !runtime.privateAssociations.IsRetained() && host.freeCalls == frees + 2 &&
                      Hook::destructorCalls == destructors + 1 && Hook::constructorCalls == constructors && NodeHook::exchanges == 1 &&
                      Hook::Load<std::uint16_t>(source.counter) == 17,
                  "accepted-null metadata requalifies known resources for exactly one genuine native retirement");
            Check(service.End() && host.freeCalls == frees + 2 && Hook::destructorCalls == destructors + 1,
                  "terminal End never replays the acknowledged deleting destructor");
            std::puts("Production animation association bones accepted cleanup completed");
        }
        else
        {
            const auto shortMap = source.nodes;
            OwnedMap   longMap;
            longMap.value = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 70 * sizeof(std::uintptr_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(longMap.value != 0, "own exactly the source map prefix used by a70-node constructor");
            for (std::size_t index = 0; index != 70; ++index)
                Hook::Store(longMap.value + index * 8, source.sequences + (index % 2) * 80);
            source.nodes = longMap.value;
            source.ResetRow(70);
            create();
            const auto                         receiver = Hook::lastReceiver, nodes = Hook::Load<std::uintptr_t>(receiver + 0x20);
            std::array<unsigned char, 70 * 40> before{}, expected{};
            std::memcpy(before.data(), reinterpret_cast<void*>(nodes), before.size());
            expected = before;
            const auto baselineMap =
                std::vector<std::uintptr_t>(reinterpret_cast<std::uintptr_t*>(longMap.value), reinterpret_cast<std::uintptr_t*>(longMap.value) + 70);
            action("same-owner-hierarchy-facade",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       const auto                          frees = host.freeCalls;
                       auto                                hierarchy = observer.GetAnimHierarchy();
                       Check(hierarchy && reinterpret_cast<std::uintptr_t>(hierarchy->GetInterface()) == source.hierarchy && host.freeCalls == frees,
                             "actual GetAnimHierarchy returns a same-provider current bank facade without storage ownership");
                   });
            action("mask-over64",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBones(std::bitset<64>(~std::uint64_t{0}));
                   });
            for (std::size_t index = 64; index != 70; ++index)
            {
                const std::uintptr_t zero = 0;
                std::memcpy(expected.data() + index * 40 + 0x10, &zero, 8);
            }
            Check(!std::memcmp(expected.data(), reinterpret_cast<void*>(nodes), expected.size()) && NodeHook::exchanges == 6 &&
                      !std::memcmp(baselineMap.data(), reinterpret_cast<void*>(longMap.value), baselineMap.size() * 8) &&
                      Hook::Load<std::uint16_t>(source.counter) == 18,
                  "mask preserves every node byte except actual sequence pointers at indices64andabove, without modifying the source map or reference");
            const auto once = NodeHook::exchanges;
            action("acknowledged-mask-idempotence",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBones(std::bitset<64>(~std::uint64_t{0}));
                   });
            Check(NodeHook::exchanges == once, "acknowledged null suppression is idempotent without another CAS");
            action("clear-all-owned-nodes",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBones(std::bitset<64>{});
                   });
            const auto cleared = NodeHook::exchanges;
            action("never-resurrect-suppressed-nodes",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBones(std::bitset<64>(~std::uint64_t{0}));
                   });
            for (std::size_t index = 0; index != 70; ++index)
                Check(!Hook::Load<std::uintptr_t>(nodes + index * 40 + 0x10), "acknowledged suppression cannot resurrect a constructor baseline pointer");
            Check(NodeHook::exchanges == cleared, "a later all-set mask does not rewrite acknowledged nulls");
            action("retire-mask-association",
                   [&]
                   {
                       Check(access.RetirePrivateAnimationAssociation(token) == ClockResult::Ok,
                             "mask-only metadata changes retain genuine native retirement authority");
                   });
            Check(Hook::Load<std::uint16_t>(source.counter) == 17 && !access.HasPendingPrivateAnimationAssociations(),
                  "mask record retirement balances the real block reference");

            source.nodes = shortMap;
            source.ResetRow(5);
            create();
            const auto                    secondReceiver = Hook::lastReceiver, secondNodes = Hook::Load<std::uintptr_t>(secondReceiver + 0x20);
            const auto                    original = source.children + 32;
            std::array<std::uintptr_t, 5> originalMap{source.sequences, source.sequences, source.sequences, source.sequences, source.sequences};
            Hook::Store(original + 8, std::uint16_t{0xffff});
            Hook::Store(original + 0x10, reinterpret_cast<std::uintptr_t>(originalMap.data()));
            const auto noOps = NodeHook::exchanges;
            action("of-null-source",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBonesOf(nullptr);
                   });
            Hook::Store(original + 0x10, std::uintptr_t{0});
            action("of-null-map-highbit-count",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBonesOf(reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(original));
                   });
            Check(NodeHook::exchanges == noOps, "actual null and qualified null-map paths ignore unsigned high-bit count without inventing work");
            Hook::Store(original + 0x10, reinterpret_cast<std::uintptr_t>(originalMap.data()));
            action("of-highbit-valid-five-prefix",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBonesOf(reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(original));
                   });
            Check(NodeHook::exchanges == noOps && Hook::Load<std::uintptr_t>(secondNodes + 0x10) == source.map[0] &&
                      Hook::Load<std::uintptr_t>(secondNodes + 3 * 40 + 0x10) == source.map[3],
                  "unsignedFFFF source count reads only the valid5-pointer target prefix");
            Hook::Store(original + 8, std::uint16_t{2});
            action("of-shorter-source",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBonesOf(reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(original));
                   });
            Check(Hook::Load<std::uintptr_t>(secondNodes + 0x10) == source.map[0] && !Hook::Load<std::uintptr_t>(secondNodes + 2 * 40 + 0x10) &&
                      !Hook::Load<std::uintptr_t>(secondNodes + 3 * 40 + 0x10),
                  "shorter unsigned source extent suppresses target rows beyond its own count");
            Hook::Store(original + 8, std::uint16_t{0});
            Hook::Store(original + 0x10, std::uintptr_t{1});
            action("of-zero-count-no-map-dereference",
                   [&]
                   {
                       PrivateAnimationAssociationReceiver observer(access, token);
                       observer.RestrictToBonesOf(reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(original));
                   });
            for (std::size_t index = 0; index != 5; ++index)
                Check(!Hook::Load<std::uintptr_t>(secondNodes + index * 40 + 0x10),
                      "zero original count clears every target without reading its poison nonnull map");
            action("retire-original-restricted-association",
                   [&]
                   {
                       Check(access.RetirePrivateAnimationAssociation(token) == ClockResult::Ok,
                             "original-map suppression retires exactly its issued native resources");
                   });
            Check(Hook::Load<std::uint16_t>(source.counter) == 17 && !access.HasPendingPrivateAnimationAssociations() && Hook::constructorCalls == 2 &&
                      Hook::destructorCalls == 2 && host.freeCalls == 4,
                  "two actual constructors and deleting destructors balance each original node/outer resource exactly once");
            if (!service.End())
                fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "bone scope restores actual hooks and releases admission after acknowledged native retirement");
            std::puts("Production animation association bones accepted cleanup completed");
            std::puts("Production native animation association bones owner fixture completed");
            std::puts("Production actual animation association bones SDK owner fixture completed");
            std::puts("Production copied dynamic constructor helper and destructor executed in owned fixture");
        }
        Check(Hook::targetsValid && Hook::snapshotsCopied && !Hook::heapFailed && runtime.modulePinned.load(),
              "bone fixture uses only exact copied native bodies and retains the genuine sticky module PIN");
        ClockReaderTestHost::image = previousImage;
        Hook::image = 0;
        Host::active = nullptr;
        saved.Restore();
    }
}

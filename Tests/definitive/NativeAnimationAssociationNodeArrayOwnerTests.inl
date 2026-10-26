/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exactly-once node-only release through the actual native owner
 ****************************************************************************/
namespace ClockAnimationAssociationNodeArrayTest
{
    namespace Hook = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    unsigned                      frees = 0, clears = 0, guardedDestructors = 0, mode = 0;
    bool                          active = false, actualFreed = false, freeAcknowledged = false, clearAcknowledged = false;
    std::uintptr_t                receiver = 0, nodes = 0, raw = 0, freeTarget = 0;
    std::array<std::uintptr_t, 4> reservations{};
    unsigned                      reservationCount = 0;
    bool                          Unreadable() noexcept
    {
        MEMORY_BASIC_INFORMATION info{};
        return raw && VirtualQuery(reinterpret_cast<void*>(raw), &info, sizeof(info)) == sizeof(info) && info.State == MEM_RESERVE;
    }
    ClockResult Free(NativeCustomAnimSequenceStorageCalls::FreeFunction function, std::uintptr_t heap, std::uintptr_t base, std::uint32_t& exception) noexcept
    {
        ++frees;
        freeTarget = reinterpret_cast<std::uintptr_t>(function);
        Check(active && Host::active && base == raw && Host::active->Contains(base) && Hook::Load<std::uintptr_t>(nodes - 8) == base,
              "node-only release receives the constructor's original aligned allocation base and actual qualified native heap target");
        const auto result = Host::Free(function, heap, base, exception);
        if (result != ClockResult::Ok)
            return result;
        actualFreed = true;
        Check(reservationCount < reservations.size(), "bounded node-only fixture owns at most four unreadable stale-address guards");
        const auto guard = reinterpret_cast<std::uintptr_t>(VirtualAlloc(reinterpret_cast<void*>(base), 0x1000, MEM_RESERVE, PAGE_NOACCESS));
        Check(guard == base, "after genuine bounded heap release reserve the stale address without committing readable bytes");
        reservations[reservationCount++] = guard;
        Check(Unreadable(), "freed node header and payload remain genuinely unreadable throughout metadata and retirement checks");
        if (mode == 1)
        {
            // Consume the real allocation first, then lose only the return
            // acknowledgement. No copied body unwinds a hardware exception.
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        freeAcknowledged = true;
        return result;
    }
    ClockResult Clear(std::uintptr_t address, std::uintptr_t expected, std::uintptr_t target, std::uintptr_t& observed, bool& accepted,
                      std::uint32_t& exception) noexcept
    {
        ++clears;
        Check(active && actualFreed && freeAcknowledged && address == receiver + 0x20 && expected == nodes && target == 0 && Unreadable(),
              "cleanup pointer CAS is separate from public free and runs before any genuine destructor can inspect stale nodes");
        const auto result = AnimationAssociationStorageDetail::ExchangeNode(address, expected, target, observed, accepted, exception);
        if (result == ClockResult::Ok && accepted && mode == 2)
        {
            // Physical CAS happened; its unknown ACK must never authorize a
            // destructor or a second pointer clear during End recovery.
            accepted = false;
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        clearAcknowledged = result == ClockResult::Ok && accepted;
        if (clearAcknowledged && mode == 3)
            Hook::Store(Hook::image + NativeBorrowedGcProfile::markerRva, LONG{1});
        return result;
    }
    ClockResult Destroy(NativeAnimationAssociationCalls::DestructorFunction function, std::uintptr_t object, std::uint32_t flags, std::uintptr_t& returned,
                        std::uint32_t& exception) noexcept
    {
        if (active && actualFreed)
        {
            Check(object == receiver && freeAcknowledged && clearAcknowledged && Hook::Load<std::uintptr_t>(object + 0x20) == 0 && Unreadable(),
                  "guard copied destructor entry on an acknowledged stale-pointer clear without touching unreadable released node bytes");
            ++guardedDestructors;
        }
        return Hook::Destroy(function, object, flags, returned, exception);
    }
    void ReleaseGuards()
    {
        // These CPU-only reservations prevent address reuse in this test;
        // disposing one never supplies any engine allocation/free receipt.
        for (unsigned index = 0; index != reservationCount; ++index)
            Check(VirtualFree(reinterpret_cast<void*>(reservations[index]), 0, MEM_RELEASE) != FALSE, "retire only the task-owned unreadable address guard");
        reservationCount = 0;
    }
}
namespace AnimationAssociationNodeArrayOwnerFixture
{
    namespace Hook = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace NodeHook = ClockAnimationAssociationNodeArrayTest;
    using SourceBank = PrivateAnimationAssociationOwnerFixture::SourceBank;
    using Token = NativeAnimationAssociationReferenceToken;
    using Values = NativeAnimationAssociationValues;
    unsigned      scenario = 0;
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    template <class Action>
    void Throws(Action action, const char* why)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, why);
    }
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
        // Length has its own genuine field/profile prerequisites. Installing
        // them here never adds those dependencies to the old scalar reader.
        admit(NativeAnimationHierarchyReadProfile::Code);
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
              "association node-only release scope pins the actual callback module once for this process");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew a bounded real callback before each single node-only release semantic operation");
            fixture.invokeTimer();
        };
        auto& access = static_cast<NativeAnimationAssociationAccess&>(runtime.access);

        const auto execute = [&](auto action)
        {
            callback.action = [&](NativeGameTimingAccess&) { action(); };
            invoke();
            // Do not retain the local action parameter between callbacks.
            callback.action = [](NativeGameTimingAccess&) {};
        };
        NodeHook::active = true;
        NodeHook::frees = NodeHook::clears = NodeHook::guardedDestructors = NodeHook::mode = 0;
        NodeHook::reservationCount = 0;
        saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x18, 2.0f);
        const auto create = [&](std::uint16_t count, Token& token, std::unique_ptr<PrivateAnimationAssociationReceiver>& observer)
        {
            source.ResetRow(count);
            NodeHook::actualFreed = NodeHook::freeAcknowledged = NodeHook::clearAcknowledged = false;
            execute(
                [&]
                {
                    Check(access.CreatePrivateAnimationAssociation(generation, token) == ClockResult::Ok && token.serial,
                          "node-only release starts from an issued unconstructed native104 record");
                });
            execute(
                [&]
                {
                    observer = std::make_unique<PrivateAnimationAssociationReceiver>(access, token);
                    Check(observer->Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(source.children)) != nullptr,
                          "execute genuine copied constructor and aligned-node allocator through the actual SDK");
                });
            NodeHook::receiver = Hook::lastReceiver;
            NodeHook::nodes = Hook::Load<std::uintptr_t>(NodeHook::receiver + 0x20);
            NodeHook::raw = Hook::Load<std::uintptr_t>(NodeHook::nodes - 8);
            Check(NodeHook::nodes && Host::active->Contains(NodeHook::raw) && Hook::Load<std::uint16_t>(source.counter) == 18 &&
                      Hook::Load<std::uint16_t>(NodeHook::receiver + 0x18) == count && (count || host.bytes == 64),
                  "even count0 owns the genuine nonnull64B allocation and an independent bank reference");
        };
        const auto knownFree = [&](const Token& token, PrivateAnimationAssociationReceiver& observer, bool sdk)
        {
            std::array<unsigned char, 104> before{};
            std::memcpy(before.data(), reinterpret_cast<void*>(NodeHook::receiver), before.size());
            const auto frees = NodeHook::frees, hostFrees = host.freeCalls, clears = NodeHook::clears;
            execute(
                [&]
                {
                    if (sdk)
                        observer.FreeAnimBlendNodeArray();
                    else
                    {
                        NativeAnimationAssociationCalls::Receipt receipt;
                        Check(runtime.privateAssociations.FreeNodeArray(fixture.image, generation, token, &CheckTimingOwner, &receipt) == ClockResult::Ok &&
                                  receipt.entered && receipt.returned && receipt.accepted && receipt.verified && receipt.nodeFreeEntered &&
                                  receipt.nodesReleased && receipt.nodeFreeVerified && !receipt.nodePointerClearEntered && !receipt.retired &&
                                  fixture.image + receipt.nodeFreeEntryRva == NodeHook::freeTarget,
                              "native controller publishes known node-free return and verified phase independently of pointer clear or retirement");
                    }
                });
            Check(NodeHook::frees == frees + 1 && host.freeCalls == hostFrees + 1 && NodeHook::Unreadable() &&
                      !std::memcmp(before.data(), reinterpret_cast<void*>(NodeHook::receiver), before.size()) &&
                      Hook::Load<std::uint16_t>(source.counter) == 18 && NodeHook::clears == clears,
                  "public node-only free preserves all104 receiver bytes stale pointer/count and real bank reference while consuming only node raw storage");
        };
        if (scenario)
        {
            Token                                                token;
            std::unique_ptr<PrivateAnimationAssociationReceiver> observer;
            create(5, token, observer);
            std::array<unsigned char, 104> before{};
            std::memcpy(before.data(), reinterpret_cast<void*>(NodeHook::receiver), before.size());
            if (scenario == 1)
            {
                NodeHook::mode = 1;
                execute(
                    [&]
                    { Throws([&] { observer->FreeAnimBlendNodeArray(); }, "actual SDK exposes lost node-free acknowledgement after allocation consumption"); });
                Check(NodeHook::frees == 1 && NodeHook::Unreadable() && !std::memcmp(before.data(), reinterpret_cast<void*>(NodeHook::receiver), before.size()),
                      "unknown free keeps physical stale pointer/count unchanged without claiming a known release");
            }
            else
            {
                knownFree(token, *observer, true);
                NodeHook::mode = scenario == 2 ? 2u : 3u;
                execute(
                    [&]
                    {
                        const auto result = access.RetirePrivateAnimationAssociation(token);
                        Check(result == (scenario == 2 ? ClockResult::WriteFailure : ClockResult::Busy),
                              "retirement distinguishes unknown clear ACK from an accepted clear followed by GC-owner loss");
                    });
                Check(NodeHook::clears == 1 && !Hook::Load<std::uintptr_t>(NodeHook::receiver + 0x20) && !Hook::destructorCalls && NodeHook::Unreadable(),
                      "cleanup CAS physically clears only the stale native pointer before any destructor entry");
            }
            Check(service.IsRetained() && runtime.privateAssociations.IsRetained() &&
                      (scenario == 3 ? !runtime.privateAssociations.RecoveryPending() : runtime.privateAssociations.RecoveryPending()) &&
                      runtime.modulePinned.load() && Hook::Load<std::uint16_t>(source.counter) == 18 && host.freeCalls == 1 && !Hook::destructorCalls,
                  "uncertain free or incomplete clear keeps actual outer heap bank ref callback and module debt");
            Check(!service.End(), "entered node-release effects cannot acknowledge producer shutdown before qualified cleanup");
            saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            fixture.invokeTimer();
            if (scenario == 3)
            {
                Check(service.End() && !service.IsRetained() && !runtime.privateAssociations.IsRetained() && NodeHook::frees == 1 && NodeHook::clears == 1 &&
                          NodeHook::guardedDestructors == 1 && Hook::destructorCalls == 1 && host.freeCalls == 2 &&
                          Hook::Load<std::uint16_t>(source.counter) == 17,
                      "known accepted clear resumes through read-only ACK and exactly one genuine destructor without another CAS or node free");
                NodeHook::ReleaseGuards();
                std::puts("Production animation association node-array accepted clear recovery completed");
            }
            else
            {
                Check(!service.End() && service.IsRetained() && runtime.privateAssociations.IsRetained() && runtime.privateAssociations.RecoveryPending() &&
                          NodeHook::frees == 1 && NodeHook::clears == (scenario == 2 ? 1u : 0u) && !Hook::destructorCalls && host.freeCalls == 1 &&
                          Hook::Load<std::uintptr_t>(NodeHook::receiver + 0x20) == (scenario == 2 ? std::uintptr_t{0} : NodeHook::nodes) &&
                          NodeHook::Unreadable() && Hook::Load<std::uint16_t>(source.counter) == 18 && runtime.modulePinned.load() &&
                          NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                      "unknown free or pointer-clear acknowledgement pins without replay double-free destructor or stale pointer copyback");
                std::puts(scenario == 1 ? "Production animation association node-array unknown free retained"
                                        : "Production animation association node-array unknown clear retained");
                std::printf("Passed %u production clock service checks\n", checks.load());
                std::fflush(nullptr);
                std::_Exit(0);
            }
        }
        else
        {
            for (const std::uint16_t count : {std::uint16_t{5}, std::uint16_t{0}})
            {
                Token                                                token;
                std::unique_ptr<PrivateAnimationAssociationReceiver> observer;
                create(count, token, observer);
                knownFree(token, *observer, count == 0);
                execute(
                    [&]
                    {
                        Values value;
                        Check(access.ReadPrivateAnimationAssociation(token, value) == ClockResult::Ok && value.interfaceAddress == NodeHook::receiver &&
                                  value.speedBits == Bits(1.0f) && value.currentTimeBits == 0 && NodeHook::Unreadable(),
                              "known freed node memory remains unreadable while the actual native receiver metadata requalifies");
                    });
                execute(
                    [&]
                    {
                        Check(reinterpret_cast<std::uintptr_t>(observer->GetInterface()) == NodeHook::receiver && NodeHook::Unreadable(),
                              "actual SDK interface read never touches released nodes");
                    });
                execute(
                    [&]
                    {
                        Check(Bits(observer->GetLength()) == Bits(2.0f) && NodeHook::Unreadable(),
                              "actual SDK duration read borrows the current bank without released node access");
                    });
                execute(
                    [&]
                    {
                        Check(Bits(observer->GetCurrentSpeed()) == Bits(1.0f) && NodeHook::Unreadable(),
                              "actual SDK scalar read remains valid with unreadable released nodes");
                    });
                execute(
                    [&]
                    {
                        Check(Bits(observer->GetCurrentProgress()) == 0 && NodeHook::Unreadable(),
                              "actual SDK absolute-time read never reconstructs node state");
                    });
                std::unique_ptr<CAnimBlendHierarchy> hierarchy;
                execute(
                    [&]
                    {
                        hierarchy = observer->GetAnimHierarchy();
                        Check(hierarchy && NodeHook::Unreadable(), "same-owner hierarchy facade borrows the receiver/bank origin without released node access");
                    });
                execute(
                    [&]
                    {
                        Check(reinterpret_cast<std::uintptr_t>(hierarchy->GetInterface()) == source.hierarchy && NodeHook::Unreadable(),
                              "borrowed hierarchy facade independently requalifies its current bank interface");
                    });
                hierarchy.reset();
                if (count)
                {
                    execute([&] { observer->SetCurrentSpeed(1.25f); });
                    execute(
                        [&]
                        {
                            Check(Bits(observer->GetCurrentSpeed()) == Bits(1.25f) && NodeHook::Unreadable(),
                                  "one actual SDK scalar write/read remains legal in known node-less phase");
                        });
                }
                const auto noNodeEntry = [&](auto operation)
                {
                    const auto frees = NodeHook::frees, clears = NodeHook::clears;
                    execute(
                        [&]
                        {
                            NativeAnimationAssociationCalls::Receipt receipt;
                            Check(operation(receipt) == ClockResult::NotInitialized && !receipt.entered && receipt.nodesReleased && receipt.nodeFreeVerified,
                                  "node consumers and repeated free refuse NotInitialized before entry instead of inspecting freed nodes");
                        });
                    Check(NodeHook::frees == frees && NodeHook::clears == clears && NodeHook::Unreadable(),
                          "refused node work never retries release or cleanup CAS");
                };
                noNodeEntry([&](auto& r) { return runtime.privateAssociations.FreeNodeArray(fixture.image, generation, token, &CheckTimingOwner, &r); });
                noNodeEntry([&](auto& r)
                            { return runtime.privateAssociations.SetCurrentProgress(fixture.image, generation, token, Bits(0.5f), &CheckTimingOwner, &r); });
                noNodeEntry(
                    [&](auto& r)
                    { return runtime.privateAssociations.RestrictToBones(fixture.image, generation, token, ~std::uint64_t{0}, &CheckTimingOwner, &r); });
                noNodeEntry(
                    [&](auto& r)
                    { return runtime.privateAssociations.RestrictToBonesOf(fixture.image, generation, token, source.children, &CheckTimingOwner, &r); });
                const auto clears = NodeHook::clears, frees = host.freeCalls, destructors = Hook::destructorCalls;
                observer.reset();
                Check(host.freeCalls == frees, "CPU observer destruction does not free or retire the native association");
                execute(
                    [&]
                    {
                        NativeAnimationAssociationCalls::Receipt receipt;
                        Check(runtime.privateAssociations.RetireOne(fixture.image, generation, token, &CheckTimingOwner, &receipt) == ClockResult::Ok &&
                                  receipt.nodePointerClearEntered && receipt.nodePointerCleared && receipt.nodePointerClearVerified && receipt.retired &&
                                  receipt.verified,
                              "retirement records its separate pointer-clear ACK before the genuine deleting destructor");
                    });
                Check(NodeHook::clears == clears + 1 && host.freeCalls == frees + 1 && Hook::destructorCalls == destructors + 1 &&
                          Hook::Load<std::uint16_t>(source.counter) == 17 && !access.HasPendingPrivateAnimationAssociations() && NodeHook::Unreadable(),
                      "node-less retirement frees outer only and balances the held bank ref exactly once");
            }
            Check(NodeHook::frees == 2 && NodeHook::clears == 2 && NodeHook::guardedDestructors == 2 && Hook::constructorCalls == 2 &&
                      Hook::destructorCalls == 2 && host.freeCalls == 4 && Hook::heapAllocations == 2 && Hook::heapFrees == 2 && Hook::targetsValid &&
                      Hook::snapshotsCopied && !Hook::heapFailed,
                  "count5 and count0 each execute genuine constructor node-only free and guarded destructor without double-free or old suite replay");
            Check(!service.End(), "known native resources retire before natural callback hook shutdown");
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.privateAssociations.IsRetained() && runtime.modulePinned.load() &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "successful node-only release lifecycle drains current callback and admission once");
            NodeHook::ReleaseGuards();
            std::puts("Production native animation association node-array owner fixture completed");
            std::puts("Production actual animation association node-array SDK owner fixture completed");
            std::puts("Production copied constructor allocator destructor and native node-free heap protocol executed in owned fixture");
        }
        ClockReaderTestHost::image = previousImage;
        Hook::image = 0;
        Host::active = nullptr;
        NodeHook::active = false;
        saved.Restore();
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact current-player reapply through the real after-app owner
 ****************************************************************************/
#include "../../Client/game_de/NativeAnimationHierarchyReadProfile.h"
namespace PlayerMoveAnimReapplyOwnerFixture
{
    namespace Hook = ClockPlayerMoveAnimReapplyOwnerTest;
    namespace P = NativePlayerMoveAnimReapplyProfile;
    namespace R = NativePedReferenceProfile;
    namespace Ped = NativePedPoolProfile;
    namespace Player = NativePlayerInfoProfile;
    namespace Call = NativePlayerMoveAnimReapply;
    template <class T>
    void Store(std::uintptr_t address, const T& value)
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }
    struct Sources
    {
        // These owned raw arrays extend the existing genuine hierarchy fixture;
        // no test factory grants production allocation or task lifetime.
        PrivateAnimationAssociationOwnerFixture::SourceBank source;
        std::uintptr_t                                      groupAllocation = 0, childAllocation = 0, groups = 0, children = 0;
        explicit Sources(ClockImageFixture& fixture, WorldGroundOwnerFixture::Writes& writes) : source(fixture, writes)
        {
            constexpr std::uint64_t groupCapacity = 1, childCapacity = 6;
            groupAllocation = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 8 + groupCapacity * 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            childAllocation = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 8 + childCapacity * 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            Check(groupAllocation && childAllocation, "own bounded raw current animation group/source storage");
            groups = groupAllocation + 8;
            children = childAllocation + 8;
            Store(groupAllocation, groupCapacity);
            Store(childAllocation, childCapacity);
            for (std::uint32_t index = 0; index != childCapacity; ++index)
            {
                const auto row = children + index * 32;
                std::memcpy(reinterpret_cast<void*>(row), reinterpret_cast<void*>(source.children), 32);
                Store(row + NativeAnimationStaticAssociationProfile::IdOffset, static_cast<std::int16_t>(index));
            }
            for (std::uint32_t index = 0; index != groupCapacity; ++index)
            {
                const auto row = groups + index * 32;
                Store(row + 8, children);
                Store(row + 0x10, std::uint32_t{6});
                Store(row + 0x18, index);
            }
            writes.Global(NativeAnimationGroupArrayProfile::GlobalRva, groups);
            writes.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{1});
        }
        ~Sources()
        {
            for (const auto address : {groupAllocation, childAllocation})
                if (address)
                    VirtualFree(reinterpret_cast<void*>(address), 0, MEM_RELEASE);
        }
    };
    struct LateProbe
    {
        unsigned                     calls = 0, trigger = 0;
        std::uintptr_t               address = 0;
        std::array<unsigned char, 8> replacement{};
        std::size_t                  bytes = 0;
    };
    LateProbe*  currentLateProbe = nullptr;
    ClockResult LateAfterApplicationOwner() noexcept
    {
        // Keep the genuine current callback admission. The only injected side
        // effect is a bounded replacement in test-owned storage AFTER it passes.
        const auto result = CheckApplicationTimingOwner();
        if (result == ClockResult::Ok && currentLateProbe && ++currentLateProbe->calls == currentLateProbe->trigger)
            std::memcpy(reinterpret_cast<void*>(currentLateProbe->address), currentLateProbe->replacement.data(), currentLateProbe->bytes);
        return result;
    }
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, bytes, size, protection); };
        const auto code = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        const auto typed = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        code(NativeStreamingLoadAllProfile::Code);
        typed(NativeStartupOwnerProfile::pins);
        typed(NativeStartupWorldProfile::pins);
        typed(NativeGameStateProfile::pins);
        code(NativeUObjectMembershipProfile::pins);
        code(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        // Native world admission and the normal application caller share
        // RootRva5725750. Their one owned receiver must carry BOTH roles;
        // publishing a different frame object would discard the typed graph.
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeCustomAnimSequenceStorageFixture::State allocations;
        allocations.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        code(Ped::Code);
        code(R::Code);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        code(NativeAnimationBlockProfile::Code);
        writes.Global(NativeAnimationBlockProfile::RemapRva + NativeAnimationBlockProfile::LoadedOpcodeIndex,
                      static_cast<unsigned char>(NativeAnimationBlockProfile::LoadedSelector), PAGE_EXECUTE_READ);
        writes.Global(NativeAnimationBlockProfile::DispatchRva + NativeAnimationBlockProfile::LoadedSelector * 4,
                      static_cast<std::uint32_t>(NativeAnimationBlockProfile::LoadedBranchRva), PAGE_EXECUTE_READ);
        writes.Global(NativeAnimationBlockProfile::GroupSlotRva, fixture.image + NativeAnimationBlockProfile::GroupRva, PAGE_READONLY);
        code(NativeAnimationBlockRefProfile::Code);
        code(NativeAnimationGroupArrayProfile::Code);
        code(NativeAnimationStaticAssociationProfile::Code);
        code(NativeAnimationStaticAssociationProfile::ReadCode);
        code(NativeAnimationHierarchyReadProfile::Code);
        code(NativeAnimationAssociationProfile::Code);
        writes.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        Sources sources(fixture, writes);
        writes.Global(Ped::RootRva, std::uintptr_t{0});
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto                                               ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        alignas(std::uintptr_t) std::array<unsigned char, 0x100> model{};
        NativePedRebuildFixture::State                           state;
        Check(state.Ready(), "own the existing intelligence/task pools without publishing SDK task products");
        state.Reset(fixture.image, ped, reinterpret_cast<std::uintptr_t>(model.data()), 0, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        // Rebuild's general render fixture has a synthetic list sentinel. This
        // distinct current clump deliberately models the original's valid EMPTY list.
        Store(state.Container(), std::uintptr_t{0});
        Store(ped, fixture.image + Player::PlayerVtableRva);
        pool.objects[R::TypeOffset] = 3;
        Store(ped + NativePedMoveAnimProfile::GroupOffset, std::int32_t{0});
        code(NativePedMoveAnimProfile::Code);
        writes.Global(NativePedMoveAnimProfile::GroupSlotRva, fixture.image + NativePedMoveAnimProfile::GroupRva, PAGE_READONLY);
        code(P::Pins);
        std::array<unsigned char, 2 * Player::RowBytes> players{};
        apply(Player::BankRva, players.data(), players.size(), PAGE_READWRITE);
        const auto publish = [&](std::int32_t selector)
        {
            const auto row = fixture.image + Player::BankRva + static_cast<std::uintptr_t>(selector) * Player::RowBytes;
            writes.Global(Player::BankRva, std::uintptr_t{0});
            writes.Global(Player::BankRva + Player::RowBytes, std::uintptr_t{0});
            writes.Global(Player::BankRva + static_cast<std::uintptr_t>(selector) * Player::RowBytes, ped);
            Store(ped + 0x5f0, row + 8);
            Store(ped + 0x7e0, selector);
        };
        publish(0);
        std::vector<unsigned char> references(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t index = 0; index + 1 != R::ReferenceCount; ++index)
        {
            const auto next = fixture.image + R::ReferencesRva + (index + 1) * R::ReferenceBytes;
            std::memcpy(references.data() + index * R::ReferenceBytes, &next, sizeof(next));
        }
        apply(R::ReferencesRva, references.data(), references.size(), PAGE_READWRITE);
        writes.Global(R::FreeHeadRva, fixture.image + R::ReferencesRva);
        graph.Reset();
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        // Public Begin does not construct this independent synthetic receiver.
        // Its pinned normal caller dispatches through +3b0 before our hook, so
        // seed the existing owned vtable exactly as BeginApplicationOwned does.
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image, previousReferenceImage = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        Hook::image = fixture.image;
        Hook::ped = ped;
        Hook::scenario = scenario;
        Hook::calls = Hook::effects = 0;
        Hook::callerToken = nullptr;
        Hook::retireToken = nullptr;
        Hook::retireResult = Hook::nestedResult = ClockResult::NotInitialized;
        Hook::retire = +[](const NativePedReferenceToken& selected) noexcept { return runtime.access.ReleasePedReference(selected); };
        Hook::after = nullptr;
        struct DefaultAccess final : NativePedMoveAnimAccess
        {
        } absent;
        Check(absent.ReapplyCurrentPlayerMoveAnim({}) == ClockResult::NotInitialized,
              "older providers refuse the optional internal effect without inventing a completed public setter");
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.modulePinned,
              "current-player effect preserves shipping executable admission before any native hook or process pin");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.bound && admitted.hooked && runtime.frameHooked.load() && CheckBinding(),
              "effect fixture uses actual public timing-after-application admission and its live detour binding");
        const auto generation = admitted.generation;
        const auto pulse = [&](const char* label, std::function<void(NativeGameTimingAccess&)> action)
        {
            callback.action = [&, label, action = std::move(action)](NativeGameTimingAccess& access)
            {
                const auto start = GetTickCount64();
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "new native tail runs only in the genuine distinct after-application callback");
                action(access);
                std::printf("Reapply phase %s elapsed %llu ms\n", label, static_cast<unsigned long long>(GetTickCount64() - start));
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
        };
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        pulse("acquire", [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); });
        Check(result == ClockResult::Ok && runtime.pedReferences.IsRetained(), "real private weak-cell owner issues the exact current player token");
        const auto                        fixedToken = token;
        std::unique_ptr<Call::Controller> retainedLateController;
        const auto                        lateCheck = [&](const char* label, std::uintptr_t address, std::uint64_t replacement, std::size_t bytes, bool entered)
        {
            pulse(label,
                  [&](NativeGameTimingAccess& access)
                  {
                      NativePedPoolReader::Value current;
                      Check(access.ResolvePedReference(fixedToken, current) == ClockResult::Ok,
                            "late-probe test first resolves the actual same registered player");
                      std::array<unsigned char, 8> before{};
                      Check(bytes && bytes <= before.size(), "late mutation stays inside one explicitly owned scalar/pointer field");
                      std::memcpy(before.data(), reinterpret_cast<void*>(address), bytes);
                      LateProbe probe;
                      probe.trigger = entered ? 8u : 5u;
                      probe.address = address;
                      probe.bytes = bytes;
                      std::memcpy(probe.replacement.data(), &replacement, bytes);
                      Call::Controller local;
                      if (entered)
                          retainedLateController = std::make_unique<Call::Controller>();
                      auto&         controller = entered ? *retainedLateController : local;
                      Call::Receipt receipt;
                      const auto    calls = Hook::calls;
                      const auto    priorScenario = Hook::scenario;
                      Hook::scenario = 0;
                      currentLateProbe = &probe;
                      const auto observed = controller.Reapply(fixture.image, current, &CheckTimingOwner, &LateAfterApplicationOwner, &receipt);
                      currentLateProbe = nullptr;
                      Hook::scenario = priorScenario;
                      if (!(observed != ClockResult::Ok && probe.calls == probe.trigger && Hook::calls == calls + (entered ? 1u : 0u)))
                      {
                          // Failure-only native observations separate incomplete
                          // fixture admission from rejection at the intended final
                          // bookend. None supplies a successful replacement result.
                          NativePedMetadata                  metadata;
                          NativePedIntelligenceReader::Value intelligence;
                          std::int32_t                       currentGroup = -1;
                          const auto metadataResult = NativePedMetadataMemory({fixture.image}, &CheckTimingOwner).Read(current, metadata);
                          const auto intelligenceResult = NativePedIntelligenceReader({fixture.image}, &CheckTimingOwner).Read(current, intelligence);
                          const auto groupResult = NativePedMoveAnimReader({fixture.image}, &CheckTimingOwner).Read(current, currentGroup);
                          NativeAnimationManagerMemory                             manager({fixture.image}, &CheckTimingOwner);
                          NativeAnimationAssociationGroupReferenceToken            groupToken;
                          NativeAnimationManagerMemory::AssociationGroupCopySource selected;
                          NativeAnimationStaticAssociationValues                   values;
                          NativeAnimationHierarchyValues                           hierarchy;
                          const auto                                               resolveResult = manager.ResolveAssociationGroup(0, groupToken);
                          const auto                                               sourceResult =
                              resolveResult == ClockResult::Ok ? manager.ReadAssociationGroupCopySource(groupToken, 0, selected) : resolveResult;
                          const auto readResult = sourceResult == ClockResult::Ok ? manager.ReadStaticAssociation(selected.selected, values) : sourceResult;
                          const auto hierarchyResult =
                              readResult == ClockResult::Ok
                                  ? NativeAnimationHierarchyMemory({fixture.image}, &CheckTimingOwner).Read(values.hierarchyAddress, hierarchy)
                                  : readResult;
                          std::uintptr_t child = 0;
                          std::uint32_t  count = 0, offset = 0;
                          std::uint64_t  groupCapacity = 0, childCapacity = 0;
                          std::memcpy(&child, reinterpret_cast<void*>(sources.groups + 8), sizeof(child));
                          std::memcpy(&count, reinterpret_cast<void*>(sources.groups + 0x10), sizeof(count));
                          std::memcpy(&offset, reinterpret_cast<void*>(sources.groups + 0x14), sizeof(offset));
                          std::memcpy(&groupCapacity, reinterpret_cast<void*>(sources.groups - 8), sizeof(groupCapacity));
                          std::memcpy(&childCapacity, reinterpret_cast<void*>(sources.children - 8), sizeof(childCapacity));
                          std::fprintf(stderr,
                                       "Reapply late diagnostic label=%s result=%u probes=%u/%u phase=%u entered=%u returned=%u unknown=%u calls=%u/%u "
                                       "metadata=%u intelligence=%u move=%u resolve=%u source=%u static=%u hierarchy=%u currentGroup=%d "
                                       "profilesMove=%u stream=%u heap=%u parent=%llx child=%llx count=%u offset=%u capacities=%llu/%llu\n",
                                       label, static_cast<unsigned>(observed), probe.calls, probe.trigger, static_cast<unsigned>(receipt.phase),
                                       static_cast<unsigned>(receipt.entered), static_cast<unsigned>(receipt.returned), static_cast<unsigned>(receipt.unknown),
                                       Hook::calls, calls, static_cast<unsigned>(metadataResult), static_cast<unsigned>(intelligenceResult),
                                       static_cast<unsigned>(groupResult), static_cast<unsigned>(resolveResult), static_cast<unsigned>(sourceResult),
                                       static_cast<unsigned>(readResult), static_cast<unsigned>(hierarchyResult), static_cast<int>(currentGroup),
                                       static_cast<unsigned>(NativePedMoveAnimReader::Revalidate({fixture.image})),
                                       static_cast<unsigned>(NativeStreamingCompletionContext::Profile(fixture.image)),
                                       static_cast<unsigned>(NativeModelReferenceHeap::QualifyProfile(fixture.image)),
                                       static_cast<unsigned long long>(sources.groups), static_cast<unsigned long long>(child), count, offset,
                                       static_cast<unsigned long long>(groupCapacity), static_cast<unsigned long long>(childCapacity));
                      }
                      Check(observed != ClockResult::Ok && probe.calls == probe.trigger && Hook::calls == calls + (entered ? 1u : 0u),
                            "callback-free final anchors reject replacement made by the final genuine owner probe");
                      Check(entered ? receipt.entered && receipt.returned && receipt.unknown && !receipt.acknowledged && controller.RecoveryPending()
                                    : !receipt.entered && !receipt.returned && !receipt.unknown && !controller.IsRetained(),
                            "late pre-entry refusal has no effect while post-entry loss truthfully retains the unknown return");
                      if (entered)
                      {
                          const auto called = Hook::calls;
                          Check(controller.Reapply(fixture.image, current, &CheckTimingOwner, &CheckApplicationTimingOwner) == ClockResult::Stopped &&
                                    Hook::calls == called && controller.IsRetained(),
                                "a retained direct-controller unknown cannot replay even inside the same real callback");
                      }
                      // Reinitialize ONLY the fixture's injected scalar for the
                      // following independent scenario; the uncertainty journal is
                      // deliberately retained and production End never repairs it.
                      std::memcpy(reinterpret_cast<void*>(address), before.data(), bytes);
                  });
        };
        Check(runtime.access.ReapplyCurrentPlayerMoveAnim(token) != ClockResult::Ok && !Hook::calls && !runtime.stopping.load(),
              "producer outside after-app cannot invoke the native entry");
        std::thread foreign([&] { result = runtime.access.ReapplyCurrentPlayerMoveAnim(token); });
        foreign.join();
        Check(result != ClockResult::Ok && !Hook::calls && !runtime.stopping.load(), "foreign thread refuses before effect or owner Stop");
        if (!scenario)
        {
            lateCheck("late-pool-root", fixture.image + Ped::RootRva, 0, sizeof(std::uintptr_t), false);
            lateCheck("late-clump-head", state.Container(), 0x11111110, sizeof(std::uintptr_t), false);
            alignas(std::uintptr_t) std::array<unsigned char, NativeAnimationAssociationProfile::ReceiverBytes> association{};
            const auto associationAddress = reinterpret_cast<std::uintptr_t>(association.data());
            Store(associationAddress, fixture.image + NativeAnimationAssociationProfile::VtableRva);
            Store(associationAddress + 16, state.Container());
            Store(associationAddress + 0x28, sources.source.hierarchy);
            Store(state.Container(), associationAddress + 8);
            lateCheck("late-association-row", associationAddress + 0x50, 0x13579bdf, sizeof(std::uint32_t), false);
            Store(state.Container(), std::uintptr_t{0});
            lateCheck("late-selected-child", sources.children + 0x18, 0, sizeof(std::uintptr_t), false);
            lateCheck("late-hierarchy-prefix", sources.source.hierarchy, 0x2468ace0, sizeof(std::uint32_t), false);
            auto stale = token;
            ++stale.ownerGeneration;
            pulse("stale", [&](NativeGameTimingAccess& access) { result = access.ReapplyCurrentPlayerMoveAnim(stale); });
            Check(result != ClockResult::Ok && !Hook::calls && !runtime.playerMoveAnimReapply.IsRetained() && !runtime.stopping.load(),
                  "stale CPU identity refuses before native admission without debt");
            const auto savedSelector = std::int32_t{0};
            Store(ped + 0x7e0, std::int32_t{2});
            pulse("unsupported-selector", [&](NativeGameTimingAccess& access) { result = access.ReapplyCurrentPlayerMoveAnim(token); });
            Store(ped + 0x7e0, savedSelector);
            Check(result != ClockResult::Ok && !Hook::calls && !runtime.playerMoveAnimReapply.IsRetained() && !runtime.stopping.load(),
                  "unpublished selector refuses before call without guessing a local-only player");
            for (std::int32_t selector = 0; selector != 2; ++selector)
            {
                publish(selector);
                Hook::callerToken = &token;
                Hook::after = +[]() noexcept { Hook::nestedResult = runtime.access.ReapplyCurrentPlayerMoveAnim(Hook::selected); };
                Hook::selected = fixedToken;
                const auto before = Hook::calls;
                pulse(selector ? "player-one" : "player-zero", [&](NativeGameTimingAccess& access) { result = access.ReapplyCurrentPlayerMoveAnim(token); });
                const auto receipt = runtime.playerMoveAnimReapplyReceipt;
                Check(result == ClockResult::Ok && Hook::calls == before + 1 && receipt.phase == Call::Phase::Acknowledged && receipt.entered &&
                          receipt.returned && receipt.acknowledged && !receipt.unknown && receipt.nativePed == ped && receipt.currentSelector == selector &&
                          receipt.beforeGroup == 0 && receipt.afterGroup == 0 && !runtime.playerMoveAnimReapply.IsRetained() && !runtime.stopping.load(),
                      "both genuinely published player rows acknowledge exactly one original using their current group");
                Check(Hook::entry == fixture.image + P::EntryRva && Hook::received == ped && !allocations.mallocCalls && !allocations.freeCalls,
                      "typed owned seam receives the whole qualified entry and exact player without allocator substitution");
                Check(token.version != fixedToken.version && Hook::nestedResult == ClockResult::Busy,
                      "native request captures the original token before seam mutation and rejects nested replay");
                token = fixedToken;
                Hook::after = nullptr;
            }
            std::printf("Production player move animation reapply actual runtime completed\n");
            std::printf("Production player move animation reapply same token completed\n");
            std::printf("Production player move animation reapply refusals completed\n");
            pulse("retire", [&](NativeGameTimingAccess& access) { result = access.ReleasePedReference(token); });
            Check(result == ClockResult::Ok && !runtime.pedReferences.IsRetained(), "successful native ACK does not create strong entity ownership");
            if (!service.End())
            {
                engine.Pulse(service, generation, true);
                Check(service.End(), "actual engine acknowledges ordinary owner restoration before successful End");
            }
            Check(!service.IsRetained() && !runtime.playerMoveAnimReapply.IsRetained(), "acknowledged native tail needs no End rollback or retained record");
        }
        else
        {
            if (scenario == 2)
                lateCheck("returned-late-selected-child", sources.children + 0x18, 0, sizeof(std::uintptr_t), true);
            const auto actualCallsBefore = Hook::calls;
            if (scenario == 3)
                Hook::retireToken = &token;
            pulse("uncertainty",
                  [&](NativeGameTimingAccess& access)
                  {
                      result = access.ReapplyCurrentPlayerMoveAnim(token);
                      if (scenario != 3)
                          Check(access.ReleasePedReference(fixedToken) == ClockResult::Ok, "existing producer recovery retires its private cell after Stop");
                  });
            const auto receipt = runtime.playerMoveAnimReapplyReceipt;
            Check(result != ClockResult::Ok && Hook::calls == actualCallsBefore + 1 && receipt.phase == Call::Phase::Unknown && receipt.entered &&
                      receipt.unknown && !receipt.acknowledged && runtime.playerMoveAnimReapply.IsRetained() &&
                      runtime.playerMoveAnimReapply.RecoveryPending() && runtime.stopping.load() && NativeLifetimeRetained() &&
                      !runtime.pedReferences.IsRetained(),
                  "first uncertain native prefix survives private-cell retirement and independently retains the module");
            Check((scenario == 1 && !receipt.returned && receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION) || (scenario == 2 && receipt.returned) ||
                      (scenario == 3 && receipt.returned && Hook::retireResult == ClockResult::Ok),
                  "entry, returned bookend loss and outer token retirement keep distinct truthful acknowledgements");
            const auto calls = Hook::calls;
            Check(runtime.access.ReapplyCurrentPlayerMoveAnim(fixedToken) != ClockResult::Ok && Hook::calls == calls && !service.End(),
                  "stopped uncertainty refuses replay and End cannot acknowledge an unobserved native outcome");
            ClockServiceStateV1 denied;
            Check(!service.BeginGameTiming(request, denied, nullptr, nullptr) && Hook::calls == calls && runtime.playerMoveAnimReapply.IsRetained(),
                  "new generation cannot discard the first uncertain original or replay it");
            if (scenario == 2)
            {
                std::int32_t group = 0;
                std::memcpy(&group, reinterpret_cast<void*>(ped + NativePedMoveAnimProfile::GroupOffset), sizeof(group));
                Check(group == 1, "Stop and End preserve the observed native group change without rollback");
            }
            std::printf("Production player move animation reapply quarantine completed\n");
            std::printf("Production player move animation reapply no replay completed\n");
        }
        Check(!allocations.mallocCalls && !allocations.freeCalls, "fixture never substitutes an allocator or invokes a game routine");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        ClockPedReferenceOwnerTest::image = previousReferenceImage;
        std::printf("Production player move animation reapply focused owner completed\n");
    }
}

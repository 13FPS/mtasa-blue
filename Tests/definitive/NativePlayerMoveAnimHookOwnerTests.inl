/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real Runtime/token/peer-hold hook lifecycle in owned image storage
 ****************************************************************************/
#include <intrin.h>
#include "NativePlayerMoveAnimHookRangeFixture.h"
#include "PlayerMoveAnimHookOverlayTests.inl"
namespace ClockMoveAnimHookOwnerTest
{
    std::uintptr_t rangeImage = 0;
    std::size_t    rangeImageBytes = 0;
    bool           Range(const void* function, NativeFunctionRange& output) noexcept
    {
        const auto address = reinterpret_cast<std::uintptr_t>(function);
        if (rangeImage && address >= rangeImage && address - rangeImage < rangeImageBytes)
        {
            const auto rva = static_cast<std::uint32_t>(address - rangeImage);
            if (rva != NativePlayerMoveAnimHookProfile::ProcessContextRva && rva != NativePedRebuildProfile::EntryRva &&
                rva != NativePlayerMoveAnimHookProfile::ClothesConsumerRva)
                return false;
            for (const auto& entry : NativePlayerMoveAnimHookRangeFixture::Entries)
                if (rva >= entry.begin && rva < entry.end)
                    return NativeFunctionRangeDetail::Decode(reinterpret_cast<void*>(rangeImage), rangeImageBytes, rva, entry, output);
            return false;
        }
        return mta::game_de::FindNativeFunctionRange(function, output);
    }
    struct alignas(16) Snapshot
    {
        std::array<std::uint64_t, 15> gpr{};
        std::uint64_t                 flags = 0;
        alignas(16) std::array<unsigned char, 512> fx{};
    };
    static_assert(sizeof(Snapshot) == 640 && offsetof(Snapshot, fx) == 128);
    volatile LONG            failDelete = 0, deleteCalls = 0;
    volatile LONG            injectPartial = 0, partialInjected = 0, partialParked = 0;
    unsigned                 consumerCalls = 0;
    std::uint32_t            consumerGroup = 0;
    ApplicationFrameFunction savedOriginal = nullptr;
    std::function<void()>    originalAction;
    std::function<void()>    rebuildAction;
    void                     Consumer(std::uint32_t group) noexcept
    {
        ++consumerCalls;
        consumerGroup = group;
        Check(CheckTimingOwner() != ClockResult::Ok && CheckApplicationTimingOwner() != ClockResult::Ok,
              "the original clothes consumer cannot borrow general timing writes");
    }
    std::uintptr_t ClothesConsumer(std::uintptr_t) noexcept
    {
        return reinterpret_cast<std::uintptr_t>(&Consumer);
    }
    BOOLEAN DeleteFunctions(PRUNTIME_FUNCTION functions) noexcept
    {
        InterlockedIncrement(&deleteCalls);
        if (InterlockedCompareExchange(&failDelete, 0, 0))
            return FALSE;
        return ::RtlDeleteFunctionTable(functions);
    }
    BOOL Protect(LPVOID address, SIZE_T bytes, DWORD protection, PDWORD previous) noexcept
    {
        const auto result = ::VirtualProtect(address, bytes, protection, previous);
        // Fault only the owned second source, after the first site's restore
        // succeeds. This runs without allocation/logging while real peers park.
        if (result && rangeImage && reinterpret_cast<std::uintptr_t>(address) == rangeImage + NativePlayerMoveAnimHookProfile::ProcessCompareRva &&
            bytes == 6 && protection == PAGE_EXECUTE_READ && InterlockedCompareExchange(&injectPartial, 0, 1) == 1)
        {
            partialParked = runtime.playerMoveAnimHookPeersParked.load() && runtime.playerMoveAnimHookPeers.Held() ? 1 : 0;
            auto* target = reinterpret_cast<unsigned char*>(rangeImage + NativePlayerMoveAnimHookProfile::ClothesCallRva);
            DWORD before = 0, ignored = 0;
            if (!::VirtualProtect(target, 1, PAGE_EXECUTE_READWRITE, &before))
                return FALSE;
            *target = 0xcc;
            const auto flushed = ::FlushInstructionCache(GetCurrentProcess(), target, 1);
            const auto restored = ::VirtualProtect(target, 1, before, &ignored);
            partialInjected = flushed && restored ? 1 : 0;
        }
        return result;
    }
    void Original(void* receiver, float delta)
    {
        savedOriginal(receiver, delta);
        if (originalAction)
        {
            Check(!timingCallbackGeneration && !frameCallbackGeneration && !frameCallbackReceiver && CheckTimingOwner() != ClockResult::Ok &&
                      CheckApplicationTimingOwner() != ClockResult::Ok,
                  "natural originals keep general AfterApp authority suspended");
            originalAction();
        }
    }
}
extern "C"
{
    alignas(16) ClockMoveAnimHookOwnerTest::Snapshot g_MoveAnimBridgeSeed, g_MoveAnimBridgeAfter, g_MoveAnimBridgeConsumer;
    void*          g_MoveAnimBridgeTarget = nullptr;
    std::uintptr_t g_MoveAnimBridgeBeforeRsp = 0, g_MoveAnimBridgeAfterRsp = 0, g_MoveAnimBridgeConsumerRsp = 0;
    std::uint32_t  g_MoveAnimBridgeConsumerCalls = 0, g_MoveAnimBridgeClobberMxcsr = 0x1f81;
    void           ExecuteMoveAnimHookFixture();
}
namespace PlayerMoveAnimHookOwnerFixture
{
    namespace HP = NativePlayerMoveAnimHookProfile;
    namespace RF = NativePlayerMoveAnimHookRangeFixture;
    namespace P = NativePlayerMoveAnimReapplyProfile;
    namespace R = NativePedReferenceProfile;
    namespace Ped = NativePedPoolProfile;
    namespace Player = NativePlayerInfoProfile;
    using Controller = NativePlayerMoveAnimHookController;
    using PlayerMoveAnimReapplyOwnerFixture::Store;
    struct OwnerSources
    {
        PrivateAnimationAssociationOwnerFixture::SourceBank source;
        std::uintptr_t                                      childrenAllocation = 0, children = 0, groups = 0;
        OwnerSources(ClockImageFixture& fixture, WorldGroundOwnerFixture::Writes& writes) : source(fixture, writes)
        {
            groups = source.groups;
            childrenAllocation = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 8 + 6 * 32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(childrenAllocation != 0, "own six typed movement hierarchy source rows");
            children = childrenAllocation + 8;
            Store(childrenAllocation, std::uint64_t{6});
            for (std::uint32_t id = 0; id != 6; ++id)
            {
                const auto row = children + id * 32;
                std::memcpy(reinterpret_cast<void*>(row), reinterpret_cast<void*>(source.children), 32);
                Store(row + NativeAnimationStaticAssociationProfile::IdOffset, static_cast<std::int16_t>(id));
            }
            // All physical rows retain the same actual block/hierarchy owner;
            // their logical group indices cover the real player pose choices.
            for (std::uint32_t group = 0; group != 71; ++group)
            {
                const auto row = groups + group * 32;
                Store(row, fixture.image + NativeAnimationBlockProfile::BankRva);
                Store(row + 8, children);
                Store(row + 0x10, std::uint32_t{6});
                Store(row + 0x14, std::uint32_t{0});
                Store(row + 0x18, group);
            }
            writes.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{71});
        }
        ~OwnerSources()
        {
            if (childrenAllocation)
                VirtualFree(reinterpret_cast<void*>(childrenAllocation), 0, MEM_RELEASE);
        }
    };
    std::uint32_t Invoke(std::uintptr_t stub, std::uintptr_t ped, std::uint32_t requested, bool clothes)
    {
#if defined(MTA_DE_PLAYER_MOVEANIM_HOOK_OWNER_FIXTURE)
        auto& seed = g_MoveAnimBridgeSeed;
        seed = {};
        _fxsave64(seed.fx.data());
        for (std::size_t reg = 0; reg != seed.gpr.size(); ++reg)
            seed.gpr[reg] = 0x1234000000000000ull + reg;
        seed.gpr[0] = seed.gpr[1] = requested;
        seed.gpr[3] = seed.gpr[6] = ped;
        seed.flags = 0x202;
        g_MoveAnimBridgeTarget = reinterpret_cast<void*>(stub);
        ExecuteMoveAnimHookFixture();
        Check(g_MoveAnimBridgeBeforeRsp == g_MoveAnimBridgeAfterRsp, "actual Runtime bridge restores its native caller stack");
        return clothes ? ClockMoveAnimHookOwnerTest::consumerGroup : static_cast<std::uint32_t>(g_MoveAnimBridgeAfter.gpr[0]);
#else
        (void)stub;
        (void)ped;
        (void)clothes;
        return requested;
#endif
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
        OwnerSources sources(fixture, writes);
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
        apply(HP::ProcessContextRva, HP::ProcessContext.data(), HP::ProcessContext.size(), PAGE_EXECUTE_READ);
        apply(HP::ClothesConsumerRva, HP::ClothesConsumer.data(), HP::ClothesConsumer.size(), PAGE_EXECUTE_READ);
        apply(NativePedRebuildProfile::EntryRva, NativePedRebuildProfile::Entry.data(), NativePedRebuildProfile::Entry.size(), PAGE_EXECUTE_READ);
        for (const auto& pin : RF::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_READONLY);
        apply(RF::ExceptionDirectoryRva, RF::Entries, sizeof(RF::Entries), PAGE_READONLY);
        IMAGE_DOS_HEADER dos{};
        std::memcpy(&dos, reinterpret_cast<void*>(fixture.image), sizeof(dos));
        IMAGE_NT_HEADERS64 nt{};
        std::memcpy(&nt, reinterpret_cast<void*>(fixture.image + dos.e_lfanew), sizeof(nt));
        const IMAGE_DATA_DIRECTORY exceptionDirectory{RF::ExceptionDirectoryRva, static_cast<DWORD>(sizeof(RF::Entries))};
        apply(static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory) +
                  IMAGE_DIRECTORY_ENTRY_EXCEPTION * sizeof(IMAGE_DATA_DIRECTORY),
              &exceptionDirectory, sizeof(exceptionDirectory), PAGE_READONLY);
        ClockMoveAnimHookOwnerTest::rangeImage = fixture.image;
        ClockMoveAnimHookOwnerTest::rangeImageBytes = nt.OptionalHeader.SizeOfImage;
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
        // The reapply fixture needed no CPlayerInfo/model provider. Explicit
        // Rebuild consumes those real owners, so reuse its established setup
        // rather than weakening a production admission check.
        for (const auto& pin : NativePlayerInfoProfile::Code)
            apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativePlayerInfoExtraProfile::Code)
            apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        writes.Global(Player::PlayerIndexRva, static_cast<unsigned char>(0));
        writes.Global(Player::PlayerVtableRva + 0x50, fixture.image + Player::PlayerControlRva, PAGE_READONLY);
        alignas(std::uintptr_t) std::array<unsigned char, NativePedRebuildProfile::ClothesBytes> rebuildClothes{};
        writes.Global(NativePedRebuildProfile::ClothesSlotRva, reinterpret_cast<std::uintptr_t>(rebuildClothes.data()));
        namespace Request = NativeStreamingRequestProfile;
        namespace Ref = NativeModelReferenceProfile;
        namespace Objects = NativeStartupProvidersProfile;
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderCountOffset + sizeof(std::int32_t)> provider{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderRowStride>                          providerRows{};
        alignas(std::uintptr_t) std::array<unsigned char, 12 * Objects::objectItemBytes>                       liveItems{};
        std::array<std::uintptr_t, 1>                                                                          liveChunks{};
        struct alignas(std::uintptr_t) ModelRefRow
        {
            std::uint32_t key = 0, count = 7;
            std::int32_t  next = -1, bucket = 0;
        };
        std::array<ModelRefRow, 4> modelReferences{};
        const auto                 put = [](unsigned char* base, std::size_t offset, const auto& value) { std::memcpy(base + offset, &value, sizeof(value)); };
        const auto                 providerAddress = reinterpret_cast<std::uintptr_t>(provider.data());
        put(provider.data(), 0, fixture.image + Request::RootVtableRva);
        put(provider.data(), NativeStartupWorldProfile::objectIndexOffset, std::int32_t{11});
        put(provider.data(), NativeStartupWorldProfile::objectClassOffset, graph.Class(0));
        put(provider.data(), Request::ProviderRowsOffset, reinterpret_cast<std::uintptr_t>(providerRows.data()));
        put(provider.data(), Request::ProviderCountOffset, std::int32_t{1});
        auto* rootBytes = reinterpret_cast<unsigned char*>(fixture.frameOwner.data());
        put(rootBytes, Request::ProviderOffset, providerAddress);
        std::memcpy(liveItems.data(), graph.items.data(), graph.items.size());
        put(liveItems.data(), 11 * Objects::objectItemBytes, providerAddress);
        put(liveItems.data(), 11 * Objects::objectItemBytes + 0x10, std::uint32_t{111});
        liveChunks[0] = reinterpret_cast<std::uintptr_t>(liveItems.data());
        writes.Global(Objects::objectChunksRva, reinterpret_cast<std::uintptr_t>(liveChunks.data()));
        writes.Global(Objects::objectCountRva, std::int32_t{12});
        auto* referenceMap = rootBytes + Ref::MapOffset;
        put(referenceMap, 0, reinterpret_cast<std::uintptr_t>(modelReferences.data()));
        put(referenceMap, 8, std::int32_t{1});
        put(referenceMap, 0xc, std::int32_t{4});
        put(referenceMap, 0x10, std::uint32_t{1});
        put(referenceMap, 0x28, std::int32_t{1});
        put(referenceMap, 0x2c, std::int32_t{128});
        put(referenceMap, 0x30, std::int32_t{-1});
        put(referenceMap, 0x34, std::int32_t{0});
        put(referenceMap, 0x38, std::int32_t{0});
        put(referenceMap, 0x40, std::uintptr_t{0});
        put(referenceMap, 0x48, std::int32_t{1});
        put(model.data(), 0, fixture.image + 0x4301000);
        for (const auto& pin : NativeModelBankProfile::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        writes.Global(0x4301000 + Request::AnimationMethodSlot, fixture.image + NativePedRebuildFixture::RenderEventRva, PAGE_READONLY);

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
        struct OldProvider final : NativePedMoveAnimAccess
        {
        } absent;
        Check(absent.EnablePlayerMoveAnimHooks() == ClockResult::NotInitialized &&
                  absent.PublishPlayerMoveAnimOverride({}, 55) == ClockResult::NotInitialized &&
                  absent.WithdrawPlayerMoveAnimOverride({}) == ClockResult::NotInitialized && absent.RetirePlayerMoveAnimHooks() == ClockResult::NotInitialized,
              "old internal providers do not invent hook ownership or a complete setter");
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
                std::printf("Hook lifecycle phase %s elapsed %llu ms\n", label, static_cast<unsigned long long>(GetTickCount64() - start));
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
        };
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        pulse("acquire", [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); });
        Check(result == ClockResult::Ok && runtime.pedReferences.IsRetained(), "real private weak-cell owner issues the exact current player token");
        const auto fixedToken = token;

        Controller::Receipt                                              installed;
        std::array<unsigned char, HP::ProcessContextBytes>               oldProcess{};
        std::array<unsigned char, NativePedRebuildProfile::Entry.size()> oldRebuild{};
        ClockMoveAnimHookOwnerTest::failDelete = ClockMoveAnimHookOwnerTest::deleteCalls = 0;
        ClockMoveAnimHookOwnerTest::consumerCalls = 0;
        ClockMoveAnimHookOwnerTest::savedOriginal = runtime.originalFrame;
        runtime.originalFrame = &ClockMoveAnimHookOwnerTest::Original;
        if (scenario == 4)
        {
            ClockMoveAnimHookOwnerTest::injectPartial = 1;
            pulse("partial-install",
                  [&](NativeGameTimingAccess& access)
                  {
                      Check(access.EnablePlayerMoveAnimHooks() == ClockResult::BindingChanged && runtime.playerMoveAnimHooks.Read(installed),
                            "second source loss refuses after the actual first-site write prefix");
                      Check(ClockMoveAnimHookOwnerTest::partialInjected == 1 && ClockMoveAnimHookOwnerTest::partialParked == 1 &&
                                installed.sites[0].installed && installed.sites[0].verified && !installed.sites[1].writeEntered && installed.uncertain &&
                                runtime.playerMoveAnimHooks.RecoveryPending(),
                            "an acknowledged first patch and untouched foreign second source retain first Unknown");
                  });
            // A later external rewrite cannot retroactively acknowledge the
            // lost prefix or authorize the owner to delete callback metadata.
            fixture.Patch(HP::ClothesCallRva, HP::ClothesCallBytes.data(), HP::ClothesCallBytes.size());
            runtime.originalFrame = ClockMoveAnimHookOwnerTest::savedOriginal;
            ClockMoveAnimHookOwnerTest::savedOriginal = nullptr;
            const auto deletes = ClockMoveAnimHookOwnerTest::deleteCalls;
            Check(!service.End() && !service.End() && service.IsRetained() && runtime.playerMoveAnimHooks.IsRetained() &&
                      ClockMoveAnimHookOwnerTest::deleteCalls == deletes,
                  "restoring the foreign source later does not infer safe detach/unwind deletion");
            MEMORY_BASIC_INFORMATION page{};
            Check(VirtualQuery(reinterpret_cast<void*>(installed.allocation), &page, sizeof(page)) == sizeof(page) && page.State == MEM_COMMIT,
                  "the partial-install callback allocation is still physically retained");
            engine.Stop();
            ClockReaderTestHost::image = previousImage;
            ClockPedReferenceOwnerTest::image = previousReferenceImage;
            ClockMoveAnimHookOwnerTest::rangeImage = 0;
            ClockMoveAnimHookOwnerTest::rangeImageBytes = 0;
            std::printf("Production player move animation hook partial prefix completed\n");
            std::printf("Production player move animation hook retirement completed\n");
            std::printf("Production player move animation hook refusals completed\n");
            std::printf("Production player move animation hook owner completed\n");
            return;
        }
        pulse("enable",
              [&](NativeGameTimingAccess& access)
              {
                  result = access.EnablePlayerMoveAnimHooks();
                  Check(result == ClockResult::Ok && runtime.playerMoveAnimHooks.Ready() && runtime.playerMoveAnimHooks.Read(installed),
                        "real AfterApp owner installs both exact hooks through actual peer hold");
                  Check(installed.sites[0].installed && installed.sites[1].installed && installed.unwindRegistered && installed.codeRx &&
                            !installed.uncertain && installed.inFlight == 0,
                        "both native sites and unwind/RX resource acknowledge before decisions open");
                  std::array<NativeFunctionRange, 4> ranges{};
                  Check(runtime.playerMoveAnimHooks.Exclusions(ranges) &&
                            std::any_of(ranges.begin(), ranges.end(),
                                        [&](const auto& range) { return range.Contains(fixture.image + HP::ProcessCompareRva + 5); }) &&
                            std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.Contains(fixture.image + HP::ClothesCallRva); }) &&
                            std::any_of(ranges.begin(), ranges.end(),
                                        [&](const auto& range) { return range.Contains(installed.allocation + installed.allocationBytes - 1); }),
                        "real chain decoding excludes call continuations and the entire code allocation during retirement");
                  Check(access.RetirePlayerMoveAnimHooks() != ClockResult::Ok && runtime.playerMoveAnimHooks.Ready(),
                        "an engine callback cannot retire producer-owned hook resources");
                  std::memcpy(oldProcess.data(), reinterpret_cast<void*>(fixture.image + HP::ProcessContextRva), oldProcess.size());
                  std::memcpy(oldRebuild.data(), reinterpret_cast<void*>(fixture.image + NativePedRebuildProfile::EntryRva), oldRebuild.size());
                  if (scenario == 0)
                      CheckMoveAnimHookOverlayBuffers(
                          fixture.image, oldProcess.data(), oldRebuild.data(), [&](std::uintptr_t image, const unsigned char* bytes, std::size_t size)
                          { return runtime.playerMoveAnimHooks.MatchesProcess(image, bytes, size); },
                          [&](std::uintptr_t image, const unsigned char* bytes, std::size_t size)
                          { return runtime.playerMoveAnimHooks.MatchesRebuild(image, bytes, size); });
              });
        std::printf("Production player move animation hook actual runtime completed\n");
        pulse("publish",
              [&](NativeGameTimingAccess& access)
              {
                  Check(access.PublishPlayerMoveAnimOverride(fixedToken, 55) == ClockResult::Ok && runtime.playerMoveAnimHooks.HasOverride(fixedToken),
                        "same real registered token publishes MTA's custom physique snapshot");
                  std::int32_t raw = -1;
                  Check(access.ReadPedMoveAnim(fixedToken, raw) == ClockResult::Ok && raw == 0,
                        "CPU override publication does not invent an immediate native group write");
              });
        Check(Invoke(installed.sites[0].stub, ped, 54, false) == 54, "producer/wrong-thread execution of an owned stub preserves the native request");
        if (scenario == 0)
        {
            ClockMoveAnimHookOwnerTest::originalAction = [&]
            {
                Check(Invoke(installed.sites[0].stub, ped, 54, false) == 55 && Invoke(installed.sites[0].stub, ped, 70, false) == 70,
                      "natural original uses the same custom physique and preserves the native jetpack request");
                const auto before = ClockMoveAnimHookOwnerTest::consumerCalls;
                Check(Invoke(installed.sites[1].stub, ped, 54, true) == 55 && ClockMoveAnimHookOwnerTest::consumerCalls == before + 1,
                      "natural clothes decision calls the typed original consumer once");
            };
            engine.Pulse(service, generation);
            ClockMoveAnimHookOwnerTest::originalAction = {};
        }
        if (scenario == 5)
        {
            pulse("explicit-rebuild",
                  [&](NativeGameTimingAccess& access)
                  {
                      Store(state.Container(), state.Head());
                      ClockPedRebuildOwnerTest::scenario = 0;
                      ClockPedRebuildOwnerTest::callerToken = nullptr;
                      ClockPedRebuildOwnerTest::reenter = nullptr;
                      ClockMoveAnimHookOwnerTest::rebuildAction = [&]
                      {
                          Check(!timingCallbackGeneration && !frameCallbackGeneration && CheckTimingOwner() != ClockResult::Ok,
                                "explicit whole Rebuild suspends general writer admission until its finally");
                          Check(Invoke(installed.sites[1].stub, ped, 54, true) == 55, "explicit MTA Rebuild receives only its same-token clothes hook scope");
                          Check(Invoke(installed.sites[0].stub, ped, 54, false) == 54, "a named clothes scope does not authorize unrelated process decisions");
                      };
                      ClockPedRebuildOwnerTest::afterNative = +[](void*) noexcept
                      {
                          if (ClockMoveAnimHookOwnerTest::rebuildAction)
                              ClockMoveAnimHookOwnerTest::rebuildAction();
                      };
                      const auto calls = ClockPedRebuildOwnerTest::calls;
                      const auto rebuilt = access.RebuildPed(fixedToken);
                      if (rebuilt != ClockResult::Ok || ClockPedRebuildOwnerTest::calls != calls + 1)
                          std::fprintf(stderr, "Hook rebuild diagnostic result=%u calls=%u/%u owner=%u infoBound=%u generation=%llu\n",
                                       static_cast<unsigned>(rebuilt), ClockPedRebuildOwnerTest::calls, calls, static_cast<unsigned>(CheckTimingOwner()),
                                       static_cast<unsigned>(runtime.playerInfo.IsBound()), static_cast<unsigned long long>(runtime.playerInfoGeneration));
                      ClockPedRebuildOwnerTest::afterNative = nullptr;
                      ClockMoveAnimHookOwnerTest::rebuildAction = {};
                      Store(state.Container(), std::uintptr_t{0});
                      Check(rebuilt == ClockResult::Ok && ClockPedRebuildOwnerTest::calls == calls + 1 && CheckTimingOwner() == ClockResult::Ok,
                            "owned overlay admits one original Rebuild and restores the exact AfterApp scope");
                  });
            std::printf("Production player move animation hook explicit rebuild completed\n");
        }
        if (scenario == 0)
        {
            pulse("update", [&](NativeGameTimingAccess& access)
                  { Check(access.PublishPlayerMoveAnimOverride(fixedToken, 56) == ClockResult::Ok, "replace only the same token's CPU override intent"); });
            ClockMoveAnimHookOwnerTest::originalAction = [&]
            {
                const auto processChoice = Invoke(installed.sites[0].stub, ped, 60, false);
                const auto clothesChoice = Invoke(installed.sites[1].stub, ped, 54, true);
                if (processChoice != 62 || clothesChoice != 56)
                    std::fprintf(stderr, "Hook choice diagnostic process=%u clothes=%u scope=%u group56=%u group62=%u ready=%u recovery=%u\n", processChoice,
                                 clothesChoice, static_cast<unsigned>(CheckMoveAnimOriginalOwner()), static_cast<unsigned>(QualifyMoveAnimHookGroup(56)),
                                 static_cast<unsigned>(QualifyMoveAnimHookGroup(62)), static_cast<unsigned>(runtime.playerMoveAnimHooks.Ready()),
                                 static_cast<unsigned>(runtime.playerMoveAnimHooks.RecoveryPending()));
                Check(processChoice == 62 && clothesChoice == 56, "both real original-scope hooks reuse MTA's distinct process/clothes physique rules");
            };
            engine.Pulse(service, generation);
            ClockMoveAnimHookOwnerTest::originalAction = {};
            std::printf("Production player move animation hook same token completed\n");
        }
        if (scenario == 2)
        {
            pulse("token-loss",
                  [&](NativeGameTimingAccess& access)
                  {
                      Check(access.ReleasePedReference(fixedToken) == ClockResult::Ok && !runtime.playerMoveAnimHooks.HasOverride(fixedToken),
                            "withdrawal precedes actual registered-cell retirement");
                      Check(access.PublishPlayerMoveAnimOverride(fixedToken, 55) != ClockResult::Ok,
                            "retired full token cannot republish a snapshot for the same native address");
                  });
            ClockMoveAnimHookOwnerTest::originalAction = [&]
            { Check(Invoke(installed.sites[0].stub, ped, 54, false) == 54, "retired token makes the native hook pass through"); };
            engine.Pulse(service, generation);
            ClockMoveAnimHookOwnerTest::originalAction = {};
        }
        else
        {
            pulse("withdraw",
                  [&](NativeGameTimingAccess& access)
                  {
                      Check(access.WithdrawPlayerMoveAnimOverride(fixedToken) == ClockResult::Ok && !runtime.playerMoveAnimHooks.HasOverride(fixedToken) &&
                                access.ReleasePedReference(fixedToken) == ClockResult::Ok,
                            "explicit snapshot withdrawal and weak-cell release preserve their actual owners");
                  });
        }
        std::printf("Production player move animation hook refusals completed\n");
        if (scenario == 1)
        {
            const unsigned char foreign = 0xcc;
            fixture.Patch(HP::ProcessCompareRva, &foreign, 1);
        }
        if (scenario == 3)
            ClockMoveAnimHookOwnerTest::failDelete = 1;
        runtime.originalFrame = ClockMoveAnimHookOwnerTest::savedOriginal;
        ClockMoveAnimHookOwnerTest::savedOriginal = nullptr;
        bool ended = service.End();
        // End retires the movement resources on the producer, but its frame
        // hook still requires the genuine engine's final restore ACK. Follow
        // the existing row-borrow drain only after healthy resource retirement;
        // unknown effects must retain their owner and must never be replayed.
        if (!ended && scenario != 1 && scenario != 3 && !runtime.playerMoveAnimHooks.IsRetained() && !runtime.playerMoveAnimHooks.RecoveryPending())
        {
            engine.Pulse(service, generation, true);
            ended = service.End();
        }
        if (!ended && scenario != 1 && scenario != 3)
        {
            NativePlayerMoveAnimHookController::Receipt endReceipt;
            const auto                                  readEnd = runtime.playerMoveAnimHooks.Read(endReceipt);
            std::fprintf(stderr,
                         "Hook End diagnostic read=%u phase=%u result=%u unknown=%u sites=%u,%u unwind=%u,%u free=%u,%u inFlight=%u runtimeFlight=%u "
                         "retained=%u,%u binding=%u,%u\n",
                         static_cast<unsigned>(readEnd), static_cast<unsigned>(endReceipt.phase), static_cast<unsigned>(endReceipt.result),
                         static_cast<unsigned>(endReceipt.uncertain), static_cast<unsigned>(endReceipt.sites[0].installed),
                         static_cast<unsigned>(endReceipt.sites[1].installed), static_cast<unsigned>(endReceipt.unwindRegistered),
                         static_cast<unsigned>(endReceipt.unwindDeleted), static_cast<unsigned>(endReceipt.freeEntered),
                         static_cast<unsigned>(endReceipt.freed), endReceipt.inFlight, runtime.inFlight.load(),
                         static_cast<unsigned>(runtime.playerMoveAnimHooks.IsRetained()), static_cast<unsigned>(NativeLifetimeRetained()),
                         static_cast<unsigned>(CheckBinding(false, true)), static_cast<unsigned>(CheckTimingBinding()));
            std::fprintf(stderr, "Hook End runtime captured=%u restored=%u timing=%u,%u peers=%u,%u,%u api=%u hook=%u admission=%llu,%llu,%llu\n",
                         static_cast<unsigned>(runtime.captured.load()), static_cast<unsigned>(runtime.restored.load()),
                         static_cast<unsigned>(runtime.timingDirty.load()), static_cast<unsigned>(runtime.timingRecovery.load()),
                         static_cast<unsigned>(runtime.peersRetained.load()), static_cast<unsigned>(runtime.playerMoveAnimHookPeers.Held()),
                         static_cast<unsigned>(runtime.playerMoveAnimHookPeers.PresentationHeld()), static_cast<unsigned>(runtime.apiBusy.load()),
                         static_cast<unsigned>(runtime.frameHooked.load()), static_cast<unsigned long long>(runtime.generation.load()),
                         static_cast<unsigned long long>(runtime.playerMoveAnimHookGeneration.load()),
                         static_cast<unsigned long long>(runtime.admissionGeneration.load()));
        }
        if (scenario == 1 || scenario == 3)
        {
            Check(!ended && service.IsRetained() && runtime.playerMoveAnimHooks.IsRetained() && runtime.playerMoveAnimHooks.RecoveryPending(),
                  "foreign native bytes or unacknowledged unwind deletion retains the actual runtime and executable owner");
            const auto deleteCalls = ClockMoveAnimHookOwnerTest::deleteCalls;
            ClockMoveAnimHookOwnerTest::failDelete = 0;
            Check(!service.End() && ClockMoveAnimHookOwnerTest::deleteCalls == deleteCalls,
                  "an entered unknown resource effect is not replayed by repeated End");
            MEMORY_BASIC_INFORMATION page{};
            Check(VirtualQuery(reinterpret_cast<void*>(installed.allocation), &page, sizeof(page)) == sizeof(page) && page.State == MEM_COMMIT,
                  "unknown callback/unwind resources remain physically allocated");
        }
        else
        {
            Check(ended && !service.IsRetained() && !runtime.playerMoveAnimHooks.IsRetained(),
                  "ordinary producer End retires both hooks and their unwind/code owner");
            CheckMoveAnimHookOverlayWithdrawn(
                fixture.image, oldProcess.data(), oldRebuild.data(), [&](std::uintptr_t image, const unsigned char* bytes, std::size_t size)
                { return runtime.playerMoveAnimHooks.MatchesProcess(image, bytes, size); },
                [&](std::uintptr_t image, const unsigned char* bytes, std::size_t size)
                { return runtime.playerMoveAnimHooks.MatchesRebuild(image, bytes, size); });
            Check(std::memcmp(reinterpret_cast<void*>(fixture.image + HP::ProcessContextRva), HP::ProcessContext.data(), HP::ProcessContext.size()) == 0 &&
                      std::memcmp(reinterpret_cast<void*>(fixture.image + NativePedRebuildProfile::EntryRva), NativePedRebuildProfile::Entry.data(),
                                  NativePedRebuildProfile::Entry.size()) == 0,
                  "acknowledged retirement restores each entire stock context without other native edits");
        }
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        ClockPedReferenceOwnerTest::image = previousReferenceImage;
        ClockMoveAnimHookOwnerTest::rangeImage = 0;
        ClockMoveAnimHookOwnerTest::rangeImageBytes = 0;
        std::printf("Production player move animation hook retirement completed\n");
        std::printf("Production player move animation hook owner completed\n");
    }
}

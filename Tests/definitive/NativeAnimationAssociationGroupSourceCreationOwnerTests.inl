/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK/runtime source composition with closed native owners
 ****************************************************************************/
#include "../../Client/game_de/NativeAnimationAssociationGroupCreationController.h"
#include "../../Client/game_de/NativeModelHashProfile.h"
namespace ClockGroupSourceCreationOwnerTest
{
    namespace F = NativeStaticAssociationInitializeFixture;
    namespace C = NativeAnimationAssociationGroupCreation;
    namespace T = NativeTemporaryModelClumpCalls;
    namespace P = NativeTemporaryModelClumpProfile;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    struct Context
    {
        std::uintptr_t        image = 0, parent = 0, model = 0, child = 0;
        F::State*             setup = nullptr;
        unsigned              mode = 0, creates = 0, setups = 0, prepares = 0, originals = 0, retires = 0, oldRetires = 0, words = 0, index = 0;
        bool                  gateBusy = false, lifetimeBusy = false;
        std::vector<unsigned> events;
        C::Invocations        invocations;
    };
    Context*              current = nullptr;
    const C::Invocations* Supplier() noexcept
    {
        return current ? &current->invocations : nullptr;
    }
    ClockResult Allocate(Host::Calls::MallocFunction function, std::uintptr_t receiver, std::size_t size, std::uint32_t alignment, std::uintptr_t& output,
                         std::uint32_t& exception) noexcept
    {
        current->events.push_back(4);
        return Host::Malloc(function, receiver, size, alignment, output, exception);
    }
    ClockResult RetireGroup(NativeAnimationAssociationGroupReplacementCalls::RetireFunction function, std::uintptr_t parent, std::uint32_t& exception) noexcept
    {
        auto& c = *current;
        ++c.oldRetires;
        c.events.push_back(0);
        Check(parent == c.parent, "original old-child retirement consumes the exact source group");
        return ClockGroupReplacementOwnerTest::Retire(function, parent, exception);
    }
    ClockResult Qword(std::uintptr_t address, std::uint64_t before, std::uint64_t target, std::uint64_t& observed, bool& accepted,
                      std::uint32_t& exception) noexcept
    {
        auto&      c = *current;
        const bool child = address == c.parent + 8;
        Check(child || address == c.parent, "source publication touches only exact block/child fields");
        c.events.push_back(child ? 2 : 1);
        if (child && c.mode == 1)
        {
            observed = before ^ 0x10000;
            accepted = false;
            return ClockResult::Ok;
        }
        const auto result = AssociationGroupReplacementDetail::Qword(address, before, target, observed, accepted, exception);
        if (result == ClockResult::Ok && child && accepted)
            c.child = target;
        return result;
    }
    ClockResult Dword(std::uintptr_t address, std::uint32_t before, std::uint32_t target, std::uint32_t& observed, bool& accepted,
                      std::uint32_t& exception) noexcept
    {
        Check(address == current->parent + 0x10 && target == 3, "source publishes full count including missing-model rows");
        current->events.push_back(3);
        return AssociationGroupReplacementDetail::Dword(address, before, target, observed, accepted, exception);
    }
    ClockResult Word(std::uintptr_t address, std::uint16_t before, std::uint16_t target, std::uint16_t& observed, bool& accepted,
                     std::uint32_t& exception) noexcept
    {
        auto&      c = *current;
        const auto index = (address - c.child) / 32;
        const auto offset = (address - c.child) % 32;
        Check(index == c.index && (offset == 0x0a || offset == 0x0c), "source WORD stores target only the just-initialized physical row");
        Check(target == (offset == 0x0a ? static_cast<std::uint16_t>(17 + index) : std::uint16_t{42}),
              "source ID/group stores preserve low WORD semantics of current full DWORD parent fields");
        c.events.push_back(40 + static_cast<unsigned>(index) * 2 + (offset == 0x0c));
        ++c.words;
        const auto result = AnimationStaticAssociationWriteDetail::ExchangeWord(address, before, target, observed, accepted, exception);
        if (result == ClockResult::Ok && c.mode == 4)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        return result;
    }
    ClockResult Create(T::CreateFunction function, std::uintptr_t model, std::uintptr_t opaque, std::uint32_t zero, std::int32_t minusOne,
                       std::uintptr_t stackZero, std::uintptr_t& output, std::uint32_t&) noexcept
    {
        auto& c = *current;
        c.index = (c.creates % 2) * 2;
        ++c.creates;
        c.events.push_back(10 + c.index);
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + P::CreateRva && model == c.model && opaque == c.image + P::OpaqueDefaultRva && !zero &&
                  minusOne == -1 && !stackZero,
              "source reaches genuine model-vslot create tuple without guessed model/default arguments");
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        c.gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        c.lifetimeBusy = runtime.groupCreations.IsRetained() && NativeLifetimeRetained();
        output = c.setup->Clump();
        return ClockResult::Ok;
    }
    ClockResult Setup(T::SetupFunction function, std::uintptr_t clump, std::uintptr_t opaque, std::uint32_t zero, std::uint32_t secondZero,
                      std::uint32_t&) noexcept
    {
        auto& c = *current;
        ++c.setups;
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + P::SetupRva && clump == c.setup->Clump() && opaque == c.image + P::OpaqueDefaultRva &&
                  !zero && !secondZero,
              "source temporary setup keeps exact class-qualified native arguments");
        return ClockResult::Ok;
    }
    ClockResult Prepare(T::PrepareFunction function, std::uintptr_t clump, std::uintptr_t model, std::uint32_t&) noexcept
    {
        auto& c = *current;
        ++c.prepares;
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + P::PrepareRva && clump == c.setup->Clump() && model == c.model,
              "source explicit clump preparation uses the actual returned actor and selected model");
        F::Store(c.setup->container.data(), 0x20, std::uintptr_t{0});
        return ClockResult::Ok;
    }
    ClockResult Retire(T::RetireFunction function, std::uintptr_t clump, bool flag, bool& accepted, std::uint32_t&) noexcept
    {
        auto& c = *current;
        ++c.retires;
        c.events.push_back(30 + c.index);
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + P::RetireRva && clump == c.setup->Clump() && !flag,
              "only verified source initialization reaches accepted deferred actor retirement");
        accepted = true;
        return ClockResult::Ok;
    }
    ClockResult Initialize(NativeStaticAssociationInitializeCalls::NativeFunction function, std::uintptr_t row, std::uintptr_t clump, std::uintptr_t hierarchy,
                           std::uint32_t& exception) noexcept
    {
        auto& c = *current;
        ++c.originals;
        c.events.push_back(20 + c.index);
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + NativeStaticAssociationInitializeProfile::EntryRva && row == c.child + c.index * 32 &&
                  clump == c.setup->Clump() &&
                  hierarchy == c.image + NativeAnimationBlockHierarchyProfile::BankRva + (1 + c.index) * NativeAnimationBlockHierarchyProfile::RowBytes &&
                  !F::Load<std::uintptr_t>(reinterpret_cast<void*>(row), 0x10) && !F::Load<std::uintptr_t>(reinterpret_cast<void*>(row), 0x18),
              "actual joint source entry owns first H/node stores on its causal published row; no prebind");
        NativeModelReferenceHeap::Value heap;
        Check(NativeModelReferenceHeap::Capture(c.image, heap) == ClockResult::Ok, "source final-call fixture captures the actual heap receiver");
        const auto     entry = heap.current == heap.authority ? NativeCustomAnimSequenceStorageProfile::AuthorityMallocRva
                                                              : NativeCustomAnimSequenceStorageProfile::ProxyMallocRva;
        std::uintptr_t nodes = 0;
        Check(Host::Malloc(reinterpret_cast<Host::Calls::MallocFunction>(c.image + entry), heap.current, 16, 0, nodes, exception) == ClockResult::Ok && nodes,
              "source node effect fixture has a real task-owned direct allocator base");
        const std::array<std::uintptr_t, 2> map{c.setup->Sequence(0), c.setup->Sequence(1)};
        std::memcpy(reinterpret_cast<void*>(nodes), map.data(), sizeof(map));
        F::Store(reinterpret_cast<void*>(row), 8, std::uint16_t{2});
        F::Store(reinterpret_cast<void*>(row), 0x10, nodes);
        F::Store(reinterpret_cast<void*>(row), 0x18, hierarchy);
        // These fixed known effects are injected only at the admitted original
        // entry. They do not execute or claim FEB5F0/UE game instructions.
        if (c.mode == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (c.mode == 3)
            ClockPrivateAnimationAssociationTest::Store(c.image + NativeBorrowedGcProfile::markerRva, LONG{1});
        return ClockResult::Ok;
    }
}
namespace GroupSourceCreationOwnerFixture
{
    unsigned scenario = 0;
    void     RunPrepared(ClockImageFixture& fixture)
    {
        namespace F = NativeStaticAssociationInitializeFixture;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Request = NativeStreamingRequestProfile;
        namespace B = NativeAnimationBlockProfile;
        namespace H = NativeAnimationBlockHierarchyProfile;
        namespace G = NativeAnimationGroupArrayProfile;
        namespace Hook = ClockGroupSourceCreationOwnerTest;
        using Setup = NativeStaticAssociationInitializeFixture::State;
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : Ped::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &writes);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeCustomAnimSequenceStorageFixture::State heap;
        heap.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeCustomAnimSequenceStorageFixture::active = &heap;
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationBlockRefProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : G::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationStaticAssociationProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        // The actual owner reacquires and reads the physical row before native
        // entry. Seed those read leaves too; selector/retire pins alone only
        // admitted factory resolution and hid this fixture omission.
        for (const auto& pin : NativeAnimationStaticAssociationProfile::ReadCode)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&writes, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        writes.Global(NativePlayerInfoProfile::BankRva, std::uintptr_t{0});
        std::vector<unsigned char> references(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < R::ReferenceCount; ++i)
        {
            const auto next = fixture.image + R::ReferencesRva + (i + 1) * R::ReferenceBytes;
            std::memcpy(references.data() + i * R::ReferenceBytes, &next, 8);
        }
        WorldGroundOwnerFixture::Writes::Apply(&writes, R::ReferencesRva, references.data(), references.size(), PAGE_READWRITE);
        writes.Global(R::FreeHeadRva, fixture.image + R::ReferencesRva);
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        NativeStaticAssociationInitializeFixture::Store(pool.objects, 0, fixture.image + R::PedVtableRva);
        pool.objects[R::TypeOffset] = 3;
        NativeStaticAssociationInitializeFixture::Store(pool.objects, NativePedMetadataProfile::ModelOffset, std::uint16_t{0});
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderCountOffset + 8> provider{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderRowStride>       providerRows{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ModelPrefixBytes>        model{};
        alignas(std::uintptr_t) std::array<unsigned char, 12 * 24>                          baseItems{};
        std::memcpy(baseItems.data(), graph.items.data(), graph.items.size());
        const auto providerAddress = reinterpret_cast<std::uintptr_t>(provider.data());
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0, fixture.image + Request::RootVtableRva);
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0x0c, std::int32_t{11});
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0x10, graph.Class(0));
        NativeStaticAssociationInitializeFixture::Store(provider.data(), Request::ProviderRowsOffset, reinterpret_cast<std::uintptr_t>(providerRows.data()));
        NativeStaticAssociationInitializeFixture::Store(provider.data(), Request::ProviderCountOffset, std::int32_t{1});
        NativeStaticAssociationInitializeFixture::Store(root.data(), Request::ProviderOffset, providerAddress);
        NativeStaticAssociationInitializeFixture::Store(baseItems.data(), 11 * 24, providerAddress);
        NativeStaticAssociationInitializeFixture::Store(baseItems.data(), 11 * 24 + 0x10, std::uint32_t{111});
        NativeStaticAssociationInitializeFixture::Store(model.data(), 0, fixture.image + 0x4301000);
        for (const auto& pin : NativeModelBankProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        std::array<std::uintptr_t, 0x80 / 8> modelTable{};
        modelTable[Request::TypeMethodSlot / 8] = fixture.image + NativeStaticAssociationInitializeFixture::ModelTypeRva;
        modelTable[Request::AnimationMethodSlot / 8] = fixture.image + NativeStaticAssociationInitializeFixture::EventRva;
        WorldGroundOwnerFixture::Writes::Apply(&writes, 0x4301000, modelTable.data(), sizeof(modelTable), PAGE_READONLY);
        writes.Global(NativeModelBankProfile::BankRva, reinterpret_cast<std::uintptr_t>(model.data()));
        writes.Global(NativeModelLoadedProfile::ArrayRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(1));
        alignas(std::uintptr_t) std::array<unsigned char, 8 + 2 * 32> parents{};
        alignas(std::uintptr_t) std::array<unsigned char, 8 + 32>     children{};
        NativeStaticAssociationInitializeFixture::Store(parents.data(), 0, std::uint64_t{2});
        NativeStaticAssociationInitializeFixture::Store(children.data(), 0, std::uint64_t{1});
        const auto groups = reinterpret_cast<std::uintptr_t>(parents.data()) + 8;
        const auto association = reinterpret_cast<std::uintptr_t>(children.data()) + 8;
        const auto block = fixture.image + B::BankRva + B::RowBytes;
        const auto hierarchy = fixture.image + H::BankRva + H::RowBytes;
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0, block);
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 8, association);
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x10, std::uint32_t{1});
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x14, std::uint32_t{17});
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x18, std::uint32_t{1});
        writes.Global(G::GlobalRva, groups);
        writes.Global(G::CountRva, std::int32_t{2});
        writes.Global(B::CountRva, std::int32_t{2});
        writes.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{2});
        const NativeAnimationBlockName name{'b', 'a', 'n', 'k', 0};
        writes.Global(B::BankRva + B::RowBytes, name);
        writes.Global(B::BankRva + B::RowBytes + B::LoadedOffset, static_cast<unsigned char>(1));
        writes.Global(B::BankRva + B::RowBytes + B::IdOffset, std::int32_t{1});
        writes.Global(B::BankRva + B::RowBytes + B::AnimationCountOffset, std::uint32_t{1});
        Setup setup;
        setup.Reset(fixture.image, ped, association, hierarchy, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        setup.EnableSkeletonInputs();
        Check(setup.AttachRegistry(baseItems.data(), 12, graph.Class(0), &WorldGroundOwnerFixture::Writes::Apply, &writes),
              "append actual clump/class/component/skeleton members to the existing world/provider fixture registry");
        // Arrange the empty source group before Begin; every published child
        // in this fixture is subsequently issued through the real fresh API.
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 8, std::uintptr_t{0});
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x10, std::uint32_t{0});
        writes.Global(B::BankRva + B::RowBytes + B::AnimationCountOffset, std::uint32_t{3});
        writes.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{4});
        NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes, 2, 1);
        const auto missingHierarchy = fixture.image + H::BankRva + 2 * H::RowBytes;
        writes.Global(missingHierarchy - fixture.image + 8, setup.Sequence(0));
        writes.Global(missingHierarchy - fixture.image + 0x10, std::uint16_t{0});
        for (const auto& pin : NativeTemporaryModelClumpProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const unsigned char ret = 0xc3, opaque = 0x5a;
        WorldGroundOwnerFixture::Writes::Apply(&writes, NativeTemporaryModelClumpProfile::SetupRva, &ret, 1, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&writes, NativeTemporaryModelClumpProfile::OpaqueDefaultRva, &opaque, 1, PAGE_READONLY);
        writes.Global(0x4301000 + NativeTemporaryModelClumpProfile::CreateSlot, fixture.image + NativeTemporaryModelClumpProfile::CreateRva, PAGE_READONLY);
        writes.Global(NativeTemporaryModelClumpProfile::SetupClassSlotRva, reinterpret_cast<std::uintptr_t>(setup.actorClass.data()));
        NativeStaticAssociationInitializeFixture::Store(model.data(), NativeTemporaryModelClumpProfile::FlagsOffset, std::uint16_t{0});
        NativeStaticAssociationInitializeFixture::Store(setup.container.data(), 0x20, std::uintptr_t{0});
        WorldGroundOwnerFixture::Writes::Apply(&writes, NativeStaticAssociationArrayProfile::ConstructorRecipeRva,
                                               NativeStaticAssociationArrayProfile::ConstructorRecipeBytes.data(),
                                               NativeStaticAssociationArrayProfile::ConstructorRecipeBytes.size(), PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&writes, NativeStaticAssociationArrayProfile::DeletingDestructorRva,
                                               NativeStaticAssociationArrayProfile::DeletingDestructorBytes.data(),
                                               NativeStaticAssociationArrayProfile::DeletingDestructorBytes.size(), PAGE_EXECUTE_READ);
        writes.Global(NativeStaticAssociationArrayProfile::VtableRva, fixture.image + NativeStaticAssociationArrayProfile::DeletingDestructorRva,
                      PAGE_READONLY);
        // The source lookup traverses the complete bank on its middle-row
        // miss. Arrange the genuine finite bank and terminated native names.
        const NativeAnimationBlockName unusedName{'u', 'n', 'u', 's', 'e', 'd', 0};
        writes.Global(B::BankRva, unusedName);
        std::vector<std::uintptr_t> modelBank(NativeModelBankProfile::ModelCount, 0);
        modelBank[0] = reinterpret_cast<std::uintptr_t>(model.data());
        WorldGroundOwnerFixture::Writes::Apply(&writes, NativeModelBankProfile::BankRva, modelBank.data(), NativeModelBankProfile::BankBytes, PAGE_READWRITE);
        for (const auto& pin : NativeModelHashProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        F::Store(model.data(), NativeModelHashProfile::HashOffset, std::uint32_t{0x13579bdf});
        // Reset prepares skeleton data with an unnamed hierarchy. This source
        // fixture must give both found rows the same real lookup key as the model.
        writes.Global(hierarchy - fixture.image, std::uint32_t{0x13579bdf});
        writes.Global(missingHierarchy - fixture.image, std::uint32_t{0x2468ace0});
        std::array<unsigned char, H::RowBytes> finalHierarchy{};
        std::memcpy(finalHierarchy.data(), reinterpret_cast<void*>(hierarchy), finalHierarchy.size());
        writes.Global(H::BankRva + 3 * H::RowBytes, finalHierarchy);
        F::Store(reinterpret_cast<void*>(groups + 32), 0x14, std::uint32_t{0xffff0011});
        F::Store(reinterpret_cast<void*>(groups + 32), 0x18, std::uint32_t{0xabcd002a});
        const auto previousImage = ClockReaderTestHost::image, previousReferences = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "pin actual callback code while registered references and native journal may remain");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto action = [&](const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&) { body(); };
            Check(service.Renew(generation), "renew one actual source composer owner callback");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(),
                  "source composer native work has an independent real producer without extending its lease");
            fixture.invokeTimer();
            pulse.Stop();
            Check(!pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(),
                  "source composer callback preserves and restores the existing producer/engine handoff policy");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        namespace C = NativeAnimationAssociationGroupCreation;
        namespace Host = NativeCustomAnimSequenceStorageFixture;
        Hook::Context context;
        context.image = fixture.image;
        context.parent = groups + 32;
        context.model = reinterpret_cast<std::uintptr_t>(model.data());
        context.setup = &setup;
        context.mode = scenario;
        context.events.reserve(80);
        context.invocations.retireGroup = &Hook::RetireGroup;
        context.invocations.qword = &Hook::Qword;
        context.invocations.dword = &Hook::Dword;
        context.invocations.allocateArray = &Hook::Allocate;
        context.invocations.retireArray = &ClockGroupReplacementOwnerTest::DestroyFresh;
        context.invocations.freeArray = &Host::Free;
        context.invocations.temporary = {&Hook::Create, &Hook::Setup, &Hook::Prepare, &Hook::Retire};
        context.invocations.initialize = &Hook::Initialize;
        context.invocations.word = &Hook::Word;
        Hook::current = &context;
        ClockGroupReplacementOwnerTest::image = fixture.image;
        ClockGroupReplacementOwnerTest::parent = context.parent;
        ClockGroupReplacementOwnerTest::mode = 0;
        ClockGroupReplacementOwnerTest::targetsValid = true;
        ClockGroupReplacementOwnerTest::events.reserve(40);
        ClockPrivateAnimationAssociationTest::image = fixture.image;
        ClockPrivateAnimationAssociationTest::heapFailed = false;
        NativeAnimationAssociationGroupReferenceToken group;
        action(
            [&]
            {
                Check(runtime.access.ResolveAnimationAssociationGroup(generation, 1, group) == ClockResult::Ok && group.locator == context.parent,
                      "source composer starts from the actual current SDK group token");
                auto wrong = group;
                ++wrong.ownerGeneration;
                Check(runtime.access.CreateAnimationAssociationGroup(wrong, "bank", {}) != ClockResult::Ok && context.events.empty() && !heap.mallocCalls &&
                          !runtime.groupCreations.IsRetained(),
                      "wrong-generation source request refuses before child invalidation or native effects");
            });
        AnimationAssociationGroupReceiver receiver(runtime.access, group);
        CAnimBlendAssocGroup&             sdk = receiver;
        bool                              threw = false, sawPrivatePending = false;
        C::Receipt                        observed;
        action(
            [&]
            {
                try
                {
                    sdk.CreateAssociations("bank");
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                observed = runtime.animationGroupCreation;
                Check(runtime.customSequenceGeneration.load() == (observed.retained ? generation : 0),
                      "actual runtime publishes only actual pending owners into its existing cleanup epoch");
                if (scenario && scenario != 4)
                    Check(runtime.groupCreations.IsRetained() && NativeLifetimeRetained() && runtime.stopping.load(),
                          "failed entered source call keeps its actual owner alive before this native callback can perform recovery");
                if (scenario == 1)
                {
                    sawPrivatePending = observed.privateCleanupPending && !observed.nativeRecovery && !heap.freeCalls;
                    Check(sawPrivatePending && !service.End(),
                          "End cannot invent private release or detach a hook while its source callback is still in flight");
                }
            });
        ClockPrivateAnimationAssociationTest::Store(fixture.image + NativeBorrowedGcProfile::markerRva, LONG{0});
        Check(observed.nameCopied && observed.queryPrepared && observed.count == 3 && observed.group.locator == group.locator &&
                  observed.generation == generation,
              "actual SDK->NativeAccess source request preserves exact fixed diagnostic generation/name/group facts");
        if (scenario)
        {
            Check(threw && observed.result != ClockResult::Ok && observed.hadEffects && !observed.completed && runtime.stopping.load(),
                  "entered source failure stops the actual runtime and preserves its failed command facts");
            const auto originals = context.originals, creates = context.creates, retires = context.retires, frees = heap.freeCalls;
            if (scenario == 1)
            {
                Check(observed.privateCleanupPending && !observed.nativeRecovery && observed.array.allocated && observed.array.initialized &&
                          !observed.replacement.childStore.accepted && !context.child && !context.creates && sawPrivatePending,
                      "acknowledged child CAS refusal leaves only its genuine unpublished fresh array eligible for typed cleanup");
                Check(service.End() && !service.IsRetained() && !runtime.groupCreations.IsRetained() && !runtime.groupCreations.RecoveryPending() &&
                          heap.freeCalls == 1 && context.originals == originals && context.creates == creates && context.retires == retires &&
                          !runtime.customSequenceGeneration.load(),
                      "actual Runtime End drains the known private fresh32 before dependent cleanup and hook unload exactly once");
                std::puts("Production group source creation runtime private drain fixture completed");
            }
            else if (scenario == 4)
            {
                Check(observed.enteredUnknown && !observed.retained && !observed.nativeRecovery && !observed.privateCleanupPending &&
                          !runtime.groupCreations.IsRetained() && observed.initialized == 1 && !observed.idsStored &&
                          observed.lastStep.initialization.verified && observed.lastStep.temporary.retireAccepted && observed.lastStep.idStore.entered &&
                          observed.lastStep.idStore.accepted && !observed.lastStep.idStore.verified &&
                          F::Load<std::uint16_t>(reinterpret_cast<void*>(context.child), 0x0a) == 17 && context.creates == 1 && context.originals == 1 &&
                          context.retires == 1 && context.words == 1 && !heap.freeCalls,
                      "lost persistent WORD acknowledgement stops without rollback or inventing private ownership of acknowledged engine rows");
                Check(runtime.access.CreateAnimationAssociationGroup(group, "bank", {}) != ClockResult::Ok,
                      "stopped scalar command cannot replay or replace its accepted native prefix");
                (void)service.End();
                fixture.invokeTimer();
                Check(service.End() && !service.IsRetained() && !runtime.groupCreations.IsRetained() && heap.freeCalls == frees && context.words == 1 &&
                          context.creates == creates && context.originals == originals && context.retires == retires,
                      "ordinary hook teardown preserves engine-owned node/ID facts without private free or scalar replay");
                std::puts("Production group source creation scalar facts without private debt fixture completed");
            }
            else
            {
                Check(observed.nativeRecovery && !observed.privateCleanupPending && observed.lastStep.initialization.entered &&
                          observed.lastStep.initialization.rowObserved && observed.lastStep.initialization.observedCount == 2 &&
                          observed.lastStep.initialization.observedNodes && !observed.lastStep.initialization.verified && context.originals == 1 &&
                          context.retires == 0 && !context.words && context.gateBusy && context.lifetimeBusy,
                      "unknown original/post-owner result preserves observed nodes, joint/temp journals and engine-owned published rows");
                Check((scenario == 2 ? !observed.lastStep.initialization.returned : observed.lastStep.initialization.returned) && !service.End(),
                      "source keeps missing acknowledgement distinct from returned post-owner loss and prevents unload");
                Check(runtime.access.CreateAnimationAssociationGroup(group, "bank", {}) != ClockResult::Ok,
                      "stopped runtime refuses another source request before replay");
                fixture.invokeTimer();
                Check(!service.End() && runtime.groupCreations.IsRetained() && runtime.groupCreations.RecoveryPending() && heap.freeCalls == frees &&
                          context.originals == originals && context.creates == creates && context.retires == retires,
                      "actual End never retries uncertain temporary/joint calls, privately frees published children or discards opaque native debt");
                std::puts("Production group source creation retained entered facts fixture completed");
            }
        }
        else
        {
            if (threw || observed.result != ClockResult::Ok)
                std::fprintf(stderr, "Create result=%u phase=%u visited=%u initialized=%u last=%u originals=%u\n", static_cast<unsigned>(observed.result),
                             static_cast<unsigned>(observed.phase), observed.visited, observed.initialized, static_cast<unsigned>(observed.lastStep.result),
                             context.originals);
            Check(!threw && observed.result == ClockResult::Ok && observed.completed && observed.visited == 3 && observed.missingModels == 1 &&
                      observed.initialized == 2 && observed.idsStored == 2 && observed.groupsStored == 2 && !observed.retained &&
                      !runtime.groupCreations.IsRetained() && !runtime.groupCreations.RecoveryPending() && !runtime.pedReferences.IsRetained() &&
                      context.gateBusy && context.lifetimeBusy,
                  "genuine source composition preserves full count, ascending found/missing/found behavior and no fabricated ped owner");
            const std::vector<unsigned> first{1, 4, 2, 3, 10, 20, 30, 40, 41, 12, 22, 32, 44, 45};
            Check(context.events == first && context.creates == 2 && context.originals == 2 && context.retires == 2 && context.words == 4,
                  "exact source itinerary publishes block/child/fullcount before ascending temporary-init-retire-ID-group effects");
            C::StepFacts missing;
            Check(runtime.groupCreations.ReadStep(generation, observed.serial, 1, missing) == ClockResult::Ok && missing.visited && missing.missingModel &&
                      !missing.initialization.entered && !missing.temporary.createEntered && !missing.idStore.entered && !missing.groupStore.entered &&
                      F::Load<std::uintptr_t>(reinterpret_cast<void*>(context.child + 32), 0x10) == 0 &&
                      F::Load<std::uintptr_t>(reinterpret_cast<void*>(context.child + 32), 0x18) == 0 &&
                      F::Load<std::int16_t>(reinterpret_cast<void*>(context.child + 32), 0x0a) == -1 &&
                      F::Load<std::int16_t>(reinterpret_cast<void*>(context.child + 32), 0x0c) == -1 &&
                      F::Load<std::uint32_t>(reinterpret_cast<void*>(context.parent), 0x10) == 3,
                  "complete model-bank miss leaves its physical fresh row untouched and source full count unchanged");
            const auto frees = heap.freeCalls;
            context.events.clear();
            threw = false;
            action(
                [&]
                {
                    try
                    {
                        sdk.CreateAssociations("bank");
                    }
                    catch (const std::exception&)
                    {
                        threw = true;
                    }
                });
            Check(!threw && runtime.animationGroupCreation.completed && context.oldRetires == 1 && heap.freeCalls == frees + 3 && context.originals == 4 &&
                      context.creates == 4 && context.events == std::vector<unsigned>({0, 1, 4, 2, 3, 10, 20, 30, 40, 41, 12, 22, 32, 44, 45}),
                  "second actual SDK source call retires original direct node/vector children before publishing a new causal array");
            const auto creates = context.creates, originals = context.originals, oldRetires = context.oldRetires, allocations = heap.mallocCalls,
                       lastFrees = heap.freeCalls;
            threw = false;
            context.events.clear();
            action(
                [&]
                {
                    try
                    {
                        sdk.CreateAssociations("missing");
                    }
                    catch (const std::exception&)
                    {
                        threw = true;
                    }
                });
            const auto missingName = runtime.animationGroupCreation;
            Check(threw && missingName.result == ClockResult::NotInitialized && missingName.phase == C::Phase::FindBlock && missingName.hadEffects &&
                      missingName.partialAccepted && !missingName.retained && !runtime.groupCreations.IsRetained() &&
                      !runtime.groupCreations.RecoveryPending() && context.oldRetires == oldRetires + 1 && context.events == std::vector<unsigned>({0}) &&
                      heap.freeCalls == lastFrees + 3 && heap.mallocCalls == allocations && context.creates == creates && context.originals == originals &&
                      !F::Load<std::uintptr_t>(reinterpret_cast<void*>(context.parent), 8) &&
                      !F::Load<std::uint32_t>(reinterpret_cast<void*>(context.parent), 0x10),
                  "source missing-block refusal occurs after old retirement without fallback, new allocation or invented resource debt");
            (void)service.End();
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && heap.freeCalls == lastFrees + 3 && context.originals == originals,
                  "known engine-owned prefixes allow ordinary hook cleanup without scalar rollback or extra native frees");
            std::puts("Production group source creation actual SDK runtime owner fixture completed");
            std::puts("Production group source creation source order and missing model fixture completed");
        }
        Hook::current = nullptr;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        ClockPedReferenceOwnerTest::image = previousReferences;
        writes.Restore();
    }
}

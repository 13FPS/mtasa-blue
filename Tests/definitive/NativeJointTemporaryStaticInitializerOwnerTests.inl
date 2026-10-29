/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Closed temporary-clump/published-row native-entry admission
 ****************************************************************************/
#include "../../Client/game_de/NativeTemporaryStaticAssociationInitializeCalls.h"
#include "../../Client/game_de/NativeTemporaryModelClumpProfile.h"
namespace ClockJointTemporaryStaticOwnerTest
{
    namespace F = NativeStaticAssociationInitializeFixture;
    namespace T = NativeTemporaryModelClumpCalls;
    namespace J = NativeTemporaryStaticAssociationInitializeCalls;
    namespace P = NativeTemporaryModelClumpProfile;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using Journal = NativeTemporaryStaticAssociationInitialization;
    struct Context
    {
        std::uintptr_t                                        image = 0, group = 0, modelAddress = 0;
        std::uint64_t                                         generation = 0;
        F::State*                                             setup = nullptr;
        NativeModelBankReader::Value                          model{};
        NativeAnimationAssociationGroupReplacement*           published = nullptr;
        NativeCustomAnimSequenceStorage*                      storage = nullptr;
        std::array<NativeAnimationHierarchyReferenceToken, 2> hierarchy{};
        std::array<Journal, 2>*                               journals = nullptr;
        std::array<J::Receipt, 2>                             receipts{};
        std::array<std::uintptr_t, 2>                         nodes{};
        const NativeTemporaryModelClumpBorrow*                activeBorrow = nullptr;
        NativeTemporaryModelClump*                            temporary = nullptr;
        unsigned                                              mode = 0, index = 0, creates = 0, setups = 0, prepares = 0, rows = 0, retires = 0, originals = 0;
        bool                                                  zeroGuarded = false, closedTested = false, reentryTested = false;
        DWORD                                                 zeroProtection = 0;
    };
    Context*    current = nullptr;
    ClockResult Row(const NativeTemporaryModelClumpBorrow&, void*) noexcept;
    ClockResult Create(T::CreateFunction function, std::uintptr_t model, std::uintptr_t opaque, std::uint32_t zero, std::int32_t minusOne,
                       std::uintptr_t stackZero, std::uintptr_t& output, std::uint32_t&) noexcept
    {
        auto& c = *current;
        ++c.creates;
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + P::CreateRva && model == c.modelAddress && opaque == c.image + P::OpaqueDefaultRva &&
                  !zero && minusOne == -1 && !stackZero && c.temporary->GetState() == NativeTemporaryModelClump::State::CreateEntered,
              "temporary original create receives exact current model/source tuple from the issued record");
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
              "temporary original setup preserves the exact class-qualified source tuple");
        return ClockResult::Ok;
    }
    ClockResult Prepare(T::PrepareFunction function, std::uintptr_t clump, std::uintptr_t model, std::uint32_t&) noexcept
    {
        auto& c = *current;
        ++c.prepares;
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + P::PrepareRva && clump == c.setup->Clump() && model == c.modelAddress,
              "temporary explicit FFBDF0 boundary keeps the causal returned actor and model");
        F::Store(c.setup->container.data(), 0x20, std::uintptr_t{0});
        F::Store(c.setup->container.data(), 0x10, std::int32_t{c.index ? 0 : 2});
        F::Store(c.setup->container.data(), 0x988, std::int32_t{c.index ? 0 : 2});
        if (c.index)
        {
            // A zero-frame clump cannot retain the mixed fixture's bone-map
            // entry pointing at frame0. Arrange a coherent empty native map.
            std::array<unsigned char, NativeStaticAssociationInitializeProfile::MapHeaderBytes> empty{};
            F::Store(empty.data(), 0x30, std::int32_t{-1});
            std::memcpy(c.setup->container.data() + NativeStaticAssociationInitializeProfile::MapOffset, empty.data(), empty.size());
        }
        return ClockResult::Ok;
    }
    ClockResult Retire(T::RetireFunction function, std::uintptr_t clump, bool flag, bool& accepted, std::uint32_t&) noexcept
    {
        auto& c = *current;
        ++c.retires;
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + P::RetireRva && clump == c.setup->Clump() && !flag,
              "only a verified synchronous row result reaches original deferred temporary retirement");
        accepted = true;
        return ClockResult::Ok;
    }
    ClockResult Original(NativeStaticAssociationInitializeCalls::NativeFunction function, std::uintptr_t association, std::uintptr_t clump,
                         std::uintptr_t hierarchy, std::uint32_t& exception) noexcept
    {
        auto& c = *current;
        ++c.originals;
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + NativeStaticAssociationInitializeProfile::EntryRva &&
                  association == c.storage->IssuedTag() + c.index * 32 && clump == c.setup->Clump() && hierarchy == c.hierarchy[c.index].locator &&
                  F::Load<std::uintptr_t>(reinterpret_cast<void*>(association), 0x18) == 0 &&
                  F::Load<std::uintptr_t>(reinterpret_cast<void*>(association), 0x10) == 0,
              "one exact FEB5F0 boundary owns the first H/node stores; adapter never prebinds the row hierarchy");
        auto& journal = (*c.journals)[c.index];
        Check(journal.GetState() == Journal::State::Entered && c.published->IsRetained() && c.temporary->HasOutstandingResource(),
              "joint journal, actual group continuation and temporary owner remain causal during native entry");
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        Check(!entered, "joint initializer uses the same held native gate as WithTemporary");
        if (entered)
            NativeStreamingCompletionContext::Leave();
        J::Receipt nestedReceipt;
        Journal    other;
        const auto count = c.originals;
        Check(J::Initialize(c.image, journal, c.generation, *c.activeBorrow, *c.published, *c.storage, c.index, c.hierarchy[c.index], &nestedReceipt,
                            &Original) != ClockResult::Ok &&
                  J::Initialize(c.image, other, c.generation, *c.activeBorrow, *c.published, *c.storage, c.index, c.hierarchy[c.index], &nestedReceipt,
                                &Original) != ClockResult::Ok &&
                  other.CanDiscard() && c.originals == count,
              "active journal and already-claimed borrow cannot reenter even through a different empty journal");
        NativeTemporaryModelClump nestedTemp;
        T::Receipt                tempReceipt;
        Check(T::WithTemporary(c.image, nestedTemp, c.generation, c.model, &CheckTimingOwner, &Row, &c, &tempReceipt) == ClockResult::Busy &&
                  nestedTemp.CanDiscard(),
              "same-scope temporary reentry cannot fabricate a second synchronous origin");
        c.reentryTested = true;
        // This final native-call seam injects fixed known effects only. The
        // production core qualifies the real target, mixed bone/name inputs,
        // actual origins and returned map; no CPU matcher/game instructions run.
        NativeModelReferenceHeap::Value heap;
        Check(NativeModelReferenceHeap::Capture(c.image, heap) == ClockResult::Ok, "native-entry fixture receives the actual current heap receiver");
        const auto     entry = heap.current == heap.authority ? NativeCustomAnimSequenceStorageProfile::AuthorityMallocRva
                                                              : NativeCustomAnimSequenceStorageProfile::ProxyMallocRva;
        std::uintptr_t nodes = 0;
        Check(Host::Malloc(reinterpret_cast<Host::Calls::MallocFunction>(c.image + entry), heap.current, c.index ? 8 : 16, 0, nodes, exception) ==
                      ClockResult::Ok &&
                  nodes,
              "node effect fixture owns a direct allocator base rather than an aligned dynamic-association buffer");
        c.nodes[c.index] = nodes;
        if (!c.index)
        {
            const std::array<std::uintptr_t, 2> values{c.setup->Sequence(0), c.setup->Sequence(1)};
            std::memcpy(reinterpret_cast<void*>(nodes), values.data(), sizeof(values));
        }
        else
        {
            Check(VirtualProtect(reinterpret_cast<void*>(nodes), 8, PAGE_NOACCESS, &c.zeroProtection) != FALSE,
                  "guard the zero-frame returned tag so no zero-extent node dereference can succeed");
            c.zeroGuarded = true;
        }
        F::Store(reinterpret_cast<void*>(association), 8, static_cast<std::uint16_t>(c.index ? 0 : 2));
        F::Store(reinterpret_cast<void*>(association), 0x10, nodes);
        F::Store(reinterpret_cast<void*>(association), 0x18, hierarchy);
        if (c.mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (c.mode == 2)
            ClockPrivateAnimationAssociationTest::Store(c.image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (c.mode == 3)
            F::Store(reinterpret_cast<void*>(c.group), 0x14, std::uint32_t{18});
        return ClockResult::Ok;
    }
    ClockResult Row(const NativeTemporaryModelClumpBorrow& borrow, void* context) noexcept
    {
        auto& c = *static_cast<Context*>(context);
        ++c.rows;
        c.activeBorrow = &borrow;
        auto& journal = (*c.journals)[c.index];
        if (!c.index && !c.mode)
        {
            const auto count = c.originals;
            auto refuse = [&](std::uint64_t generation, const NativeAnimationAssociationGroupReplacement& group, const NativeCustomAnimSequenceStorage& storage,
                              std::uint32_t index, const NativeAnimationHierarchyReferenceToken& hierarchy)
            {
                Journal    unused;
                J::Receipt receipt;
                Check(J::Initialize(c.image, unused, generation, borrow, group, storage, index, hierarchy, &receipt, &Original) != ClockResult::Ok &&
                          !receipt.entered && unused.CanDiscard() && c.originals == count,
                      "unqualified closed origin/H/physical index refuses before the original static entry");
            };
            NativeAnimationAssociationGroupReplacement emptyGroup;
            NativeCustomAnimSequenceStorage            emptyStorage;
            auto                                       wrongH = c.hierarchy[0];
            wrongH.ownerGeneration = 0;
            refuse(c.generation + 1, *c.published, *c.storage, 0, c.hierarchy[0]);
            refuse(c.generation, emptyGroup, *c.storage, 0, c.hierarchy[0]);
            refuse(c.generation, *c.published, emptyStorage, 0, c.hierarchy[0]);
            refuse(c.generation, *c.published, *c.storage, 0xffffffffu, c.hierarchy[0]);
            refuse(c.generation, *c.published, *c.storage, 0, wrongH);
            refuse(c.generation, *c.published, *c.storage, 0, c.hierarchy[1]);
            F::Store(reinterpret_cast<void*>(c.storage->IssuedTag()), 0x18, c.hierarchy[0].locator);
            refuse(c.generation, *c.published, *c.storage, 0, c.hierarchy[0]);
            F::Store(reinterpret_cast<void*>(c.storage->IssuedTag()), 0x18, std::uintptr_t{0});
            c.closedTested = true;
        }
        if (c.index)
        {
            J::Receipt unused;
            const auto count = c.originals;
            Check(J::Initialize(c.image, (*c.journals)[0], c.generation, borrow, *c.published, *c.storage, c.index, c.hierarchy[c.index], &unused, &Original) !=
                          ClockResult::Ok &&
                      !unused.entered && c.originals == count,
                  "a verified journal cannot be recycled through a different fresh temporary borrow");
        }
        const auto result =
            J::Initialize(c.image, journal, c.generation, borrow, *c.published, *c.storage, c.index, c.hierarchy[c.index], &c.receipts[c.index], &Original);
        // End only fixture fault injection after the actual journal copied its
        // post-entry facts. Claimed borrows/journal/group/temp debt never reset.
        ClockPrivateAnimationAssociationTest::Store(c.image + NativeBorrowedGcProfile::markerRva, LONG{0});
        F::Store(reinterpret_cast<void*>(c.group), 0x14, std::uint32_t{17});
        if (result == ClockResult::Ok && !c.index)
        {
            Journal    unused;
            J::Receipt receipt;
            const auto count = c.originals;
            Check(
                J::Initialize(c.image, journal, c.generation, borrow, *c.published, *c.storage, 0, c.hierarchy[0], &receipt, &Original) != ClockResult::Ok &&
                    J::Initialize(c.image, unused, c.generation, borrow, *c.published, *c.storage, 1, c.hierarchy[1], &receipt, &Original) != ClockResult::Ok &&
                    !receipt.entered && unused.CanDiscard() && c.originals == count,
                "the same live borrow cannot initialize a second row by changing journals after its one acknowledged call");
        }
        if (result != ClockResult::Ok)
        {
            J::Receipt unused;
            const auto count = c.originals;
            Check(J::Initialize(c.image, journal, c.generation, borrow, *c.published, *c.storage, c.index, c.hierarchy[c.index], &unused, &Original) !=
                          ClockResult::Ok &&
                      c.originals == count,
                  "an entered uncertain joint journal never replays after its original effect");
        }
        c.activeBorrow = nullptr;
        return result;
    }
}
namespace JointTemporaryStaticInitializerOwnerFixture
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
        namespace Hook = ClockJointTemporaryStaticOwnerTest;
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
        writes.Global(B::BankRva + B::RowBytes + B::AnimationCountOffset, std::uint32_t{2});
        writes.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{3});
        NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes, 2, 1);
        const auto zeroHierarchy = fixture.image + H::BankRva + 2 * H::RowBytes;
        writes.Global(zeroHierarchy - fixture.image + 8, setup.Sequence(0));
        writes.Global(zeroHierarchy - fixture.image + 0x10, std::uint16_t{0});
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
            Check(service.Renew(generation), "renew one actual joint original-entry admission callback");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "joint native work has an independent real producer without extending its lease");
            fixture.invokeTimer();
            pulse.Stop();
            Check(!pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(),
                  "joint callback preserves and restores the existing producer/engine handoff policy");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        namespace Group = NativeAnimationAssociationGroupReplacementCalls;
        namespace Arrays = NativeStaticAssociationArrayStorageCalls;
        namespace T = NativeTemporaryModelClumpCalls;
        namespace J = NativeTemporaryStaticAssociationInitializeCalls;
        using Journal = NativeTemporaryStaticAssociationInitialization;
        static_assert(!std::is_default_constructible_v<NativeTemporaryModelClumpBorrow> && !std::is_copy_constructible_v<NativeTemporaryModelClumpBorrow>);
        NativeAnimationAssociationGroupReferenceToken group;
        NativeAnimationBlockIdentity                  blockIdentity;
        NativeAnimationAssociationGroupReplacement    published;
        NativeCustomAnimSequenceStorage               storage;
        std::array<Journal, 2>                        journals;
        std::array<NativeTemporaryModelClump, 2>      temporary;
        std::array<T::Receipt, 2>                     temporaryReceipts;
        Group::Receipt                                groupReceipt;
        Arrays::Receipt                               allocation;
        action(
            [&]
            {
                Check(runtime.access.ResolveAnimationAssociationGroup(generation, 1, group) == ClockResult::Ok && group.locator == groups + 32,
                      "joint test begins with the actual current native group identity");
                Check(NativeAnimationBlockFactoryMemory({fixture.image}, &CheckTimingOwner).ReadIdentity(1, blockIdentity) == ClockResult::Ok,
                      "joint publication selects a current same-bank animation block");
                blockIdentity.ownerGeneration = generation;
                Check(Group::RetireCurrentChildren(fixture.image, published, generation, group, &CheckTimingOwner, {}, &groupReceipt) == ClockResult::Ok &&
                          Group::PublishBlock(fixture.image, published, generation, blockIdentity, &CheckTimingOwner, &groupReceipt) == ClockResult::Ok &&
                          Arrays::AllocateFresh(fixture.image, storage, generation, 2, &CheckTimingOwner, &allocation) == ClockResult::Ok &&
                          Group::PublishChildren(fixture.image, published, generation, storage, &CheckTimingOwner, &groupReceipt) == ClockResult::Ok &&
                          Group::PublishFullCount(fixture.image, published, generation, &CheckTimingOwner, &groupReceipt) == ClockResult::Ok &&
                          published.GetState() == NativeAnimationAssociationGroupReplacement::State::Published && published.IsRetained() &&
                          storage.GetState() == NativeCustomAnimSequenceStorage::State::EnginePublished,
                      "joint initializer receives an actual causal published static32 array, never a raw/fake group or private-free fallback");
            });
        Hook::Context context;
        context.image = fixture.image;
        context.group = group.locator;
        context.modelAddress = reinterpret_cast<std::uintptr_t>(model.data());
        context.model = {0, fixture.image + NativeModelBankProfile::BankRva, context.modelAddress};
        context.generation = generation;
        context.setup = &setup;
        context.published = &published;
        context.storage = &storage;
        context.journals = &journals;
        context.mode = scenario;
        context.hierarchy = {NativeAnimationHierarchyReferenceToken{generation, hierarchy}, NativeAnimationHierarchyReferenceToken{generation, zeroHierarchy}};
        Hook::current = &context;
        const T::Invocations invocations{&Hook::Create, &Hook::Setup, &Hook::Prepare, &Hook::Retire};
        ClockResult          result = ClockResult::NotInitialized;
        context.temporary = &temporary[0];
        action(
            [&]
            {
                result = T::WithTemporary(fixture.image, temporary[0], generation, context.model, &CheckTimingOwner, &Hook::Row, &context,
                                          &temporaryReceipts[0], &invocations);
            });
        const auto& observed = context.receipts[0];
        if (!scenario && result != ClockResult::Ok)
            std::fprintf(
                stderr,
                "Joint stage0 result=%u creates=%u prepares=%u originals=%u temp=%u journal=%u group=%u entered=%u returned=%u observed=%u verified=%u\n",
                static_cast<unsigned>(result), context.creates, context.prepares, context.originals, static_cast<unsigned>(temporary[0].GetState()),
                static_cast<unsigned>(journals[0].GetState()), static_cast<unsigned>(published.GetState()), observed.entered, observed.returned,
                observed.rowObserved, observed.verified);
        if (scenario)
        {
            Check(result != ClockResult::Ok && context.originals == 1 && context.retires == 0 && observed.entered && observed.rowObserved &&
                      observed.observedCount == 2 && observed.observedNodes == context.nodes[0] && observed.hierarchy == hierarchy && !observed.verified &&
                      journals[0].HasOutstandingResource() && temporary[0].HasOutstandingResource() && published.IsRetained() &&
                      storage.GetState() == NativeCustomAnimSequenceStorage::State::EnginePublished && !storage.CanFreeStorage() && !heap.freeCalls,
                  "unknown original or failed post-entry owner/group bookend preserves actual observed nodes and all three causal owner records");
            Check((scenario == 1 ? !observed.returned : observed.returned) && context.reentryTested && context.activeBorrow == nullptr,
                  "returned versus missing original acknowledgement remains distinct and no stack borrow escapes");
            const auto calls = context.originals, creates = context.creates, retires = context.retires;
            action(
                [&]
                {
                    T::Receipt rejected;
                    Check(T::WithTemporary(fixture.image, temporary[0], generation, context.model, &CheckTimingOwner, &Hook::Row, &context, &rejected,
                                           &invocations) != ClockResult::Ok &&
                              context.originals == calls && context.creates == creates && context.retires == retires && !heap.freeCalls,
                          "retained temporary/joint uncertainty cannot recreate, initialize again, retire actor or release static nodes");
                    Check(Arrays::RetireFresh(fixture.image, storage, generation, &CheckTimingOwner, &allocation) != ClockResult::Ok && !allocation.entered &&
                              !heap.freeCalls,
                          "published engine rows never become privately freeable after an entered joint failure");
                });
            std::puts("Production joint temporary static entered facts retained fixture completed");
        }
        else
        {
            Check(
                result == ClockResult::Ok && observed.entered && observed.returned && observed.rowObserved && observed.verified && observed.nativePed == 0 &&
                    observed.frameCount == 2 && observed.observedCount == 2 && observed.observedNodes == context.nodes[0] &&
                    F::Load<std::uintptr_t>(reinterpret_cast<void*>(context.nodes[0]), 0) == setup.Sequence(0) &&
                    F::Load<std::uintptr_t>(reinterpret_cast<void*>(context.nodes[0]), 8) == setup.Sequence(1) &&
                    F::Load<std::uintptr_t>(reinterpret_cast<void*>(storage.IssuedTag()), 0x18) == hierarchy && temporary[0].CanDiscard() &&
                    temporaryReceipts[0].retireAccepted && journals[0].CanDiscard() && context.closedTested && context.reentryTested &&
                    !runtime.pedReferences.IsRetained(),
                "mixed bone/name original-entry effects verify through actual closed temporary/published-row origins without ped registration or H prebinding");
            context.index = 1;
            context.temporary = &temporary[1];
            action(
                [&]
                {
                    result = T::WithTemporary(fixture.image, temporary[1], generation, context.model, &CheckTimingOwner, &Hook::Row, &context,
                                              &temporaryReceipts[1], &invocations);
                });
            const auto&              zero = context.receipts[1];
            MEMORY_BASIC_INFORMATION page{};
            if (result != ClockResult::Ok)
                std::fprintf(stderr, "Joint stage1 result=%u originals=%u temp=%u journal=%u entered=%u returned=%u observed=%u verified=%u\n",
                             static_cast<unsigned>(result), context.originals, static_cast<unsigned>(temporary[1].GetState()),
                             static_cast<unsigned>(journals[1].GetState()), zero.entered, zero.returned, zero.rowObserved, zero.verified);
            Check(result == ClockResult::Ok && zero.entered && zero.returned && zero.verified && zero.frameCount == 0 && zero.observedCount == 0 &&
                      zero.observedNodes == context.nodes[1] && context.zeroGuarded &&
                      VirtualQuery(reinterpret_cast<void*>(context.nodes[1]), &page, sizeof(page)) == sizeof(page) && (page.Protect & 0xff) == PAGE_NOACCESS &&
                      F::Load<std::uintptr_t>(reinterpret_cast<void*>(storage.IssuedTag() + 32), 0x10) == context.nodes[1] &&
                      F::Load<std::uintptr_t>(reinterpret_cast<void*>(storage.IssuedTag() + 32), 0x18) == zeroHierarchy && temporary[1].CanDiscard() &&
                      temporaryReceipts[1].retireAccepted && journals[1].CanDiscard() && context.originals == 2 && context.creates == 2 && context.retires == 2,
                  "zero-frame original-entry observation preserves its exact guarded tag without any positive node-extent dereference");
            DWORD ignored = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(context.nodes[1]), 8, context.zeroProtection, &ignored) != FALSE,
                  "restore only the task-owned guard page before ordinary engine-group fixture retirement");
            context.zeroGuarded = false;
            action(
                [&]
                {
                    Check(Group::FinishPublished(fixture.image, published, generation, &CheckTimingOwner, &groupReceipt) == ClockResult::Ok,
                          "only acknowledged per-row outcomes finish the actual group source continuation");
                });
            NativeAnimationAssociationGroupReplacement cleanup;
            ClockGroupReplacementOwnerTest::image = fixture.image;
            ClockGroupReplacementOwnerTest::parent = group.locator;
            ClockGroupReplacementOwnerTest::mode = 0;
            ClockGroupReplacementOwnerTest::targetsValid = true;
            ClockGroupReplacementOwnerTest::events.reserve(8);
            ClockPrivateAnimationAssociationTest::image = fixture.image;
            ClockPrivateAnimationAssociationTest::heapFailed = false;
            const auto frees = heap.freeCalls;
            action(
                [&]
                {
                    Check(Group::RetireCurrentChildren(fixture.image, cleanup, generation, group, &CheckTimingOwner, {}, &groupReceipt,
                                                       &ClockGroupReplacementOwnerTest::Retire) == ClockResult::Ok &&
                              heap.freeCalls == frees + 3,
                          "verified static nodes remain engine-group-owned and retire only through its original row/node/cookie lifecycle");
                });
            Check(service.End(), "joint success leaves no privately owned native storage or active borrowed scope behind");
            std::puts("Production joint temporary static initializer owner fixture completed");
            std::puts("Production joint temporary static closed origins fixture completed");
            std::puts("Production joint temporary static zero extent fixture completed");
        }
        Check(context.activeBorrow == nullptr, "every temporary borrow remains synchronous and nonescaping");
        Hook::current = nullptr;
        NativeCustomAnimSequenceStorageFixture::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        ClockPedReferenceOwnerTest::image = previousReferences;
        writes.Restore();
    }
}

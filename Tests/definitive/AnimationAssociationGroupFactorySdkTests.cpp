/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real owning group factories and derived CPU-cache destruction
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#pragma push_macro("main")
#undef main
#define main HistoricalGroupFactoryChildDefinitionsMain
#include "AnimationAssociationGroupChildProductsSdkTests.cpp"
#undef main
#pragma pop_macro("main")
#include "AnimationAssociationGroupFactoryManagerFixture.generated.h"
#include "../../Client/game_sa/CAnimBlendAssocGroupSA.h"
#include "../../Client/game_sa/CAnimBlockSA.h"
#include "ClassicAssociationGroupCpuLifetime.generated.h"
// Unexercised native routes stay explicit rejectors. The actual CPU lifecycle
// definitions above are selected verbatim from the real production SA source.
CAnimBlendAssociationSAInterface* CAnimBlendAssocGroupSA::CopyAnimation(unsigned int)
{
    std::abort();
}
void CAnimBlendAssocGroupSA::InitEmptyAssociations(RpClump*)
{
    std::abort();
}
bool CAnimBlendAssocGroupSA::IsCreated()
{
    std::abort();
}
int CAnimBlendAssocGroupSA::GetNumAnimations()
{
    std::abort();
}
CAnimBlendStaticAssociation* CAnimBlendAssocGroupSA::GetAnimation(unsigned int)
{
    std::abort();
}
eAnimGroup CAnimBlendAssocGroupSA::GetGroupID()
{
    std::abort();
}
void CAnimBlendAssocGroupSA::CreateAssociations(const char*)
{
    std::abort();
}
bool CAnimBlendAssocGroupSA::IsLoaded()
{
    std::abort();
}
int CAnimBlockSAInterface::GetIndex()
{
    std::abort();
}
void CAnimBlockSA::Request(EModelRequestType, bool)
{
    std::abort();
}
CAnimBlendHierarchySAInterface* CAnimBlockSA::GetAnimationHierarchyInterface(size_t)
{
    std::abort();
}
static_assert(std::has_virtual_destructor_v<CAnimBlendAssocGroup> && std::has_virtual_destructor_v<CAnimBlendAssocGroupSA>);
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupFactoryManagerReceiver::GetAnimBlendAssoc), decltype(&CAnimManagerDE::GetAnimBlendAssoc)>);
static_assert(
    std::is_same_v<decltype(&AnimationAssociationGroupFactoryManagerReceiver::GetAnimBlendAssocGroup), decltype(&CAnimManagerDE::GetAnimBlendAssocGroup)>);
namespace group_factory_tests
{
    using namespace group_child_products_tests;
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationAssociationGroupAccess, NativeAnimationBlockAccess, NativeAnimationStaticAssociationAccess
    {
        group_child_products_tests::Owner children;
        ClockResult                       resolveStatus = ClockResult::Ok;
        unsigned                          numeric = 0, raw = 0, malformed = 0;
        std::uint32_t                     requestedIndex = 0;
        std::uintptr_t                    requestedLocator = 0;
        std::uint64_t                     receivedGeneration = 0;
        void                              Select(std::uint32_t index)
        {
            children.group.index = index;
            for (auto& row : children.rows)
                row.groupIndex = index;
        }
        void Malform(NativeAnimationAssociationGroupReferenceToken& output) const noexcept
        {
            if (malformed == 1)
                ++output.ownerGeneration;
            if (malformed == 2)
                ++output.index;
            if (malformed == 3)
                output.locator = 0;
            if (malformed == 4)
                output.locator += sizeof(std::uintptr_t);
        }
        ClockResult ResolveAnimationAssociationGroup(std::uint64_t generation, std::uint32_t index,
                                                     NativeAnimationAssociationGroupReferenceToken& output) noexcept override
        {
            ++numeric;
            receivedGeneration = generation;
            requestedIndex = index;
            Select(index);
            output = children.group;
            Malform(output);
            return resolveStatus;
        }
        ClockResult ResolveAnimationAssociationGroupInterface(std::uint64_t generation, std::uintptr_t locator,
                                                              NativeAnimationAssociationGroupReferenceToken& output) noexcept override
        {
            ++raw;
            receivedGeneration = generation;
            requestedLocator = locator;
            output = children.group;
            Malform(output);
            return resolveStatus;
        }
        ClockResult ReadAnimationAssociationGroupBlock(const NativeAnimationAssociationGroupReferenceToken& token,
                                                       NativeAnimationAssociationGroupBlock&                output) noexcept override
        {
            return children.ReadAnimationAssociationGroupBlock(token, output);
        }
        ClockResult ResolveAnimationAssociationGroupStaticById(const NativeAnimationAssociationGroupReferenceToken& token, std::uint32_t id,
                                                               NativeAnimationStaticAssociationReferenceToken& output) noexcept override
        {
            return children.ResolveAnimationAssociationGroupStaticById(token, id, output);
        }
        ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity& token, NativeAnimationBlockValues& output) noexcept override
        {
            return children.ReadAnimationBlock(token, output);
        }
        ClockResult ReadAnimationBlockName(const NativeAnimationBlockIdentity& token, char*& output) noexcept override
        {
            return children.ReadAnimationBlockName(token, output);
        }
        ClockResult AddAnimationBlockRef(const NativeAnimationBlockIdentity& token) noexcept override { return children.AddAnimationBlockRef(token); }
        ClockResult ReadAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken& token,
                                                   NativeAnimationStaticAssociationValues&               output) noexcept override
        {
            return children.ReadAnimationStaticAssociation(token, output);
        }
    };
    struct NoGroup final : NativeAnimationManagerAccess, NativeAnimationBlockAccess
    {
    };
    using Buffers = std::array<std::array<unsigned char, 32>, 5>;
    Buffers Bytes(const Owner& o)
    {
        const auto& c = o.children;
        return {c.groupRow, c.blockA, c.blockB, c.staticA, c.staticB};
    }
    unsigned Calls(const Owner& o)
    {
        const auto& c = o.children;
        return o.numeric + o.raw + c.blockLookups + c.staticLookups + c.blockReads + c.staticReads + c.adds + c.scalarReads;
    }
    void DeChecks()
    {
        Owner                                           owner;
        GameFixture                                     game;
        AnimationAssociationGroupFactoryManagerReceiver manager(owner, owner, game, Generation);
        const auto                                      bytes = Bytes(owner);
        const auto                                      beforeWarm = owner.children.blockLookups;
        auto                                            first = manager.GetAnimBlendAssoc(static_cast<AssocGroupId>(0xffffffffu));
        Check(first && owner.requestedIndex == 0xffffffffu && owner.receivedGeneration == Generation && owner.children.blockLookups == beforeWarm + 1 &&
                  !owner.children.scalarReads,
              "numeric factory preserves full DWORD without enum/Created gates and warms actual block path exactly once");
        Watch(first.get(), 0);
        auto* block = first->GetAnimBlock();
        auto* row0 = first->GetAnimation(100);
        auto* row1 = first->GetAnimation(101);
        Watch(block, 1);
        Watch(row0, 2);
        Watch(row1, 3);
        const auto locator = owner.children.group.locator;
        const auto rawWarm = owner.children.blockLookups;
        auto       second = manager.GetAnimBlendAssocGroup(reinterpret_cast<CAnimBlendAssocGroupSAInterface*>(locator));
        Check(second && second.get() != first.get() && owner.requestedLocator == locator && owner.children.blockLookups == rawWarm + 1,
              "opaque raw locator makes a distinct owning CPU wrapper with one warm and exact full-width identity roundtrip");
        if constexpr (sizeof(std::uintptr_t) > 4)
            Check(locator > 0xffffffffull, "x64 factory fixture exercises actual group tags above DWORD range");
        Watch(second.get(), 4);
        Watch(second->GetAnimBlock(), 5);
        Watch(second->GetAnimation(100), 6);
        const auto beforeDelete = Calls(owner);
        first.reset();
        Check(deleted[0] == 1 && deleted[1] == 1 && deleted[2] == 1 && deleted[3] == 1 && !deleted[4] && !deleted[5] && !deleted[6] &&
                  Calls(owner) == beforeDelete && Bytes(owner) == bytes,
              "real unique_ptr common-base deletion destroys DE group and all owned CPU children once without native operations");
        second.reset();
        Check(deleted[4] == 1 && deleted[5] == 1 && deleted[6] == 1 && Calls(owner) == beforeDelete && Bytes(owner) == bytes,
              "independent factory wrapper owns only its CPU caches and never shared native children or refs");
        AnimationAssociationGroupFactoryManagerReceiver observation(owner, Generation);
        const auto                                      noProviderCalls = Calls(owner);
        Check(!observation.GetAnimBlendAssocGroup(nullptr) && Calls(owner) == noProviderCalls, "raw null returns before any provider/game/native observation");
        Refuses([&] { (void)observation.GetAnimBlendAssoc(0); }, ClockResult::NotInitialized, "GetAnimBlendAssoc");
        NoGroup                                         noGroup;
        AnimationAssociationGroupFactoryManagerReceiver missing(noGroup, noGroup, game, Generation);
        Refuses([&] { (void)missing.GetAnimBlendAssoc(0); }, ClockResult::NotInitialized, "GetAnimBlendAssoc");
        bool affinity = false;
        try
        {
            AnimationAssociationGroupFactoryManagerReceiver foreign(owner, noGroup, game, Generation);
        }
        catch (const std::invalid_argument&)
        {
            affinity = true;
        }
        Check(affinity, "factory manager refuses a second complete block owner before producing any group");
        for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::WriteFailure,
                                   ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
        {
            owner.resolveStatus = failure;
            const auto warm = owner.children.blockLookups;
            Refuses([&] { (void)manager.GetAnimBlendAssoc(119); }, failure, "GetAnimBlendAssoc");
            Refuses([&] { (void)manager.GetAnimBlendAssocGroup(reinterpret_cast<CAnimBlendAssocGroupSAInterface*>(locator)); }, failure,
                    "GetAnimBlendAssocGroup");
            Check(owner.children.blockLookups == warm, "owner resolver refusal cannot warm, allocate native data or invent a fallback product");
        }
        owner.resolveStatus = ClockResult::Ok;
        for (unsigned malformed = 1; malformed != 4; ++malformed)
        {
            owner.malformed = malformed;
            Refuses([&] { (void)manager.GetAnimBlendAssoc(119); }, malformed == 1 ? ClockResult::OwnerConflict : ClockResult::BindingChanged,
                    "GetAnimBlendAssoc");
        }
        owner.malformed = 4;
        Refuses([&] { (void)manager.GetAnimBlendAssocGroup(reinterpret_cast<CAnimBlendAssocGroupSAInterface*>(locator)); }, ClockResult::OwnerConflict,
                "GetAnimBlendAssocGroup");
        owner.malformed = 0;
        owner.children.blockStatus = ClockResult::ReadFailure;
        Refuses([&] { (void)manager.GetAnimBlendAssoc(119); }, ClockResult::ReadFailure, "GetAnimBlock");
        owner.children.blockStatus = ClockResult::Ok;
        Check(!gameCalls && !owner.children.adds && !owner.children.scalarReads && Bytes(owner) == bytes,
              "warm refusal RAII and all factory paths preserve native buffers/ref facts with no successful game body");
        std::puts("Actual association group manager factory SDK fixture completed");
    }
    void ClassicChecks()
    {
        CAnimBlockSAInterface blockA{}, blockB{};
        blockA.usRefs = 23;
        blockB.usRefs = 37;
        CAnimBlendAssocGroupSAInterface row{};
        row.pAnimBlock = &blockA;
        row.iNumAnimations = 7;
        row.iIDOffset = -2;
        row.groupID = static_cast<AssocGroupId>(0xffffffffu);
        std::array<unsigned char, sizeof(row)>    savedRow{};
        std::array<unsigned char, sizeof(blockA)> savedA{}, savedB{};
        std::memcpy(savedRow.data(), &row, sizeof(row));
        std::memcpy(savedA.data(), &blockA, sizeof(blockA));
        std::memcpy(savedB.data(), &blockB, sizeof(blockB));
        std::unique_ptr<CAnimBlendAssocGroup> group = std::make_unique<CAnimBlendAssocGroupSA>(&row);
        Watch(group.get(), 0);
        auto* first = group->GetAnimBlock();
        Watch(first, 1);
        Check(first && first->GetInterface() == &blockA && first->GetRefs() == 23,
              "actual extracted classic constructor/setup owns a CPU block wrapper over the borrowed native block");
        row.pAnimBlock = &blockB;
        auto* second = group->GetAnimBlock();
        Check(second && second->GetInterface() == &blockB && deleted[1] == 1, "real classic SetupAnimBlock refresh destroys only previous CPU wrapper");
        Watch(second, 2);
        row.pAnimBlock = nullptr;
        Check(!group->GetAnimBlock() && deleted[2] == 1, "real classic null-block refresh clears cached CPU wrapper");
        row.pAnimBlock = &blockA;
        auto* final = group->GetAnimBlock();
        Watch(final, 3);
        group.reset();
        Check(deleted[0] == 1 && deleted[1] == 1 && deleted[2] == 1 && deleted[3] == 1 && std::memcmp(&row, savedRow.data(), sizeof(row)) == 0 &&
                  std::memcmp(&blockA, savedA.data(), sizeof(blockA)) == 0 && std::memcmp(&blockB, savedB.data(), sizeof(blockB)) == 0,
              "actual classic destructor through common unique_ptr releases final CPU cache exactly once without native/ref destruction");
        std::puts("Actual classic association group CPU base-deletion fixture completed");
    }
}
int main()
{
    try
    {
        group_factory_tests::DeChecks();
        group_factory_tests::ClassicChecks();
        std::printf("Passed %u association group factory lifecycle SDK checks (%zu-bit; CPU facades only, no native engine code)\n",
                    group_child_products_tests::checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL after %u group factory lifecycle SDK checks: %s\n", group_child_products_tests::checks, e.what());
        return 1;
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual one-name group SDK transport and bound CPU-cache expiration
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalGroupCreateChildDefinitionsMain
#include "AnimationAssociationGroupChildProductsSdkTests.cpp"
#undef main
#pragma pop_macro("main")
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::CreateAssociations), decltype(&CAnimBlendAssocGroupDE::CreateAssociations)>);
static_assert(!std::is_abstract_v<CAnimBlendAssocGroupDE> && !std::is_default_constructible_v<CAnimBlendAssocGroupDE>);
namespace group_create_tests
{
    using namespace group_child_products_tests;
    struct Owner final : NativeAnimationAssociationGroupAccess, NativeAnimationBlockAccess, NativeAnimationStaticAssociationAccess
    {
        // Reuse only the established owned buffers/read answers. All three
        // service interfaces still belong to this exact complete owner object.
        group_child_products_tests::Owner                     children;
        NativeAnimationAssociationGroupReferenceToken         received{};
        NativeAnimationAssociationGroupChildBorrowInvalidator bound{};
        void*                                                 expectedContext = nullptr;
        std::string                                           expectedName, receivedName;
        char*                                                 callerName = nullptr;
        NativeAnimationAssociationGroupReferenceToken*        callerToken = nullptr;
        ClockResult                                           status = ClockResult::Ok;
        unsigned                                              creates = 0, invalidations = 0;
        bool                                                  invalidate = false;
        ClockResult CreateAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken& token, const std::string& name,
                                                    NativeAnimationAssociationGroupChildBorrowInvalidator invalidator) noexcept override
        {
            ++creates;
            received = token;
            receivedName = name;
            bound = invalidator;
            Check(Same(token, children.group) && name == expectedName && bound.context == expectedContext && bound.invalidate,
                  "one actual SDK owner entry receives copied token/full name and an exact CPU-owner invalidator");
            if (callerName)
                callerName[0] = 'X';
            if (callerToken)
                *callerToken = {};
            Check(Same(token, children.group) && name == expectedName, "mutating caller buffers during owner entry cannot redirect copied group/name intent");
            if (invalidate)
            {
                ++invalidations;
                bound();
            }
            return status;
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
    using Buffers = std::array<std::array<unsigned char, 32>, 5>;
    Buffers NativeBuffers(const Owner& owner)
    {
        return {owner.children.groupRow, owner.children.blockA, owner.children.blockB, owner.children.staticA, owner.children.staticB};
    }
    std::array<unsigned, 6> Reads(const Owner& owner)
    {
        const auto& c = owner.children;
        return {c.blockLookups, c.staticLookups, c.blockReads, c.staticReads, c.adds, c.scalarReads};
    }
    void CreateChecks()
    {
        Owner       owner;
        GameFixture game;
        auto        callerToken = owner.children.group;
        {
            AnimationAssociationGroupReceiver receiver(owner, callerToken, game);
            owner.expectedContext = static_cast<CAnimBlendAssocGroupDE*>(&receiver);
            CAnimBlendAssocGroup& sdk = receiver;
            auto*                 block = sdk.GetAnimBlock();
            auto*                 row0 = sdk.GetAnimation(100);
            auto*                 row1 = sdk.GetAnimation(101);
            Watch(block, 0);
            Watch(row0, 1);
            Watch(row1, 2);
            const auto buffers = NativeBuffers(owner);
            const auto reads = Reads(owner);
            char       name[] = "MiXeD-Original-Block-Name-Longer-Than16";
            owner.expectedName = name;
            owner.callerName = name;
            owner.callerToken = &callerToken;
            owner.invalidate = true;
            sdk.CreateAssociations(name);
            Check(owner.creates == 1 && owner.invalidations == 1 && owner.receivedName == owner.expectedName && name[0] == 'X' &&
                      owner.received.ownerGeneration == Generation && !callerToken.ownerGeneration,
                  "SDK copies full case-sensitive long name/token before callbacks and forwards exactly one source operation");
            Check(deleted[0] == 1 && deleted[1] == 1 && deleted[2] == 1 && Reads(owner) == reads && NativeBuffers(owner) == buffers,
                  "owner-selected invalidation expires actual CPU children without frontend reads, native writes, refs or rollback");
            owner.callerName = nullptr;
            owner.callerToken = nullptr;
            owner.invalidate = false;
            auto* nextBlock = sdk.GetAnimBlock();
            auto* nextRow = sdk.GetAnimation(100);
            Watch(nextBlock, 3);
            Watch(nextRow, 4);
            const auto refusalReads = Reads(owner);
            owner.expectedName = "PED";
            for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::WriteFailure,
                                       ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
            {
                owner.status = failure;
                const auto before = owner.creates;
                Refuses([&] { sdk.CreateAssociations("PED"); }, failure, "CreateAssociations");
                Check(owner.creates == before + 1 && !deleted[3] && !deleted[4] && Reads(owner) == refusalReads,
                      "pre-effect owner refusal is operation-tagged, called once and cannot proactively clear or replace children");
            }
            owner.invalidate = true;
            owner.status = ClockResult::BindingChanged;
            Refuses([&] { sdk.CreateAssociations("PED"); }, ClockResult::BindingChanged, "CreateAssociations");
            Check(deleted[3] == 1 && deleted[4] == 1 && owner.invalidations == 2 && NativeBuffers(owner) == buffers,
                  "a later owner refusal preserves acknowledged CPU expiration instead of reviving old borrows");
            owner.status = ClockResult::Ok;
            owner.invalidate = false;
            owner.expectedName.clear();
            const auto beforeEmpty = owner.creates;
            sdk.CreateAssociations("");
            Check(owner.creates == beforeEmpty + 1 && owner.receivedName.empty(),
                  "empty name intent reaches the real owner without a frontend default or missing-block policy");
            owner.expectedName.assign("high-");
            owner.expectedName.push_back(static_cast<char>(0xe9));
            sdk.CreateAssociations(owner.expectedName.c_str());
            Check(owner.receivedName == owner.expectedName, "full byte spelling survives SDK transport without case folding or name truncation");
            const auto beforeNull = owner.creates;
            bool       refusedNull = false;
            try
            {
                sdk.CreateAssociations(nullptr);
            }
            catch (const std::invalid_argument&)
            {
                refusedNull = true;
            }
            Check(refusedNull && owner.creates == beforeNull, "null source C string refuses explicitly before owner entry");
            Check(!gameCalls && !owner.children.adds && !owner.children.scalarReads && NativeBuffers(owner) == buffers,
                  "SDK transport and CPU borrow expiration execute no game body or native group creation/free");
        }
        Check(deleted[0] == 1 && deleted[1] == 1 && deleted[2] == 1 && deleted[3] == 1 && deleted[4] == 1 && !gameCalls,
              "concrete CPU destruction cannot double-delete source-expired child facades");
        NativeAnimationAssociationGroupAccess absent;
        AnimationAssociationGroupReceiver     unconfigured(absent, owner.children.group);
        Refuses([&] { unconfigured.CreateAssociations("PED"); }, ClockResult::NotInitialized, "CreateAssociations");
        std::puts("Actual animation association group CreateAssociations SDK fixture completed");
        std::puts("Actual animation association group Create CPU invalidation transport fixture completed");
    }
}
int main()
{
    try
    {
        group_create_tests::CreateChecks();
        std::printf("Passed %u association group Create SDK checks (%zu-bit; dispatch only, no native engine code)\n", group_child_products_tests::checks,
                    sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u group Create SDK checks: %s\n", group_child_products_tests::checks, error.what());
        return 1;
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Expire actual group-owned CPU child borrows without native effects
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalGroupChildProductsDefinitionsMain
#include "AnimationAssociationGroupChildProductsSdkTests.cpp"
#undef main
#pragma pop_macro("main")
#include <memory>
namespace group_cache_invalidation_tests
{
    using namespace group_child_products_tests;
    class Receiver final : public CAnimBlendAssocGroupDE
    {
    public:
        Receiver(NativeAnimationAssociationGroupAccess& owner, const NativeAnimationAssociationGroupReferenceToken& token, CGame& game) noexcept
            : CAnimBlendAssocGroupDE(owner, token, game)
        {
        }
        NativeAnimationAssociationGroupChildBorrowInvalidator BindInvalidator() noexcept { return ChildBorrowInvalidator(); }
        void                                                  CreateAssociations(const char*) override
        {
            // Completing the fixture's one remaining pure method grants no
            // successful production group factory or native replacement path.
            std::abort();
        }
    };
    static_assert(!std::is_abstract_v<Receiver> && std::is_abstract_v<CAnimBlendAssocGroupDE>);
    using Buffers = std::array<std::array<unsigned char, 32>, 5>;
    Buffers NativeBuffers(const Owner& owner)
    {
        return {owner.groupRow, owner.blockA, owner.blockB, owner.staticA, owner.staticB};
    }
    std::array<unsigned, 7> Calls(const Owner& owner)
    {
        return {owner.blockLookups, owner.staticLookups, owner.blockReads, owner.staticReads, owner.adds, owner.scalarReads, gameCalls};
    }
    void CacheChecks()
    {
        Owner                 firstOwner, independentOwner;
        GameFixture           game;
        auto                  sourceToken = firstOwner.group;
        auto                  first = std::make_unique<Receiver>(firstOwner, sourceToken, game);
        auto                  independent = std::make_unique<Receiver>(independentOwner, independentOwner.group, game);
        CAnimBlendAssocGroup& sdk = *first;
        auto*                 oldBlock = sdk.GetAnimBlock();
        auto*                 oldStatic0 = sdk.GetAnimation(100);
        auto*                 oldStatic1 = sdk.GetAnimation(101);
        Check(oldBlock && oldStatic0 && oldStatic1 && oldStatic0 != oldStatic1 && oldBlock == sdk.GetAnimBlock() && oldStatic0 == sdk.GetAnimation(100) &&
                  oldStatic1 == sdk.GetAnimation(101),
              "populate actual stable CPU block and two physical static products before invalidation");
        Watch(oldBlock, 0);
        Watch(oldStatic0, 1);
        Watch(oldStatic1, 2);
        auto* independentBlock = independent->GetAnimBlock();
        auto* independentStatic = independent->GetAnimation(100);
        Watch(independentBlock, 3);
        Watch(independentStatic, 4);
        const auto firstBytes = NativeBuffers(firstOwner), independentBytes = NativeBuffers(independentOwner);
        const auto firstCalls = Calls(firstOwner), independentCalls = Calls(independentOwner);
        Receiver*  callerSelectedGroup = first.get();
        auto       bound = callerSelectedGroup->BindInvalidator();
        const auto copied = bound;
        Check(bound.context == static_cast<CAnimBlendAssocGroupDE*>(first.get()) && bound.invalidate,
              "returned callback context names the actual CPU group that owns these borrows");
        // Repoint caller variables after binding. The callback must retain the
        // exact CPU owner, not consult mutable caller group/token arguments.
        callerSelectedGroup = independent.get();
        sourceToken = independentOwner.group;
        bound();
        Check(callerSelectedGroup == independent.get() && Same(sourceToken, independentOwner.group) && deleted[0] == 1 && deleted[1] == 1 && deleted[2] == 1 &&
                  !deleted[3] && !deleted[4],
              "bound invalidation retires all first-owner CPU children once and leaves independent group cache intact");
        Check(Calls(firstOwner) == firstCalls && Calls(independentOwner) == independentCalls && NativeBuffers(firstOwner) == firstBytes &&
                  NativeBuffers(independentOwner) == independentBytes,
              "cache invalidation performs no native reads, writes, ref calls, game calls or raw row cleanup");
        copied();
        bound();
        Check(deleted[0] == 1 && deleted[1] == 1 && deleted[2] == 1 && !deleted[3] && !deleted[4] && Calls(firstOwner) == firstCalls &&
                  Calls(independentOwner) == independentCalls,
              "repeated copied invalidators on an empty cache cannot double-delete or touch another owner");
        Check(independent->GetAnimBlock() == independentBlock && independent->GetAnimation(100) == independentStatic,
              "independent CPU products preserve identity across another group's invalidation");
        const auto nativeBlock = firstOwner.block.interfaceAddress;
        const auto nativeStatic0 = firstOwner.rows[0].locator, nativeStatic1 = firstOwner.rows[1].locator;
        // The old immutable CPU identity lease has expired. Even the same
        // native address may now produce a fresh owner-issued CPU block view.
        ++firstOwner.block.identity.index;
        firstOwner.block.identity.name[0] = 'x';
        auto* freshBlock = sdk.GetAnimBlock();
        auto* freshStatic0 = sdk.GetAnimation(100);
        auto* freshStatic1 = sdk.GetAnimation(101);
        Check(freshBlock && freshStatic0 && freshStatic1 && freshStatic0 != freshStatic1 &&
                  reinterpret_cast<std::uintptr_t>(freshBlock->GetInterface()) == nativeBlock && firstOwner.rows[0].locator == nativeStatic0 &&
                  firstOwner.rows[1].locator == nativeStatic1 &&
                  freshBlock->GetIndex() == static_cast<decltype(freshBlock->GetIndex())>(firstOwner.block.identity.index) &&
                  freshStatic0->GetAnimID() == 700 && freshStatic1->GetAnimID() == 701,
              "same native addresses can issue fresh actual CPU children after invalidation without reviving the old identity lease");
        // Global delete observation drops a retired address before malloc can
        // reuse it. Fresh CPU objects need not have different pointer values.
        Watch(freshBlock, 5);
        Watch(freshStatic0, 6);
        Watch(freshStatic1, 7);
        Check(freshBlock == sdk.GetAnimBlock() && freshStatic0 == sdk.GetAnimation(100) && freshStatic1 == sdk.GetAnimation(101) && !deleted[5] &&
                  !deleted[6] && !deleted[7] && deleted[0] == 1 && deleted[1] == 1 && deleted[2] == 1,
              "fresh products establish a new stable CPU cache while old retirement counts remain exactly one");
        first.reset();
        Check(deleted[5] == 1 && deleted[6] == 1 && deleted[7] == 1 && deleted[0] == 1 && deleted[1] == 1 && deleted[2] == 1 && !deleted[3] && !deleted[4] &&
                  NativeBuffers(firstOwner) == firstBytes && NativeBuffers(independentOwner) == independentBytes,
              "actual concrete group destruction releases fresh CPU products once without native buffer cleanup or independent deletion");
        Check(independent->GetAnimBlock() == independentBlock && independent->GetAnimation(100) == independentStatic,
              "independent group remains usable after the invalidated CPU group is destroyed");
        independent.reset();
        Check(deleted[3] == 1 && deleted[4] == 1 && !firstOwner.adds && !independentOwner.adds && !firstOwner.scalarReads && !independentOwner.scalarReads &&
                  !gameCalls && NativeBuffers(firstOwner) == firstBytes && NativeBuffers(independentOwner) == independentBytes,
              "all CPU owners retire their own children once with no game body, native ref, raw free or native row mutation");
        std::puts("Actual animation association group cache invalidation SDK fixture completed");
        std::puts("Actual animation association group cache CPU retirement fixture completed");
    }
}
int main()
{
    try
    {
        group_cache_invalidation_tests::CacheChecks();
        std::printf("Passed %u association group cache invalidation SDK checks (%zu-bit; dispatch only, no native engine code)\n",
                    group_child_products_tests::checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u group cache invalidation SDK checks: %s\n", group_child_products_tests::checks, error.what());
        return 1;
    }
}

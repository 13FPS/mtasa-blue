/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse classic player movement choices without native side effects
 ****************************************************************************/
#pragma once
#include <cstdint>
#include "CPed.h"
#include "CAnimBlock.h"

namespace MtaGame
{
    class CPlayerMoveAnimPolicy final
    {
    public:
        enum class Kind : std::uint8_t
        {
            Invalid,
            Default,
            Custom
        };
        struct Selection
        {
            Kind        kind;
            const char* blockName;
        };

        static Selection SelectRequest(std::uint32_t group) noexcept
        {
            // Keep the existing SDK range gate before default handling. A
            // default request clears the wrapper override without a native write.
            if (!IsValidMoveAnim(group))
                return {Kind::Invalid, nullptr};
            if (group == MOVE_DEFAULT)
                return {Kind::Default, nullptr};
            switch (group)
            {
                case 55:
                case 58:
                case 61:
                case 64:
                case 67:
                    return {Kind::Custom, "fat"};
                case 56:
                case 59:
                case 62:
                case 65:
                case 68:
                    return {Kind::Custom, "muscular"};
                case 138:
                    return {Kind::Custom, "skate"};
                default:
                    return {Kind::Custom, "ped"};
            }
        }

        template <class Operations>
        static void SetMoveAnim(std::uint32_t group, Operations& operations)
        {
            const auto selection = SelectRequest(group);
            if (selection.kind == Kind::Invalid)
                return;
            if (selection.kind == Kind::Default)
            {
                operations.ClearOverride();
                return;
            }

            // Preserve the classic name construction and both distinct block
            // facade lifetimes. These callbacks supply actual owner operations;
            // this itinerary grants no native ownership, reference or rollback.
            typename Operations::BlockName blockName;
            blockName = selection.blockName;
            if (!operations.IsGroupLoaded(group))
            {
                auto block = operations.GetAnimationBlock(blockName);
                if (block && !block->IsLoaded())
                {
                    block->Request(BLOCKING, true);
                    operations.RecordBlockReference(blockName);  // Request added the native reference.
                }
                if (!operations.IsGroupLoaded(group))
                    return;
            }
            if (!operations.IsGroupValid(group))
                return;

            // Lookup still occurs when the module already recorded this name.
            // Insertion precedes AddRef, so an exception keeps accepted intent.
            auto block = operations.GetAnimationBlock(blockName);
            if (!block)
                return;
            if (!operations.HasBlockReference(blockName))
            {
                operations.RecordBlockReference(blockName);
                block->AddRef();
            }
            operations.StoreOverride(group);
            operations.StoreNativeGroup(group);
            operations.Reapply();
        }

        static constexpr int ChooseProcessAnimGroup(int requested, int custom) noexcept
        {
            if (custom)
            {
                // Only this hook preserves a native jetpack request. Otherwise
                // an ordinary player override combines its physique with the
                // requested pose; an explicit special pose stays authoritative.
                if (requested == MOVE_JETPACK)
                    return requested;
                if (IsPhysiqueBased(custom) && !IsSpecialPose(custom) && IsPhysiqueBased(requested))
                    return requested - PhysiqueIndex(requested) + PhysiqueIndex(custom);
                return custom;
            }
            return requested;
        }

        static constexpr int ChooseDefaultMotionGroup(int requested, int custom) noexcept
        {
            // Clothes asks for the base player physique, even when the custom
            // override has an explicit weapon pose. It has no jetpack exception.
            if (custom && IsPhysiqueBased(custom))
                return MOVE_PLAYER + PhysiqueIndex(custom);
            return requested;
        }

    private:
        static constexpr bool IsPhysiqueBased(int group) noexcept { return group >= MOVE_PLAYER && group <= MOVE_CSAW_M; }
        static constexpr bool IsSpecialPose(int group) noexcept { return group >= MOVE_ROCKET && group <= MOVE_CSAW_M; }
        static constexpr int  PhysiqueIndex(int group) noexcept { return (group - MOVE_PLAYER) % 3; }
    };
}

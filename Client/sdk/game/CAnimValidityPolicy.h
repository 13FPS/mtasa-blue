/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's animation-ID gate and signed association comparison
 ****************************************************************************/
#pragma once
#include "CAnimBlendAssocGroup.h"
#include <cstdint>
#include <utility>
namespace MtaGame
{
    class CAnimValidityPolicy final
    {
    public:
        template <class ReadAssociationId>
        static bool IsValidAnim(std::uint32_t group, std::uint32_t animation, ReadAssociationId&& readAssociationId)
        {
            static_assert(static_cast<int>(eAnimID::ANIM_ID_UNDEFINED) == -1);
            static_assert(static_cast<int>(eAnimID::ANIM_ID_MAX) > 0);
            // The actual fixed-int enum gate admits only nonnegative IDs
            // below MAX. Invalid caller bits must not acquire a group.
            if (animation >= static_cast<std::uint32_t>(eAnimID::ANIM_ID_MAX))
                return false;
            std::int16_t selected = 0;
            if (!std::forward<ReadAssociationId>(readAssociationId)(group, animation, selected))
                return false;
            // The source field is signed short. Its negative sentinels are
            // mismatches, not unsigned animation IDs or a default fallback.
            return static_cast<std::int32_t>(selected) == static_cast<std::int32_t>(animation);
        }
    };
}

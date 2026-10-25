/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's animation-group validation gate across real backends
 ****************************************************************************/
#pragma once
#include "CAnimBlendAssocGroup.h"
#include <cstdint>
#include <utility>
namespace MtaGame
{
    class CAnimGroupValidationPolicy final
    {
    public:
        template <class ReadCreated>
        static bool IsValidGroup(std::uint32_t group, ReadCreated&& readCreated)
        {
            static_assert(static_cast<int>(eAnimGroup::ANIM_GROUP_NONE) == -1);
            static_assert(static_cast<int>(eAnimGroup::ANIM_TOTAL_GROUPS) > 0);
            // The original fixed-int enum gate admits only these nonnegative
            // IDs. Express it without implementation-defined unsigned-to-int
            // conversion, and do not acquire a backend group for invalid IDs.
            if (group >= static_cast<std::uint32_t>(eAnimGroup::ANIM_TOTAL_GROUPS))
                return false;
            return std::forward<ReadCreated>(readCreated)(group);
        }
    };
}

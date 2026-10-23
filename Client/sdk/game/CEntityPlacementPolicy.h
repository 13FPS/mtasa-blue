/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share actual MTA finite placement and teleport commit ordering
 ****************************************************************************/
#pragma once
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <cmath>
#include <cstdint>

namespace MtaGame
{
    // Only CPU selection/order is shared. Native position storage, world/RW
    // operations and class-specific effects must remain real callback effects.
    struct CEntityPlacementPolicy final
    {
        static bool           IsFinite(const CVector& value) noexcept { return std::isfinite(value.fX) && std::isfinite(value.fY) && std::isfinite(value.fZ); }
        static constexpr bool RequiresTrainRailRecalculation(std::uint16_t model) noexcept
        {
            return model == 537 || model == 538 || model == 569 || model == 570 || model == 590 || model == 449;
        }
        template <class Place, class Remember, class After>
        static void SetPosition(float x, float y, float z, Place&& place, Remember&& remember, After&& after)
        {
            const CVector requested(x, y, z);
            if (!IsFinite(requested))
                return;
            // Classic remembers the actual committed vector before train or
            // object post-effects, including its null-placement branch. A DE
            // refusal/entered write failure must throw before claiming this
            // commit; no CPU cache can acknowledge uncertain native effects.
            const auto* committed = place(requested);
            if (committed)
                remember(*committed);
            after();
        }
        template <class Place>
        static void SetPosition(CVector* value, Place&& place)
        {
            if (value)
                place(value->fX, value->fY, value->fZ);
        }
        template <class HasMatrix, class Place, class NativeTeleport>
        static void Teleport(float x, float y, float z, HasMatrix&& hasMatrix, Place&& place, NativeTeleport&& teleport)
        {
            const CVector requested(x, y, z);
            if (!IsFinite(requested))
                return;
            // Select this branch before SetPosition, exactly as CEntitySA.
            // Inline-only placement does not acquire native teleport effects.
            const bool matrix = hasMatrix();
            place(requested);
            if (matrix)
                teleport(requested, true);
        }
    };
}

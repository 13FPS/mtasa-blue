/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Resolve actual registered SDK peds before world entity exclusion
 *****************************************************************************/
#pragma once
#include "CWorldJetpackHeightDE.h"
#include "CPoolsDE.h"
#include "NativeWorldIgnoreEntityAccess.h"

namespace mta::game_de
{
    class CWorldIgnoreEntityDE : public CWorldJetpackHeightDE
    {
    public:
        void IgnoreEntity(CEntity* entity) override;

    protected:
        CWorldIgnoreEntityDE(NativeWorldGroundAccess& ground, NativeWorldLineAccess& line, NativeWorldAreaAccess& area, NativeWorldJetpackHeightAccess& jetpack,
                             CPoolsDE& existingPools, NativeWorldIgnoreEntityAccess& ignore) noexcept
            : CWorldJetpackHeightDE(ground, line, area, jetpack), pools(existingPools), ignoreOwner(ignore)
        {
        }

    private:
        CPoolsDE&                      pools;
        NativeWorldIgnoreEntityAccess& ignoreOwner;
    };
    // Supported registered peds use the real existing pool/controller pair.
    // Non-ped native wrappers and a complete GetWorld/factory remain unsolved;
    // this layer never borrows a classic CEntitySA interface or invents one.
    static_assert(std::is_abstract_v<CWorldIgnoreEntityDE> && std::is_base_of_v<CWorldJetpackHeightDE, CWorldIgnoreEntityDE>);
}

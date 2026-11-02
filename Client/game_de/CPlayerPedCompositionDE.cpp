/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bind the existing ped and player chains to the same controlled owner
 ****************************************************************************/
#include "CPlayerPedCompositionDE.h"
#include "CPoolsDE.h"
#include "NativeGameTimingAccess.h"
#include <stdexcept>

namespace mta::game_de
{
    NativeGameTimingAccess& CPlayerPedCompositionDE::RequirePoolOwner(CPoolsDE& pools, NativeGameTimingAccess& owner)
    {
        // Compare the actual complete typed endpoint, not different interface
        // subobject addresses. A foreign pool must fail before the ped chain
        // copies its identity or any enclosing factory publishes the wrapper.
        if (!pools.HasNativeOwner(owner))
            throw std::invalid_argument("CPlayerPedCompositionDE requires its native owner's pool");
        return owner;
    }

    CPlayerPedCompositionDE::CPlayerPedCompositionDE(NativeGameTimingAccess& owner, const NativePedReferenceToken& token, CGame& game, CPoolsDE& pools,
                                                     CWanted* wanted, std::uint32_t wrapperType)
        : CPedRebuildDE(game, RequirePoolOwner(pools, owner), owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner,
                        owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, token, wrapperType, &owner),
          CPedPooledDE(pools, owner),
          CPlayerPedMoveAnimDE(wanted, owner, token)
    {
    }
}

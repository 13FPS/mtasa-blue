/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual player movement-group getter distinct from custom MTA policy
 ****************************************************************************/
#pragma once
#include "CPlayerPedWantedDE.h"
#include "NativePedMoveAnimAccess.h"
namespace mta::game_de
{
    class CPlayerPedMoveAnimDE : public CPlayerPedWantedDE
    {
    public:
        eMoveAnim GetMoveAnim() override;

    protected:
        // The enclosing factory supplies both borrowed CPU associations.
        // Copying the registration identity adds no native reference or
        // lookup and remains compatible with the shared virtual CPed chain.
        CPlayerPedMoveAnimDE(CWanted* wanted, NativePedMoveAnimAccess& access, const NativePedReferenceToken& token) noexcept
            : CPlayerPedWantedDE(wanted), native(access), reference(token)
        {
        }

    private:
        NativePedMoveAnimAccess&      native;
        const NativePedReferenceToken reference;
    };
    // SetMoveAnim, SetInitialState and targeting retain their mandatory real
    // contracts. Current group storage is not custom-override bookkeeping or
    // proof of block refs, loaded associations, reapply and anti-override hooks.
    static_assert(std::is_abstract_v<CPlayerPedMoveAnimDE> && std::is_base_of_v<CPlayerPed, CPlayerPedMoveAnimDE>);
}

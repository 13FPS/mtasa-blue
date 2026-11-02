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
    class NativeGameTimingAccess;
    class CAnimManagerDE;
    class CPedStateDE;
    class CPlayerPedMoveAnimDE : public CPlayerPedWantedDE
    {
    public:
        eMoveAnim GetMoveAnim() override;
        void      SetMoveAnim(eMoveAnim) override;

    protected:
        // The enclosing factory supplies both borrowed CPU associations.
        // Copying the registration identity adds no native reference or
        // lookup and remains compatible with the shared virtual CPed chain.
        CPlayerPedMoveAnimDE(CWanted* wanted, NativePedMoveAnimAccess& access, const NativePedReferenceToken& token) noexcept
            : CPlayerPedWantedDE(wanted), native(access), reference(token)
        {
        }

        // The complete enclosing wrapper lends the existing state branch and
        // an explicitly configured same-owner manager. It publishes no factory.
        CPlayerPedMoveAnimDE(CWanted*, NativeGameTimingAccess&, const NativePedReferenceToken&, CAnimManagerDE&, CPedStateDE&);
        void ConfigureMoveAnim(NativeGameTimingAccess&, CAnimManagerDE&, CPedStateDE&);

    private:
        void                          RequireMoveAnimServices(const char*) const;
        NativeGameTimingAccess*       movementOwner = nullptr;
        CAnimManagerDE*               animationManager = nullptr;
        CPedStateDE*                  customState = nullptr;
        NativePedMoveAnimAccess&      native;
        const NativePedReferenceToken reference;
    };
    // The configured setter reuses the actual MTA policy and native owner.
    // Initial state, targeting and the other incomplete SDK contracts stay pure;
    // this method does not complete or publish a player factory.
    static_assert(std::is_abstract_v<CPlayerPedMoveAnimDE> && std::is_base_of_v<CPlayerPed, CPlayerPedMoveAnimDE>);
}

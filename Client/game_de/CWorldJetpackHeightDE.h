/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's jetpack-height getter on the existing world SDK chain
 *****************************************************************************/
#pragma once
#include "CWorldAreaDE.h"
#include "NativeWorldJetpackHeightAccess.h"
namespace mta::game_de
{
    class CWorldJetpackHeightDE : public CWorldAreaDE
    {
    public:
        float GetJetpackMaxHeight() override;

    protected:
        CWorldJetpackHeightDE(NativeWorldGroundAccess& ground, NativeWorldLineAccess& line, NativeWorldAreaAccess& area,
                              NativeWorldJetpackHeightAccess& jetpack) noexcept
            : CWorldAreaDE(ground, line, area), jetpackOwner(jetpack)
        {
        }

    private:
        NativeWorldJetpackHeightAccess& jetpackOwner;
    };
    // A qualified native consumer value supplies only this getter. The real
    // setter remains pure: writing shared RData or patching an operand is not
    // an implementation of MTA's mutable world-height contract.
    static_assert(std::is_abstract_v<CWorldJetpackHeightDE> && std::is_base_of_v<CWorldAreaDE, CWorldJetpackHeightDE>);
}

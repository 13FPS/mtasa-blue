/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse SDK DisableVSync through a genuine Core presentation client
 *****************************************************************************/
#pragma once
#include "CGameDE.h"
#include "NativePresentationPolicyAccess.h"
#include <utility>

namespace mta::game_de
{
    // This branch implements one SDK obligation only. Its still-pure siblings
    // keep the full CGame factory unavailable; no fallback provider is made.
    template <typename GameBase>
    class CGamePresentationPolicyMixinDE : public GameBase
    {
    public:
        void DisableVSync() override
        {
            // The existing void SDK call must not silently claim an applied
            // policy after a stale/refused handoff. Success accepts deferred
            // intent on the installed owner, which applies at its native edge.
            if (presentation.DisableVSync() != Status::Ok)
                throw std::runtime_error("DisableVSync requires the current Core presentation client");
        }

    protected:
        template <typename... Arguments>
        CGamePresentationPolicyMixinDE(const NativePresentationPolicyBorrow& borrow, Arguments&&... arguments)
            : GameBase(std::forward<Arguments>(arguments)...), presentation(borrow, this)
        {
        }

    private:
        NativePresentationPolicyAccess presentation;
    };
    using CGamePresentationPolicyDE = CGamePresentationPolicyMixinDE<CGameDE>;
    static_assert(std::is_abstract_v<CGamePresentationPolicyDE> && std::is_base_of_v<CGame, CGamePresentationPolicyDE>);
}

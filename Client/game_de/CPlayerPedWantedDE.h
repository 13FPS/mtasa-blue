/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the player's explicitly supplied borrowed wanted SDK view
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <SharedUtil.Defines.h>
// Keep the common SDK's legacy forward bone enum warning local, as for the
// existing standalone ped layers; do not change the shared SDK declaration.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CPlayerPed.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <type_traits>

namespace mta::game_de
{
    class CPlayerPedWantedDE : public virtual CPlayerPed
    {
    public:
        CWanted* GetWanted() override { return wantedView; }

    protected:
        // Classic GetWanted only returns its initialized CPU association. The
        // enclosing factory must explicitly choose the appropriate nullable
        // view and keep it alive; selecting it grants no native wanted lease.
        explicit CPlayerPedWantedDE(CWanted* view) noexcept : wantedView(view) {}

    private:
        // Borrowed only: never resolve a global local-player wanted object for
        // an arbitrary player, invoke its methods, or delete it on retirement.
        CWanted* const wantedView;
    };
    // Initial state, movement animation, targeting and the remaining CPed
    // methods still belong to the future complete player implementation.
    static_assert(std::is_abstract_v<CPlayerPedWantedDE> && std::is_base_of_v<CPlayerPed, CPlayerPedWantedDE>);
}

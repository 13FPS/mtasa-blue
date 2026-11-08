/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's IPL order and copied-name filter on the same owner
 ****************************************************************************/
#include "CGameWorldIplDE.h"
namespace mta::game_de
{
    void CGameWorldIplDE::DisableWorldIplStreaming()
    {
        RequireNativeWrite(NativeWrites().DisableWorldIplStreaming(), "disable world IPL streaming");
    }
    void CGameWorldIplDE::EnableWorldIplStreaming(MtaGame::CGameWorldPolicy::IplFilter filter)
    {
        // The shared CGame policy selects this filter after restoring its pools.
        // No facade-local IPL list or successful empty fallback can replace it.
        RequireNativeWrite(NativeWrites().EnableWorldIplStreaming(filter), "enable world IPL streaming");
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route the shared MTA world policy through its native Cover owner
 ****************************************************************************/
#include "CGameWorldCoverDE.h"

namespace mta::game_de
{
    void CGameWorldCoverDE::RemoveWorldCovers()
    {
        // The native owner admits the complete table/list/reference itinerary
        // before the first effect and retains any accepted partial execution.
        RequireNativeWrite(NativeWrites().RemoveAllWorldCovers(), "remove world covers");
    }
}

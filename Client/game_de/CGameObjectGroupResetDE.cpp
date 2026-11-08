/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Use the model setters' retained default map in MTA's actual Reset
 ****************************************************************************/
#include "CGameObjectGroupResetDE.h"

namespace mta::game_de
{
    void CGameObjectGroupResetDE::RestoreObjectModelGroups()
    {
        // A separate facade-local map could acknowledge a reset while leaving
        // the real modified metadata and its first defaults unaccounted for.
        RequireNativeWrite(NativeWrites().RestoreAllModelObjectPropertiesGroups(), "restore object model properties groups");
    }
}

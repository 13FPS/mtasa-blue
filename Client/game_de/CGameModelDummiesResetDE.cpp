/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Restore actual dummy defaults before releasing their private model holds
 ****************************************************************************/
#include "CGameModelDummiesResetDE.h"
namespace mta::game_de
{
    void CGameModelDummiesResetDE::ResetVehicleModelDummies()
    {
        // The native owner holds the actual first defaults and restoration
        // intent; a new empty facade cache must never acknowledge this reset.
        RequireNativeWrite(NativeWrites().ResetAllModelVehicleDummies(), "ResetVehicleModelDummies");
    }
}

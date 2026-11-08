/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Use the model setters' retained wheel-default registry for game Reset
 ****************************************************************************/
#include "CGameModelWheelResetDE.h"
namespace mta::game_de
{
    void CGameModelWheelResetDE::ResetVehicleModelWheelSizes()
    {
        // The same enclosing native owner services model setters and Reset.
        // A facade-local empty cache would report success while leaving the
        // actual modified models and their original defaults unaccounted for.
        RequireNativeWrite(NativeWrites().ResetAllModelWheelSizes(), "reset vehicle model wheel sizes");
    }
}

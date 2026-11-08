/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reset the same first-default journal used by real model setters
 ****************************************************************************/
#include "CGameModelTimeResetDE.h"
namespace mta::game_de
{
    void CGameModelTimeResetDE::ResetModelTimes()
    {
        // An independent facade-local empty registry would leave modified
        // native hours behind. The enclosing owner retains every setter's
        // first tuple and acknowledged restoration prefix for this method.
        RequireNativeWrite(NativeWrites().ResetAllModelTimes(), "reset model times");
    }
}

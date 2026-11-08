/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Drain the real setters' ordered model-ID distance-default journal
 ****************************************************************************/
#include "CGameModelLodResetDE.h"
namespace mta::game_de
{
    void CGameModelLodResetDE::ResetModelLodDistances()
    {
        // A facade-local empty cache would leave actual edited metadata and
        // aliased model defaults behind. Use the same retained native owner
        // that remembers each SDK setter and acknowledges its reset prefix.
        RequireNativeWrite(NativeWrites().ResetAllModelLodDistances(), "reset model LOD distances");
    }
}

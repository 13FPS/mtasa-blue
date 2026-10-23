/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the actual classic gang-driveby task query order
 ****************************************************************************/
#include "CPedTasksDE.h"
#include <game/CPedTaskPolicy.h>
namespace mta::game_de
{
    bool CPedTasksDE::IsDoingGangDriveby() const
    {
        return MtaGame::CPedTaskPolicy::IsDoingGangDriveby(HasPedIntelligence(), [this] { return GetPedIntelligence(); });
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Return the current session's settings and camera pools through the game SDK
 ****************************************************************************/
#include "CGameClientServicesDE.h"
#include "CGameSettingsDE.h"
#include "CPoolsDE.h"

namespace mta::game_de
{
    CGameSettings* CGameClientServicesDE::GetSettings()
    {
        RequirePreparedServiceBinding("GetSettings");
        return &settings;
    }

    void CGameClientServicesDE::RequirePoolOwner() const
    {
        // Camera target lookup and the rest of the game must resolve a ped
        // through the same registry and native owner.
        RequireNativeWrite(pools.HasNativeOwner(NativeWrites()) ? ClockResult::Ok : ClockResult::OwnerConflict, "camera pool binding");
    }

    CPools* CGameClientServicesDE::GetPools() const noexcept
    {
        return PreparedServiceBindingStatus() == ClockResult::Ok ? &pools : nullptr;
    }
}

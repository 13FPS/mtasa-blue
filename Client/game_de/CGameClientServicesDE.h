/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share the session's settings and camera pools with the DE game
 ****************************************************************************/
#pragma once
#include "CGameCameraAudioDE.h"
#include "CCameraCamsDE.h"

namespace mta::game_de
{
    class CGameSettingsDE;

    class CGameClientServicesDE : public CGameCameraAudioDE
    {
    public:
        CGameSettings* GetSettings() override;
        CPools*        GetPools() const noexcept override;

    protected:
        // Core and Deathmatch retain the settings pointer. The session owner
        // must keep its settings and their Core profile alive through shutdown.
        template <typename... GameArgs>
        CGameClientServicesDE(CGameSettingsDE& settings, CCameraCamsDE& camera, CAudioEngineDE& audio, GameArgs&&... args)
            : CGameCameraAudioDE(camera, audio, std::forward<GameArgs>(args)...), settings(settings), pools(camera.GetCameraPools())
        {
            RequirePoolOwner();
        }

    private:
        void             RequirePoolOwner() const;
        CGameSettingsDE& settings;
        CPoolsDE&        pools;
    };

    using CGameClientPresentationDE = CGamePresentationPolicyMixinDE<CGameClientServicesDE>;
    static_assert(std::is_abstract_v<CGameClientPresentationDE> && std::is_convertible_v<CGameClientPresentationDE*, CGame*>);
}

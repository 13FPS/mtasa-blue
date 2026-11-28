/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bind the camera and audio engine for one DE client game
 ****************************************************************************/
#pragma once
#include "CGameModelLodResetDE.h"
#include "CGamePresentationPolicyDE.h"
#include <utility>

namespace mta::game_de
{
    class CAudioEngineDE;

    class CGameCameraAudioDE : public CGameModelLodResetDE
    {
    public:
        CCamera*         GetCamera() override;
        CAudioEngine*    GetAudioEngine() override;
        CAESoundManager* GetAESoundManager() override;

    protected:
        // Deathmatch retains the camera and registers callbacks on the audio engine.
        // The enclosing owner must keep both services alive until Deathmatch and
        // the game have retired. Neither SDK interface supports base deletion.
        template <typename... GameArgs>
        CGameCameraAudioDE(CCamera& camera, CAudioEngineDE& audioEngine, GameArgs&&... args)
            : CGameModelLodResetDE(std::forward<GameArgs>(args)...), camera(camera), audioEngine(audioEngine)
        {
        }

    private:
        CCamera&        camera;
        CAudioEngineDE& audioEngine;
    };

    // With a Core presentation borrow, VSync and the two client services share one CGame identity.
    using CGameCameraAudioPresentationDE = CGamePresentationPolicyMixinDE<CGameCameraAudioDE>;
    static_assert(std::is_abstract_v<CGameCameraAudioPresentationDE> && std::is_convertible_v<CGameCameraAudioPresentationDE*, CGame*>);
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Return the session's camera and audio engine through the game SDK
 ****************************************************************************/
#include "CGameCameraAudioDE.h"
#include "CAudioEngineDE.h"

namespace mta::game_de
{
    CCamera* CGameCameraAudioDE::GetCamera()
    {
        RequirePreparedServiceBinding("GetCamera");
        return &camera;
    }

    CAudioEngine* CGameCameraAudioDE::GetAudioEngine()
    {
        RequirePreparedServiceBinding("GetAudioEngine");
        return &audioEngine;
    }

    CAESoundManager* CGameCameraAudioDE::GetAESoundManager()
    {
        RequirePreparedServiceBinding("GetAESoundManager");
        return &audioEngine.GetSoundManager();
    }
}

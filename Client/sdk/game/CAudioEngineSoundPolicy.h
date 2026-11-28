/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Apply MTA's ambient settings and dispatch world sound events
 ****************************************************************************/
#pragma once
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include "CAudioEngine.h"

namespace MtaGame
{
    class CAudioEngineSoundPolicy : public CAudioEngine
    {
    public:
        void SetWorldSoundHandler(WorldSoundHandler* handler) override { worldSoundHandler = handler; }

        void PauseAmbientSounds(bool paused) override
        {
            ambientPaused = paused;
            UpdateAmbientSoundSettings();
        }

        void SetAmbientSoundEnabled(eAmbientSoundType type, bool enabled) override
        {
            if (type == AMBIENT_SOUND_GENERAL)
                ambientGeneralEnabled = enabled;
            else if (type == AMBIENT_SOUND_GUNFIRE)
                ambientGunfireEnabled = enabled;
            UpdateAmbientSoundSettings();
        }

        bool IsAmbientSoundEnabled(eAmbientSoundType type) override
        {
            if (ambientPaused)
                return false;
            if (type == AMBIENT_SOUND_GENERAL)
                return ambientGeneralEnabled;
            return type == AMBIENT_SOUND_GUNFIRE && ambientGunfireEnabled;
        }

        void ResetAmbientSounds() override
        {
            SetAmbientSoundEnabled(AMBIENT_SOUND_GENERAL, true);
            SetAmbientSoundEnabled(AMBIENT_SOUND_GUNFIRE, true);
        }

    protected:
        // Apply the effective settings to playback on the owning game thread.
        virtual void UpdateAmbientSoundSettings() = 0;

        template <class MakeEvent>
        bool DispatchWorldSound(uint group, uint index, MakeEvent&& makeEvent)
        {
            // Native sources may need to resolve an entity. Only request that
            // information after the mute check and when Deathmatch is listening.
            if (!IsWorldSoundEnabled(group, index))
                return false;
            return !worldSoundHandler || worldSoundHandler(makeEvent());
        }

    private:
        WorldSoundHandler* worldSoundHandler = nullptr;
        bool               ambientPaused = false;
        bool               ambientGeneralEnabled = true;
        bool               ambientGunfireEnabled = true;
    };
}

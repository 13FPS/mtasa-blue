/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Filter DE world sounds and deliver MTA's cancellable event
 ****************************************************************************/
#pragma once
#include <game/CAudioEngineSoundPolicy.h>
#include <game/CAESoundManager.h>
#include <limits>
#include <map>
#include <set>

namespace mta::game_de
{
    class CAudioEngineDE : public MtaGame::CAudioEngineSoundPolicy
    {
    public:
        CAESoundManager& GetSoundManager() const noexcept { return soundManager; }

        void SetWorldSoundEnabled(uint group, uint index, bool enabled, bool immediate) override
        {
            if (index == AllIndices)
            {
                if (enabled)
                    groups.erase(group);
                else
                    groups[group] = {false, {}};
            }
            else
            {
                auto it = groups.find(group);
                if (it == groups.end())
                {
                    if (enabled)
                        return;
                    it = groups.try_emplace(group).first;
                }
                Group& settings = it->second;
                if (enabled != settings.defaultEnabled)
                    settings.exceptions.insert(index);
                else
                    settings.exceptions.erase(index);
                if (settings.defaultEnabled && settings.exceptions.empty())
                    groups.erase(it);
            }

            // Also cancel already playing samples on repeated immediate mute
            // requests. The playback owner must implement this operation.
            if (!enabled && immediate)
                soundManager.CancelSoundsInBankSlot(group, index);
        }

        bool IsWorldSoundEnabled(uint group, uint index) override
        {
            const auto it = groups.find(group);
            if (it == groups.end())
                return true;
            const Group& settings = it->second;
            if (index == AllIndices)
                return settings.defaultEnabled && settings.exceptions.empty();
            return settings.defaultEnabled != (settings.exceptions.count(index) != 0);
        }

        void ResetWorldSounds() override { groups.clear(); }

        // Called for one resolved sample on the game thread before playback.
        // The source owns the entity's SDK identity and position for this call.
        bool CanPlayWorldSound(const SWorldSoundEvent& event)
        {
            return event.uiIndex != AllIndices && DispatchWorldSound(event.uiGroup, event.uiIndex, [&]() -> const SWorldSoundEvent& { return event; });
        }

    protected:
        explicit CAudioEngineDE(CAESoundManager& manager) noexcept : soundManager(manager) {}
        // Deathmatch registers one callback on the session's borrowed engine.
        CAudioEngineDE(const CAudioEngineDE&) = delete;
        CAudioEngineDE& operator=(const CAudioEngineDE&) = delete;

    private:
        static constexpr uint AllIndices = (std::numeric_limits<uint>::max)();

        // The SDK wildcard changes every index in a bank. Keep exceptions
        // with that bank so indices above 255 cannot affect its neighbours.
        struct Group
        {
            bool           defaultEnabled = true;
            std::set<uint> exceptions;
        };
        CAESoundManager&      soundManager;
        std::map<uint, Group> groups;
    };
}

/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share the existing model-wheel choice and default-cache itinerary
 *****************************************************************************/
#pragma once
#include <cstdint>

namespace MtaGame
{
    struct CModelWheelSizePolicy
    {
        static constexpr std::uint32_t Front = 0, Rear = 1, All = 0xff;
        template <class ReadFront, class ReadRear>
        static auto Get(std::uint32_t group, ReadFront&& front, ReadRear&& rear) -> decltype(front())
        {
            if (group == Front)
                return front();
            if (group == Rear)
                return rear();
            return {};
        }
        template <class Value, class WriteFront, class WriteRear>
        static bool Apply(std::uint32_t group, const Value& value, WriteFront&& front, WriteRear&& rear)
        {
            if (group == Front)
                front(value);
            else if (group == Rear)
                rear(value);
            else if (group == All)
            {
                front(value);
                rear(value);
            }
            else
                return false;
            return true;
        }
        template <class Map, class Key, class Capture>
        static auto& Remember(Map& defaults, const Key& key, Capture&& capture)
        {
            const auto found = defaults.find(key);
            if (found != defaults.end())
                return found->second;
            return defaults.emplace(key, capture()).first->second;
        }
        template <class Map, class Key, class Value>
        static bool Take(Map& defaults, const Key& key, Value& output)
        {
            const auto found = defaults.find(key);
            if (found == defaults.end())
                return false;
            // Copy while the node still owns its value. Keeping a pointer to
            // an erased default would turn the classic erase-before-store
            // order into a use-after-free rather than a restoration policy.
            output = found->second;
            defaults.erase(found);
            return true;
        }
        template <class Map, class Restore>
        static bool RestoreAll(Map& defaults, Restore&& restore)
        {
            for (auto& entry : defaults)
                if (!restore(entry.first, entry.second))
                    return false;
            // Actual SA keeps all defaults alive throughout its callbacks.
            // A qualified DE owner can retain an incomplete prefix instead
            // of erasing the evidence or replaying an uncertain native store.
            defaults.clear();
            return true;
        }
    };
}

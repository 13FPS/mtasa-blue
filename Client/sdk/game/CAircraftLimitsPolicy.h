/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA's aircraft limits and native consumer storage together
 ****************************************************************************/
#pragma once
#include <cstddef>
#include <type_traits>

namespace MtaGame
{
    class CAircraftLimitsPolicy final
    {
    public:
        // The real multiplayer module owns this aggregate for its whole
        // lifetime. Native instruction operands borrow these float addresses;
        // setters must update the existing cells rather than replace storage.
        struct State
        {
            float maxHeight;
            float maxVelocity;
            float maxVelocitySquared;
        };

        static void ResetToDefaults(State& state) noexcept
        {
            SetMaxHeight(state, 800.0f);
            SetMaxVelocity(state, 1.5f);
        }
        static float GetMaxHeight(const State& state) noexcept { return state.maxHeight; }
        static void  SetMaxHeight(State& state, float value) noexcept { state.maxHeight = value; }
        static float GetMaxVelocity(const State& state) noexcept { return state.maxVelocity; }
        static void  SetMaxVelocity(State& state, float value) noexcept
        {
            state.maxVelocity = value;
            // Classic binds one check to the square and three to the raw
            // value. Preserve this float product, order and compiler math;
            // SDK/network callers do not share the Lua front-end clamps.
            state.maxVelocitySquared = value * value;
        }
        static float* HeightAddress(State& state) noexcept { return &state.maxHeight; }
        static float* VelocityAddress(State& state) noexcept { return &state.maxVelocity; }
        static float* SquaredVelocityAddress(State& state) noexcept { return &state.maxVelocitySquared; }
    };
    // Replacing the original three adjacent floats must preserve both member
    // addresses and following private fields in the actual multiplayer owner.
    static_assert(std::is_aggregate_v<CAircraftLimitsPolicy::State> && std::is_standard_layout_v<CAircraftLimitsPolicy::State> &&
                  std::is_trivially_copyable_v<CAircraftLimitsPolicy::State>);
    static_assert(sizeof(float) == 4 && sizeof(CAircraftLimitsPolicy::State) == 3 * sizeof(float));
    static_assert(alignof(CAircraftLimitsPolicy::State) == alignof(float));
    static_assert(offsetof(CAircraftLimitsPolicy::State, maxHeight) == 0 && offsetof(CAircraftLimitsPolicy::State, maxVelocity) == sizeof(float) &&
                  offsetof(CAircraftLimitsPolicy::State, maxVelocitySquared) == 2 * sizeof(float));
}

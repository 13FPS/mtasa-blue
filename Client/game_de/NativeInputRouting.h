/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route canonical MTA movement only inside an owned native context
 *****************************************************************************/

#pragma once

#include "../sdk/game/CGameAdapter.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mta::game_de::NativeInputRouting
{
    struct ActionSample
    {
        float        value = 0;
        std::int32_t state = 1;
    };

    constexpr bool QueryDomain(std::uint32_t action) noexcept
    {
        return action <= 0xff;
    }

    inline bool ValidateMovementController(const CControllerState& state) noexcept
    {
        // Preserve the actual MTA synthesis contract (digital 0/255). Quietly
        // accepting unrelated controls would turn this movement trial into a
        // partially implemented full input backend.
        return state.LeftStickX >= -128 && state.LeftStickX <= 128 && state.LeftStickY >= -128 && state.LeftStickY <= 128 &&
               (state.ButtonCross == 0 || state.ButtonCross == 255) && (state.m_bPedWalk == 0 || state.m_bPedWalk == 255) && state.RightStickX == 0 &&
               state.RightStickY == 0 && state.LeftShoulder1 == 0 && state.LeftShoulder2 == 0 && state.RightShoulder1 == 0 && state.RightShoulder2 == 0 &&
               state.DPadUp == 0 && state.DPadDown == 0 && state.DPadLeft == 0 && state.DPadRight == 0 && state.Start == 0 && state.Select == 0 &&
               state.ButtonSquare == 0 && state.ButtonTriangle == 0 && state.ButtonCircle == 0 && state.ShockButtonL == 0 && state.ShockButtonR == 0 &&
               state.m_bChatIndicated == 0 && state.m_bVehicleMouseLook == 0 && state.m_bRadioTrackSkip == 0;
    }

    inline bool ValidateMovementState(const MtaInputState& state) noexcept
    {
        return ValidateMovementController(state.current) && ValidateMovementController(state.previous);
    }

    constexpr std::int32_t EdgeState(bool current, bool previous) noexcept
    {
        return current ? (previous ? 3 : 2) : (previous ? 0 : 1);
    }

    inline bool QueryAction(const MtaInputState& input, std::uint32_t action, ActionSample& output) noexcept
    {
        if (!QueryDomain(action) || !ValidateMovementState(input))
            return false;
        ActionSample next;
        if (action == 0x1e || action == 0x1f)
        {
            const auto current = action == 0x1e ? input.current.LeftStickX : input.current.LeftStickY;
            const auto previous = action == 0x1e ? input.previous.LeftStickX : input.previous.LeftStickY;
            next.value = std::clamp(static_cast<float>(current) / 128.0f, -1.0f, 1.0f);
            next.state = EdgeState(current != 0, previous != 0);
        }
        else if (action == 2)
        {
            next.value = input.current.ButtonCross != 0 ? 1.0f : 0.0f;
            next.state = EdgeState(input.current.ButtonCross != 0, input.previous.ButtonCross != 0);
        }
        // Membership/availability must route every valid action authoritatively
        // while the caller's scoped provider is active. Unknown actions remain
        // neutral instead of leaking physical mouse/touch fallback. In particular
        // native walk action0x6D stays neutral: canonical m_bPedWalk already
        // supplies MTA's walk policy, so applying another scale is incorrect.
        output = next;
        return true;
    }

    using PadStateBytes = std::array<std::byte, 96>;

    class PadTransaction final
    {
    public:
        PadTransaction() = default;
        PadTransaction(const PadTransaction&) = delete;
        PadTransaction& operator=(const PadTransaction&) = delete;

        bool Prepare(const PadStateBytes& physical, const MtaInputState& remote, PadStateBytes& replacement) noexcept
        {
            static_assert(sizeof(CControllerState) == 48);
            if (active || !ValidateMovementState(remote))
                return false;
            PadStateBytes next;
            std::memcpy(next.data(), &remote.current, sizeof(remote.current));
            std::memcpy(next.data() + sizeof(remote.current), &remote.previous, sizeof(remote.previous));
            saved = physical;
            active = true;
            replacement = next;
            return true;
        }

        bool RestoreBytes(PadStateBytes& output) const noexcept
        {
            if (!active)
                return false;
            // Native simulation may legitimately change the temporary pad. The
            // original physical snapshot must still be available after any
            // partial/failed apply or callback, not conditional on applied bytes.
            output = saved;
            return true;
        }

        bool ConfirmRestored(const PadStateBytes& observed) noexcept
        {
            if (!active || observed != saved)
                return false;
            // A failed read/write/readback retains ownership and its snapshot
            // for caller-controlled recovery. Never silently discard the only
            // restore copy or accept a nested context while it is pending.
            saved.fill(std::byte{0});
            active = false;
            return true;
        }

        bool Active() const noexcept { return active; }

    private:
        PadStateBytes saved{};
        bool          active = false;
    };
}

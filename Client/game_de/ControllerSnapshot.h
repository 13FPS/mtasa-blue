/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Publish bounded input observations using MTA's canonical control state
 *****************************************************************************/

#pragma once

#include <CControllerState.h>

#include <array>
#include <cstdint>
#include <limits>

namespace deonline
{
    class ControllerSnapshot
    {
    public:
        static constexpr std::uint32_t NativeFieldCount = 20;
        static constexpr std::uint32_t CompleteMask = 0xfffffu;
        static_assert(sizeof(signed short) == 2, "The shared MTA control contract requires int16 fields");

        // Observations arrive field by field through script FFI. A complete
        // transaction keeps a failed native read from publishing mixed samples
        // or advancing the previous MTA controller state prematurely.
        bool Begin() noexcept
        {
            Abort();
            active = true;
            return true;
        }

        bool Set(std::uint32_t index, std::int32_t value) noexcept
        {
            if (!active || failed)
                return false;
            if (index >= NativeFieldCount || value < std::numeric_limits<signed short>::min() || value > std::numeric_limits<signed short>::max())
            {
                failed = true;
                pending = CControllerState();
                pendingMask = 0;
                return false;
            }
            pending.*(fields[index]) = static_cast<signed short>(value);
            pendingMask |= 1u << index;
            return true;
        }

        bool Commit() noexcept
        {
            if (!active || failed || pendingMask != CompleteMask)
            {
                Abort();
                return false;
            }
            previous = current;
            previousMask = currentMask;
            current = pending;
            currentMask = pendingMask;
            Abort();
            return true;
        }

        // MTA's module receives one typed observation instead of twenty FFI
        // writes. Publish through the same transaction/mapping so both owners
        // preserve signed values and the same previous-sample coverage rules.
        bool Publish(const CControllerState& state) noexcept
        {
            Begin();
            for (std::uint32_t index = 0; index < NativeFieldCount; ++index)
            {
                if (!Set(index, state.*(fields[index])))
                    return false;
            }
            return Commit();
        }

        std::uint32_t Read(std::uint32_t which, std::uint32_t index) const noexcept
        {
            if (which > 1 || index >= NativeFieldCount)
                return 0;
            const auto mask = which == 0 ? currentMask : previousMask;
            if (!(mask & (1u << index)))
                return 0;
            const auto& state = which == 0 ? current : previous;
            // Return sign-extended int32 bits rather than a zero-extended short:
            // script FFI can recover negative analog input with a signed cast.
            return static_cast<std::uint32_t>(static_cast<std::int32_t>(state.*(fields[index])));
        }

        const CControllerState& Current() const noexcept { return current; }
        const CControllerState& Previous() const noexcept { return previous; }
        std::uint32_t           CurrentMask() const noexcept { return currentMask; }
        std::uint32_t           PreviousMask() const noexcept { return previousMask; }

        void Reset() noexcept
        {
            current = CControllerState();
            previous = CControllerState();
            currentMask = 0;
            previousMask = 0;
            Abort();
        }

    private:
        void Abort() noexcept
        {
            pending = CControllerState();
            pendingMask = 0;
            active = false;
            failed = false;
        }

        // A typed mapping preserves the existing MTA class instead of treating
        // separate members as an array. Only the twenty native pad controls are
        // observed; zero storage in the four MTA logical extras is not evidence.
        inline static constexpr std::array<signed short CControllerState::*, NativeFieldCount> fields = {
            &CControllerState::LeftStickX,    &CControllerState::LeftStickY,    &CControllerState::RightStickX,    &CControllerState::RightStickY,
            &CControllerState::LeftShoulder1, &CControllerState::LeftShoulder2, &CControllerState::RightShoulder1, &CControllerState::RightShoulder2,
            &CControllerState::DPadUp,        &CControllerState::DPadDown,      &CControllerState::DPadLeft,       &CControllerState::DPadRight,
            &CControllerState::Start,         &CControllerState::Select,        &CControllerState::ButtonSquare,   &CControllerState::ButtonTriangle,
            &CControllerState::ButtonCross,   &CControllerState::ButtonCircle,  &CControllerState::ShockButtonL,   &CControllerState::ShockButtonR};

        CControllerState current;
        CControllerState previous;
        CControllerState pending;
        std::uint32_t    currentMask = 0;
        std::uint32_t    previousMask = 0;
        std::uint32_t    pendingMask = 0;
        bool             active = false;
        bool             failed = false;
    };
}

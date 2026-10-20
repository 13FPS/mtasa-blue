/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/
#pragma once

#include <cstdint>

namespace deonline
{
    // The native controller and its owned fixtures share the exact key policy.
    // A fresh foreground/release boundary prevents startup-held keys or an
    // Alt-Tab return from accidentally opening a menu or stopping its owner.
    class NativeFrontendActivation
    {
    public:
        enum class Action : std::uint32_t
        {
            None,
            Toggle,
            Stop
        };
        static constexpr std::uint32_t Control = 1, Shift = 2, F9 = 4, End = 8;
        Action                         Update(bool focused, std::uint32_t keys, std::uint64_t now) noexcept
        {
            if (!focused || keys > 15 || (seen && now < previous))
            {
                Reset();
                return Action::None;
            }
            seen = true;
            previous = now;
            if (!armed)
            {
                armed = keys == 0;
                return Action::None;
            }
            if ((keys & (Control | Shift | End)) == (Control | Shift | End))
            {
                toggleHeld = toggleReady = false;
                if (!stopHeld)
                {
                    stopHeld = true;
                    heldAt = now;
                }
                if (now - heldAt >= 3000)
                {
                    Reset();
                    return Action::Stop;
                }
                return Action::None;
            }
            stopHeld = false;
            if (keys == (Control | Shift | F9))
            {
                if (!toggleHeld)
                {
                    toggleHeld = true;
                    heldAt = now;
                }
                toggleReady = toggleReady || now - heldAt >= 3000;
                return Action::None;
            }
            toggleHeld = false;
            if (toggleReady && !keys)
            {
                toggleReady = false;
                return Action::Toggle;
            }
            // Modifier release may span several Windows samples. Once the
            // full chord has qualified, admit only its eventual neutral edge.
            if (keys & End)
                toggleReady = false;
            return Action::None;
        }

    private:
        void          Reset() noexcept { armed = seen = toggleHeld = toggleReady = stopHeld = false; }
        bool          armed = false, seen = false, toggleHeld = false, toggleReady = false, stopHeld = false;
        std::uint64_t heldAt = 0, previous = 0;
    };
}

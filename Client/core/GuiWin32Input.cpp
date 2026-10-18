/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Translate Windows input without importing classic Core lifetimes
 *****************************************************************************/
#include "GuiWin32Input.h"

#include <windowsx.h>
#include <dinput.h>
#include <algorithm>
#include <SharedUtil.Defines.h>
#include <SString.h>
#include <gui/CGUI.h>

namespace GuiWin32Input
{
    namespace
    {
        Target GuiTarget(CGUI& gui) noexcept
        {
            static_assert(LeftButton == 0 && RightButton == 1 && MiddleButton == 2 && X1Button == 3 && X2Button == 4);
            return {&gui,
                    [](void* context, unsigned long x, unsigned long y) { static_cast<CGUI*>(context)->ProcessMouseInput(CGUI_MI_MOUSEPOS, x, y); },
                    [](void* context, std::uint32_t button, bool down)
                    { static_cast<CGUI*>(context)->ProcessMouseInput(down ? CGUI_MI_MOUSEDOWN : CGUI_MI_MOUSEUP, 0, 0, static_cast<CGUIMouseButton>(button)); },
                    [](void* context, bool up) { static_cast<CGUI*>(context)->ProcessMouseInput(CGUI_MI_MOUSEWHEEL, up ? 1 : 0, 0); },
                    [](void* context, unsigned long key, bool down) { static_cast<CGUI*>(context)->ProcessKeyboardInput(key, down); },
                    [](void* context, unsigned long character) { static_cast<CGUI*>(context)->ProcessCharacter(character); },
                    [](void* context) { static_cast<CGUI*>(context)->ClearSystemKeys(); },
                    [](void* context) { static_cast<CGUI*>(context)->ProcessMouseInput(CGUI_MI_MOUSECANCEL); }};
        }

        bool Valid(const Target& target) noexcept
        {
            return target.context && target.position && target.button && target.wheel && target.key && target.character && target.clearSystemKeys &&
                   target.cancelMouse;
        }
    }

    bool Translator::BindOwner() noexcept
    {
        const auto current = GetCurrentThreadId();
        DWORD      expected = 0;
        ownerThread.compare_exchange_strong(expected, current);
        if (ownerThread.load() == current)
            return true;
        rejectedWrongThread.fetch_add(1);
        return false;
    }

    StateSnapshot Translator::Snapshot() const noexcept
    {
        StateSnapshot result;
        const auto    thread = ownerThread.load();
        if (thread && thread == GetCurrentThreadId())
        {
            result = state;
            result.pressedKeys = static_cast<std::uint32_t>(std::count(keys.begin(), keys.end(), true));
            result.pressedButtons = static_cast<std::uint32_t>(std::count(buttons.begin(), buttons.end(), true));
        }
        result.ownerThread = thread;
        result.rejectedWrongThread = rejectedWrongThread.load();
        return result;
    }

    void Translator::Character(const Target& target, unsigned long value)
    {
        target.character(target.context, value);
        ++state.injectedEvents;
    }

    void Translator::Key(const Target& target, unsigned long code, bool down)
    {
        // A CEGUI handler may throw after accepting a press. Record that hold
        // before dispatch so owner cleanup still releases it on the error path.
        if (down)
            keys[code] = true;
        target.key(target.context, code, down);
        keys[code] = down;
        ++state.injectedEvents;
    }

    void Translator::Button(const Target& target, std::uint32_t button, bool down)
    {
        if (down)
            buttons[button] = true;
        target.button(target.context, button, down);
        buttons[button] = down;
        ++state.injectedEvents;
    }

    bool Translator::Reset(CGUI& gui)
    {
        return Reset(GuiTarget(gui));
    }

    bool Translator::ResetButtons(CGUI& gui)
    {
        return ResetButtons(GuiTarget(gui));
    }

    bool Translator::ResetButtons(const Target& target)
    {
        if (!Valid(target) || !BindOwner())
            return false;
        const auto held = static_cast<std::uint32_t>(std::count(buttons.begin(), buttons.end(), true));
        if (held)
        {
            // A fabricated mouse-up over a pressed widget could click it. Use
            // MTA's CEGUI cancellation path, which retires capture and click
            // tracking without completing an action or releasing keyboard keys.
            target.cancelMouse(target.context);
            buttons.fill(false);
            ++state.injectedEvents;
            state.syntheticReleases += held;
        }
        return true;
    }

    bool Translator::Reset(const Target& target)
    {
        if (!Valid(target) || !BindOwner())
            return false;
        // Focus/capture can disappear without matching key/button-up messages.
        // Release every injected hold while the existing GUI still exists.
        pendingHighSurrogate = 0;
        for (unsigned long code = 0; code < keys.size(); ++code)
        {
            if (keys[code])
            {
                Key(target, code, false);
                ++state.syntheticReleases;
            }
        }
        ResetButtons(target);
        // Classic IME handling remains in CLocalGUI and can inject independently.
        // Preserve its existing modifier cleanup as well as our complete holds.
        target.clearSystemKeys(target.context);
        return true;
    }

    bool Translator::Process(CGUI& gui, UINT message, WPARAM wparam, LPARAM lparam)
    {
        return Process(GuiTarget(gui), message, wparam, lparam);
    }

    bool Translator::ProcessConsumedRelease(CGUI& gui, UINT message, WPARAM wparam, LPARAM lparam)
    {
        return ProcessConsumedRelease(GuiTarget(gui), message, wparam, lparam);
    }

    bool Translator::ProcessConsumedRelease(const Target& target, UINT message, WPARAM wparam, LPARAM)
    {
        if (message != WM_KEYUP && message != WM_SYSKEYUP)
            return false;
        if (!Valid(target) || !BindOwner())
            return false;
        // Match the ordinary system-key policy: Windows still owns Alt and
        // its shortcuts, while Ctrl/Shift can have accepted GUI holds. A modal
        // capture must not strand those earlier presses in the translator.
        if (message == WM_SYSKEYUP && wparam != VK_CONTROL && wparam != VK_SHIFT)
            return true;
        const auto code = TranslateVirtualKey(static_cast<DWORD>(wparam));
        if (code && keys[code])
            // Key clears the hold only after successful dispatch. If CEGUI
            // throws, the same owner retains the obligation for cleanup/retry.
            Key(target, code, false);
        return true;
    }

    bool Translator::Process(const Target& target, UINT message, WPARAM wparam, LPARAM lparam)
    {
        switch (message)
        {
            case WM_KILLFOCUS:
            case WM_CANCELMODE:
            case WM_NCDESTROY:
                Reset(target);
                return false;
            case WM_CAPTURECHANGED:
                // An ordinary mouse-up can release Win32 capture while Shift
                // remains physically held. Capture alone must not release keys.
                ResetButtons(target);
                return false;
            case WM_ACTIVATE:
                if (LOWORD(wparam) == WA_INACTIVE)
                    Reset(target);
                return false;
            case WM_MOUSEWHEEL:
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
            case WM_UNICHAR:
                break;
            default:
                return false;
        }
        if (!Valid(target) || !BindOwner())
            return false;

        switch (message)
        {
            case WM_MOUSEWHEEL:
                // Preserve classic MTA's one signed wheel step per message.
                target.wheel(target.context, GET_WHEEL_DELTA_WPARAM(wparam) > 0);
                ++state.injectedEvents;
                return true;
            case WM_MOUSEMOVE:
                target.position(target.context, static_cast<unsigned long>(GET_X_LPARAM(lparam)), static_cast<unsigned long>(GET_Y_LPARAM(lparam)));
                ++state.injectedEvents;
                return true;
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                Button(target, 0, message == WM_LBUTTONDOWN);
                return true;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                Button(target, 1, message == WM_RBUTTONDOWN);
                return true;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                Button(target, 2, message == WM_MBUTTONDOWN);
                return true;
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                // Windows encodes which extra button in HIWORD, not the message
                // number. Mapping both buttons to X1 loses real MTA X2 events.
                Button(target, GET_XBUTTON_WPARAM(wparam) == XBUTTON2 ? 4 : 3, message == WM_XBUTTONDOWN);
                return true;
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                const auto code = TranslateVirtualKey(static_cast<DWORD>(wparam));
                if (code)
                    Key(target, code, message == WM_KEYDOWN);
                // Classic MTA consumes all key-down messages, but lets key-up
                // reach the game even when the GUI recognized the scan code.
                return message == WM_KEYDOWN;
            }
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                // Track modifiers released while Alt is held. Leave system
                // shortcuts with Windows, including Alt+F4 and Alt+Enter.
                if (wparam == VK_CONTROL || wparam == VK_SHIFT)
                    Key(target, TranslateVirtualKey(static_cast<DWORD>(wparam)), message == WM_SYSKEYDOWN);
                return false;
            case WM_UNICHAR:
                if (wparam == UNICODE_NOCHAR)
                    return true;
                if (pendingHighSurrogate)
                {
                    pendingHighSurrogate = 0;
                    Character(target, 0xfffd);
                }
                Character(target, wparam <= 0x10ffff && !(wparam >= 0xd800 && wparam <= 0xdfff) ? static_cast<unsigned long>(wparam) : 0xfffd);
                return true;
            case WM_CHAR:
            {
                const auto unit = static_cast<std::uint16_t>(wparam);
                // Unicode window messages carry UTF-16 code units; CEGUI takes
                // scalar characters. Never pass a lone surrogate into its fonts.
                if (pendingHighSurrogate)
                {
                    const auto high = pendingHighSurrogate;
                    pendingHighSurrogate = 0;
                    if (unit >= 0xdc00 && unit <= 0xdfff)
                    {
                        Character(target, 0x10000ul + ((high - 0xd800ul) << 10) + unit - 0xdc00ul);
                        return true;
                    }
                    Character(target, 0xfffd);
                }
                if (unit >= 0xd800 && unit <= 0xdbff)
                    pendingHighSurrogate = unit;
                else
                    Character(target, unit >= 0xdc00 && unit <= 0xdfff ? 0xfffd : unit);
                return true;
            }
        }
        return false;
    }

    DWORD Translator::TranslateVirtualKey(DWORD key) noexcept
    {
        switch (key)
        {
            case VK_HOME:
                return DIK_HOME;
            case VK_END:
                return DIK_END;
            case VK_RETURN:
                return DIK_RETURN;
            case VK_TAB:
                return DIK_TAB;
            case VK_BACK:
                return DIK_BACK;
            case VK_LEFT:
                return DIK_LEFTARROW;
            case VK_RIGHT:
                return DIK_RIGHTARROW;
            case VK_UP:
                return DIK_UPARROW;
            case VK_DOWN:
                return DIK_DOWNARROW;
            case VK_LSHIFT:
                return DIK_LSHIFT;
            case VK_RSHIFT:
                return DIK_RSHIFT;
            case VK_SHIFT:
                return DIK_LSHIFT;
            case VK_CONTROL:
                return DIK_LCONTROL;
            case VK_DELETE:
                return DIK_DELETE;
            case 0x56:
                return DIK_V;
            case 0x43:
                return DIK_C;
            case 0x58:
                return DIK_X;
            case 0x41:
                return DIK_A;
            default:
                return 0;
        }
    }
}

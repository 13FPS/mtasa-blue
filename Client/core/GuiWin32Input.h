/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share owner-thread Win32 translation into the existing MTA GUI
 *****************************************************************************/
#pragma once

#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>

class CGUI;

namespace GuiWin32Input
{
    struct StateSnapshot
    {
        std::uint32_t ownerThread = 0, pressedKeys = 0, pressedButtons = 0;
        std::uint64_t injectedEvents = 0, syntheticReleases = 0, rejectedWrongThread = 0;
    };

    // This non-owning injection target also permits translation tests without
    // constructing a renderer. The CGUI overload always uses its real input API;
    // it never calls widget callbacks or implements another GUI event system.
    struct Target
    {
        void* context = nullptr;
        void (*position)(void*, unsigned long, unsigned long) = nullptr;
        void (*button)(void*, std::uint32_t, bool) = nullptr;
        void (*wheel)(void*, bool) = nullptr;
        void (*key)(void*, unsigned long, bool) = nullptr;
        void (*character)(void*, unsigned long) = nullptr;
        void (*clearSystemKeys)(void*) = nullptr;
        void (*cancelMouse)(void*) = nullptr;
    };

    class Translator
    {
    public:
        Translator() noexcept = default;
        Translator(const Translator&) = delete;
        Translator& operator=(const Translator&) = delete;
        // Bind at the first real input/reset call, rather than assuming that
        // classic Core construction and Windows message dispatch share a thread.
        // GUI exceptions propagate to the owning lifecycle's existing handler.
        bool Process(CGUI& gui, UINT message, WPARAM wparam, LPARAM lparam);
        bool Reset(CGUI& gui);
        bool ResetButtons(CGUI& gui);
        bool Process(const Target& target, UINT message, WPARAM wparam, LPARAM lparam);
        bool Reset(const Target& target);
        bool ResetButtons(const Target& target);
        // A foreign observer gets only the atomic owner/rejection facts. GUI
        // holds and dispatch counters are readable solely by their owner.
        StateSnapshot Snapshot() const noexcept;

        // Keep the classic navigation/editing mapping. WM_CHAR carries text;
        // these are CEGUI's DirectInput scan codes, not Windows virtual keys.
        static DWORD TranslateVirtualKey(DWORD key) noexcept;

    private:
        bool BindOwner() noexcept;
        void Character(const Target& target, unsigned long value);
        void Key(const Target& target, unsigned long code, bool down);
        void Button(const Target& target, std::uint32_t button, bool down);

        StateSnapshot              state;
        std::atomic<DWORD>         ownerThread{0};
        std::atomic<std::uint64_t> rejectedWrongThread{0};
        std::array<bool, 256>      keys{};
        std::array<bool, 5>        buttons{};
        std::uint16_t              pendingHighSurrogate = 0;
    };
}

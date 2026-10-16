/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the shared Win32 translation and owner cleanup without a GUI
 *****************************************************************************/
#include "../../Client/core/GuiWin32Input.h"
#include <dinput.h>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }
    struct Event
    {
        char          kind;
        unsigned long value, other;
        bool          down;
    };
    struct Receiver
    {
        std::vector<Event>    events;
        bool                  throwPress = false, throwRelease = false, throwCancel = false;
        GuiWin32Input::Target Target()
        {
            return {this,
                    [](void* context, unsigned long x, unsigned long y) { static_cast<Receiver*>(context)->events.push_back({'p', x, y, false}); },
                    [](void* context, std::uint32_t button, bool down) { static_cast<Receiver*>(context)->events.push_back({'b', button, 0, down}); },
                    [](void* context, bool up) { static_cast<Receiver*>(context)->events.push_back({'w', 0, 0, up}); },
                    [](void* context, unsigned long key, bool down)
                    {
                        auto& receiver = *static_cast<Receiver*>(context);
                        receiver.events.push_back({'k', key, 0, down});
                        if ((down && receiver.throwPress) || (!down && receiver.throwRelease))
                            throw std::runtime_error("injected receiver exception");
                    },
                    [](void* context, unsigned long character) { static_cast<Receiver*>(context)->events.push_back({'c', character, 0, false}); },
                    [](void* context) { static_cast<Receiver*>(context)->events.push_back({'s', 0, 0, false}); },
                    [](void* context)
                    {
                        auto& receiver = *static_cast<Receiver*>(context);
                        receiver.events.push_back({'m', 0, 0, false});
                        if (receiver.throwCancel)
                            throw std::runtime_error("injected mouse cancellation exception");
                    }};
        }
    };
    void TestMapping()
    {
        const std::pair<DWORD, DWORD> mapping[] = {{VK_HOME, DIK_HOME},
                                                   {VK_END, DIK_END},
                                                   {VK_RETURN, DIK_RETURN},
                                                   {VK_TAB, DIK_TAB},
                                                   {VK_BACK, DIK_BACK},
                                                   {VK_LEFT, DIK_LEFTARROW},
                                                   {VK_RIGHT, DIK_RIGHTARROW},
                                                   {VK_UP, DIK_UPARROW},
                                                   {VK_DOWN, DIK_DOWNARROW},
                                                   {VK_LSHIFT, DIK_LSHIFT},
                                                   {VK_RSHIFT, DIK_RSHIFT},
                                                   {VK_SHIFT, DIK_LSHIFT},
                                                   {VK_CONTROL, DIK_LCONTROL},
                                                   {VK_DELETE, DIK_DELETE},
                                                   {'V', DIK_V},
                                                   {'C', DIK_C},
                                                   {'X', DIK_X},
                                                   {'A', DIK_A}};
        for (const auto& entry : mapping)
            Check(GuiWin32Input::Translator::TranslateVirtualKey(entry.first) == entry.second, "preserve every classic virtual-key mapping");
        Check(!GuiWin32Input::Translator::TranslateVirtualKey('B'), "ordinary text uses WM_CHAR rather than invented scan mappings");
        Check(!GuiWin32Input::Translator::TranslateVirtualKey(VK_F4), "system function key remains unmapped");
    }
    void TestClassicRouting()
    {
        Receiver                  receiver;
        const auto                target = receiver.Target();
        GuiWin32Input::Translator translator;
        Check(!translator.Process(target, WM_PAINT, 0, 0) && !translator.Snapshot().ownerThread, "unrelated messages do not acquire input ownership");
        Check(translator.Process(target, WM_MOUSEMOVE, 0, MAKELPARAM(19, 37)), "classic mouse movement is consumed");
        Check(receiver.events.back().kind == 'p' && receiver.events.back().value == 19 && receiver.events.back().other == 37,
              "forward client mouse coordinates");
        Check(translator.Process(target, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0) && receiver.events.back().down, "positive wheel step reaches MTA");
        Check(translator.Process(target, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), 0) && !receiver.events.back().down,
              "negative wheel step reaches MTA");
        Check(translator.Process(target, WM_KEYDOWN, 'B', 0), "unmapped classic key-down remains consumed");
        const auto before = receiver.events.size();
        Check(!translator.Process(target, WM_KEYUP, 'B', 0) && receiver.events.size() == before, "unmapped classic key-up remains unconsumed");
        Check(translator.Process(target, WM_KEYDOWN, VK_CONTROL, 0), "mapped classic key-down remains consumed");
        Check(translator.Process(target, WM_KEYDOWN, VK_CONTROL, LPARAM{1} << 30), "repeated key-down still reaches CEGUI");
        Check(translator.Snapshot().pressedKeys == 1, "repeat counts one held scan code");
        Check(!translator.Process(target, WM_KEYUP, VK_CONTROL, 0) && !translator.Snapshot().pressedKeys,
              "mapped classic key-up dispatches but remains unconsumed");
        Check(!translator.Process(target, WM_SYSKEYDOWN, VK_F4, 0), "Alt+F4 remains available to Windows");
        Check(!translator.Process(target, WM_SYSKEYDOWN, VK_RETURN, 0), "Alt+Enter remains available to Windows");
        Check(!translator.Process(target, WM_SYSKEYDOWN, VK_SHIFT, 0) && translator.Snapshot().pressedKeys == 1,
              "system modifier-down is tracked without consumption");
        Check(!translator.Process(target, WM_SYSKEYUP, VK_SHIFT, 0) && !translator.Snapshot().pressedKeys, "system modifier release prevents a stuck Shift");
        Check(translator.Snapshot().ownerThread == GetCurrentThreadId(), "actual dispatcher becomes the owner");
    }
    void TestMouseButtons()
    {
        Receiver                  receiver;
        const auto                target = receiver.Target();
        GuiWin32Input::Translator translator;
        const UINT                downs[] = {WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN, WM_XBUTTONDOWN, WM_XBUTTONDOWN};
        const UINT                ups[] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP, WM_XBUTTONUP, WM_XBUTTONUP};
        for (unsigned index = 0; index < 5; ++index)
        {
            const auto wparam = index < 3 ? WPARAM{0} : MAKEWPARAM(0, index == 3 ? XBUTTON1 : XBUTTON2);
            Check(translator.Process(target, downs[index], wparam, 0), "each mouse button-down is consumed");
            Check(receiver.events.back().kind == 'b' && receiver.events.back().value == index && receiver.events.back().down,
                  "extra button identity preserves X2 instead of aliasing X1");
            Check(translator.Snapshot().pressedButtons == index + 1, "track each button independently");
        }
        for (unsigned index = 0; index < 5; ++index)
        {
            const auto wparam = index < 3 ? WPARAM{0} : MAKEWPARAM(0, index == 3 ? XBUTTON1 : XBUTTON2);
            Check(translator.Process(target, ups[index], wparam, 0) && receiver.events.back().value == index && !receiver.events.back().down,
                  "matching button-up uses the same MTA identity");
        }
        Check(!translator.Snapshot().pressedButtons && translator.Snapshot().injectedEvents == 10, "all button holds release normally");
    }
    void TestReset()
    {
        const UINT messages[] = {WM_KILLFOCUS, WM_CANCELMODE, WM_NCDESTROY, WM_ACTIVATE};
        for (const auto message : messages)
        {
            Receiver                  receiver;
            const auto                target = receiver.Target();
            GuiWin32Input::Translator translator;
            translator.Process(target, WM_KEYDOWN, VK_CONTROL, 0);
            translator.Process(target, WM_KEYDOWN, VK_RSHIFT, 0);
            translator.Process(target, WM_LBUTTONDOWN, 0, 0);
            translator.Process(target, WM_XBUTTONDOWN, MAKEWPARAM(0, XBUTTON2), 0);
            Check(!translator.Process(target, message, WA_INACTIVE, 0), "focus and lifetime messages stay available to their original handler");
            const auto after = translator.Snapshot();
            Check(!after.pressedKeys && !after.pressedButtons && after.syntheticReleases == 4 && after.injectedEvents == 7,
                  "reset releases keys and cancels all held mouse buttons once");
            Check(receiver.events[receiver.events.size() - 2].kind == 'm', "reset uses cancellation instead of a mouse-up that could complete a click");
            Check(receiver.events.back().kind == 's', "reset also retains classic independent modifier cleanup");
            Check(translator.Reset(target) && translator.Snapshot().syntheticReleases == 4, "repeated reset does not fabricate further held releases");
        }
        Receiver                  receiver;
        GuiWin32Input::Translator translator;
        translator.Process(receiver.Target(), WM_KEYDOWN, VK_CONTROL, 0);
        Check(!translator.Process(receiver.Target(), WM_ACTIVATE, WA_ACTIVE, 0) && translator.Snapshot().pressedKeys == 1,
              "activation does not drop a held modifier");
        translator.Process(receiver.Target(), WM_LBUTTONDOWN, 0, 0);
        Check(!translator.Process(receiver.Target(), WM_CAPTURECHANGED, 0, 0) && translator.Snapshot().pressedKeys == 1 &&
                  !translator.Snapshot().pressedButtons && translator.Snapshot().syntheticReleases == 1,
              "mouse capture loss releases buttons without losing a held keyboard modifier");
        Check(receiver.events.back().kind == 'm', "button-only reset cancels capture without mouse-up or global modifier cleanup");
        translator.Reset(receiver.Target());
    }
    void TestCharacters()
    {
        Receiver                  receiver;
        const auto                target = receiver.Target();
        GuiWin32Input::Translator translator;
        Check(translator.Process(target, WM_CHAR, 'X', 0) && receiver.events.back().value == 'X', "ordinary WM_CHAR text reaches existing MTA input");
        const auto before = receiver.events.size();
        Check(translator.Process(target, WM_CHAR, 0xd83d, 0) && receiver.events.size() == before, "hold a high surrogate until its next character");
        Check(translator.Process(target, WM_CHAR, 0xde00, 0) && receiver.events.back().value == 0x1f600, "UTF-16 pair becomes one CEGUI scalar");
        translator.Process(target, WM_CHAR, 0xdc00, 0);
        Check(receiver.events.back().value == 0xfffd, "a lone low surrogate becomes a replacement character");
        translator.Process(target, WM_CHAR, 0xd800, 0);
        const auto broken = receiver.events.size();
        translator.Process(target, WM_CHAR, 'A', 0);
        Check(receiver.events.size() == broken + 2 && receiver.events[broken].value == 0xfffd && receiver.events[broken + 1].value == 'A',
              "broken pair does not discard following ordinary text");
        translator.Process(target, WM_CHAR, 0xd800, 0);
        translator.Reset(target);
        const auto reset = receiver.events.size();
        translator.Process(target, WM_CHAR, 'B', 0);
        Check(receiver.events.size() == reset + 1 && receiver.events.back().value == 'B', "reset discards a pending surrogate without inserting cleanup text");
        const auto probe = receiver.events.size();
        Check(translator.Process(target, WM_UNICHAR, UNICODE_NOCHAR, 0) && receiver.events.size() == probe, "Unicode support probe never injects text");
        translator.Process(target, WM_UNICHAR, 0x1f642, 0);
        Check(receiver.events.back().value == 0x1f642, "owned Unicode child accepts direct scalar text");
        translator.Process(target, WM_UNICHAR, 0x110000, 0);
        Check(receiver.events.back().value == 0xfffd, "invalid Unicode scalar is not passed to CEGUI");
    }
    void TestOwnerAndFailure()
    {
        Receiver                  receiver;
        GuiWin32Input::Translator translator;
        auto                      target = receiver.Target();
        auto                      invalid = target;
        invalid.character = nullptr;
        Check(!translator.Process(invalid, WM_CHAR, 'X', 0) && !translator.Reset(invalid) && !translator.Snapshot().ownerThread,
              "invalid injection target fails before acquiring ownership");
        translator.Process(target, WM_KEYDOWN, VK_CONTROL, 0);
        const auto                   before = receiver.events.size();
        bool                         foreignProcess = true, foreignReset = true;
        GuiWin32Input::StateSnapshot foreignSnapshot;
        std::thread                  foreign(
            [&]
            {
                foreignProcess = translator.Process(target, WM_CHAR, 'X', 0);
                foreignReset = translator.Reset(target);
                foreignSnapshot = translator.Snapshot();
            });
        foreign.join();
        Check(!foreignProcess && !foreignReset && receiver.events.size() == before, "wrong thread cannot dispatch or release owner GUI input");
        Check(translator.Snapshot().pressedKeys == 1 && translator.Snapshot().rejectedWrongThread == 2,
              "wrong-thread rejection preserves owned holds and is reported");
        Check(foreignSnapshot.ownerThread == GetCurrentThreadId() && foreignSnapshot.rejectedWrongThread == 2 && !foreignSnapshot.pressedKeys &&
                  !foreignSnapshot.injectedEvents && !foreignSnapshot.syntheticReleases,
              "foreign snapshot returns atomic facts without reading owner input arrays or counters");
        receiver.throwRelease = true;
        bool threw = false;
        try
        {
            translator.Reset(target);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && translator.Snapshot().pressedKeys == 1, "a failed release retains its cleanup obligation");
        receiver.throwRelease = false;
        Check(translator.Reset(target) && !translator.Snapshot().pressedKeys, "owner can retry release after dispatch failure");
        receiver.throwPress = true;
        threw = false;
        try
        {
            translator.Process(target, WM_KEYDOWN, VK_RSHIFT, 0);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && translator.Snapshot().pressedKeys == 1, "a handler throwing after a press cannot strand an untracked hold");
        receiver.throwPress = false;
        Check(translator.Reset(target) && !translator.Snapshot().pressedKeys, "owner cleanup releases the faulted press");
        translator.Process(target, WM_LBUTTONDOWN, 0, 0);
        receiver.throwCancel = true;
        threw = false;
        try
        {
            translator.ResetButtons(target);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && translator.Snapshot().pressedButtons == 1, "failed mouse cancellation retains its cleanup obligation");
        receiver.throwCancel = false;
        Check(translator.ResetButtons(target) && !translator.Snapshot().pressedButtons, "owner retries mouse cancellation without fabricating a mouse-up");
        const auto cancellations = receiver.events.size();
        Check(translator.ResetButtons(target) && receiver.events.size() == cancellations, "completed button-only cancellation is idempotent");
    }
}

int main()
{
    try
    {
        TestMapping();
        TestClassicRouting();
        TestMouseButtons();
        TestReset();
        TestCharacters();
        TestOwnerAndFailure();
        std::printf("Passed %u shared Win32 GUI input checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Win32 GUI input failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}

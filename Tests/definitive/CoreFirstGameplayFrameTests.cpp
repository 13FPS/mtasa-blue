// The runner imports the first-frame code and its per-Core state from the shipping sources.
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

struct Host
{
    std::string                           calls;
    std::function<void(std::string_view)> callback;
    void                                  Call(std::string_view stage)
    {
        if (!calls.empty())
            calls += ' ';
        calls.append(stage);
        if (callback)
            callback(stage);
    }
    static Host& GetSingleton();
    void         ValidateValues() { Call("validate"); }
    void         Connect() { Call("steam"); }
    void         SelectInputHandlers(int) { Call("input"); }
    bool         allowSteam = true;
    template <class T>
    T GetValue(const char*, T)
    {
        return static_cast<T>(allowSteam);
    }
    void Get(const char*, bool& enabled) { enabled = allowSteam; }
} host;
Host& Host::GetSingleton()
{
    return host;
}
using CClientVariables = Host;
#define CVARS_GET_VALUE        host.GetValue
#define CVARS_GET              host.Get
#define TIMING_CHECKPOINT(tag) ((void)0)
constexpr int INPUT_CORE = 0;

struct CCore
{
#include "FirstFrameState.generated.inl"
    Host* m_steamClient = &host;
    Host* m_pGUI = &host;
    bool  m_bQuitOnPulse = false, m_bDestroyMessageBox = false;
    void  Quit() { throw std::logic_error("unexpected Quit"); }
    void  RemoveMessageBox() { throw std::logic_error("unexpected RemoveMessageBox"); }
    void  ApplyConsoleSettings() { host.Call("console"); }
    void  ApplyGameSettings() { host.Call("game"); }
    void  DoPostFramePulse();
};
#include "FirstGameplayFrame.generated.inl"

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    std::string scenario = argv[1];
    CCore       core;
    std::string expected;
    if (scenario == "instances")
    {
        core.DoPostFramePulse();
        core.DoPostFramePulse();
        CCore next;
        next.DoPostFramePulse();
        expected = "validate console game steam input frame frame validate console game steam input frame";
    }
    else if (scenario == "steam-disabled")
    {
        host.allowSteam = false;
        core.DoPostFramePulse();
        core.DoPostFramePulse();
        expected = "validate console game input frame frame";
    }
    else if (scenario == "reentrant")
    {
        bool nested = false;
        host.callback = [&](std::string_view stage)
        {
            if (stage != "frame" && !nested)
            {
                nested = true;
                core.DoPostFramePulse();
                nested = false;
            }
        };
        core.DoPostFramePulse();
        expected = "validate console game steam input frame";
    }
    else
    {
        if (scenario == "validate")
            expected = "validate validate console game steam input frame frame";
        else if (scenario == "console")
            expected = "validate console console game steam input frame frame";
        else if (scenario == "game")
            expected = "validate console game game steam input frame frame";
        else if (scenario == "steam")
            expected = "validate console game steam input frame frame";
        else if (scenario == "input")
            expected = "validate console game steam input input frame frame";
        else
            return 2;
        host.callback = [&](std::string_view stage)
        {
            if (stage == scenario)
                throw std::runtime_error(scenario);
        };
        try
        {
            core.DoPostFramePulse();
            return 2;
        }
        catch (const std::runtime_error& failure)
        {
            if (failure.what() != scenario)
                return 2;
        }
        host.callback = {};
        core.DoPostFramePulse();
        core.DoPostFramePulse();
    }
    if (host.calls != expected)
    {
        std::cerr << "Expected " << expected << ", observed " << host.calls << '\n';
        return 1;
    }
    return 0;
}

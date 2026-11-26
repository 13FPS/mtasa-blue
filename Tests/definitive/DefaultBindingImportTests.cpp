// Run the ordinary Core default importer with a recording SDK controller.
// The XML receiver represents an already parsed binds node. The configured
// input host separately exercises the production XML parser and key tables.
#include "CControllerConfigManager.h"
#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#ifdef _WIN32
    #define stricmp _stricmp
#else
    #include <strings.h>
    #define stricmp strcasecmp
#endif

namespace SystemState
{
    enum Enum
    {
        GS_LOADING,
        GS_FRONTEND,
        GS_PLAYING_GAME
    };
}

struct Controller : CControllerConfigManager
{
    int  reads = 0;
    bool interrupt = false;
    int  joystick = 32;
    int  GetControllerKeyAssociatedWithAction(eControllerAction action, eControllerType slot) override
    {
        ++reads;
        if (interrupt && action == SPRINT)
            throw std::runtime_error("controller unavailable");
        if (action == FORWARDS)
        {
            switch (slot)
            {
                case KEYBOARD:
                    return GTA_KEY_W;
                case OPTIONAL_EXTRA:
                    return GTA_KEY_UP;
                case MOUSE:
                    return GTA_KEY_LMOUSE;
                case JOYSTICK:
                    return joystick;
            }
        }
        return slot == JOYSTICK ? 0 : NO_KEY_DEFINED;
    }
    void  SetControllerKeyAssociatedWithAction(eControllerAction, int, eControllerType) override { std::abort(); }
    void  SetClassicControls(bool) override { std::abort(); }
    void  SetMouseInverted(bool) override { std::abort(); }
    void  SetFlyWithMouse(bool) override { std::abort(); }
    void  SetSteerWithMouse(bool) override { std::abort(); }
    void  SuspendSteerAndFlyWithMouse(bool) override { std::abort(); }
    float GetVerticalAimSensitivityRawValue() override { std::abort(); }
    void  SetVerticalAimSensitivityRawValue(float) override { std::abort(); }
    void  SetVerticalAimSensitivitySameAsHorizontal(bool) override { std::abort(); }
};

struct CGame
{
    SystemState::Enum         state = SystemState::GS_LOADING;
    Controller*               controller = nullptr;
    SystemState::Enum         GetSystemState() const { return state; }
    CControllerConfigManager* GetControllerConfigManager() const { return controller; }
};

struct CKeyBinds;
using CKeyBindsInterface = CKeyBinds;
struct Console
{
    std::string last;
    void        Print(const char* line) { last = line; }
};
struct CCore
{
    CGame*        game = nullptr;
    CKeyBinds*    binds = nullptr;
    Console       console;
    Console*      GetConsole() { return &console; }
    CKeyBinds*    GetKeyBinds() const { return binds; }
    CGame*        GetGame() const { return game; }
    static CCore& GetSingleton()
    {
        static CCore core;
        return core;
    }
};
struct CLocalGUI
{
    static bool IsFaultDialogOpen() { return false; }
};
struct CXMLNode
{
    unsigned int count = 0;
    unsigned int GetSubNodeCount() const { return count; }
};
#include "BindTypes.inl"
struct SDefaultCommandBind
{
    char szKey[20];
    bool bState;
    char szCommand[20], szArguments[20];
};
const SDefaultCommandBind g_dcbDefaultCommands[] = {{"t", true, "chatbox", "chatboxsay"}, {"", false, "", ""}};

#define GTA_KEY_JOY(x) (500 + (x))
SBindableGTAControl g_bcControls[] = {{"forwards", FORWARDS}, {"sprint", SPRINT}, {}};
const SBindableKey  g_bkKeys[] = {{"w", 0, GTA_KEY_W},
                                  {"arrow_u", 0, GTA_KEY_UP},
                                  {"mouse1", 0, GTA_KEY_LMOUSE},
                                  {"joy32", 0, GTA_KEY_JOY(32)},
                                  {"num_sep", 0, NO_KEY_DEFINED},
                                  {"a", 0, GTA_KEY_A},
                                  {"t", 0, GTA_KEY_T},
                                  {}};
struct CKeyBinds
{
    CKeyBinds(CCore* core = &CCore::GetSingleton()) : m_pCore(core) {}
    CCore* m_pCore;
    bool   m_bWaitingToLoadDefaults = false;
    bool   m_bProcessingKeyStroke = false;
    using KeyBindPtr = std::unique_ptr<CKeyBind>;
    std::list<KeyBindPtr> m_binds;
    using Row = std::pair<int, eControllerAction>;
    SBindableGTAControl* GetBindableFromAction(eControllerAction);
    const SBindableKey*  GetBindableFromGTARelative(int);
    const SBindableKey*  GetBindableFromKey(const char*) const;
    bool                 AddGTAControl(const SBindableKey*, SBindableGTAControl*);
    std::vector<Row>     Controls() const
    {
        std::vector<Row> result;
        for (const auto& bind : m_binds)
            if (bind->type == KeyBindType::GTA_CONTROL && !bind->isBeingDeleted)
                result.emplace_back(bind->boundKey->iGTARelative, static_cast<const CGTAControlBind*>(bind.get())->control->action);
        return result;
    }
    std::vector<std::string> Commands() const
    {
        std::vector<std::string> result;
        for (const auto& bind : m_binds)
            if (bind->type == KeyBindType::COMMAND && !bind->isBeingDeleted)
                result.push_back(static_cast<const CCommandBind*>(bind.get())->arguments);
        return result;
    }
    bool AddCommand(const char* key, const char* name, const char* args, bool down)
    {
        auto bind = std::make_unique<CCommandBind>();
        bind->boundKey = GetBindableFromKey(key);
        bind->command = name;
        bind->arguments = args;
        bind->triggerState = down;
        m_binds.emplace_back(std::move(bind));
        return true;
    }
    bool CommandExists(const char*, const char* name, bool, bool down, const char* args) const
    {
        for (const auto& bind : m_binds)
        {
            if (bind->type == KeyBindType::COMMAND && !bind->isBeingDeleted)
            {
                auto* cmd = static_cast<const CCommandBind*>(bind.get());
                if (cmd->command == name && cmd->triggerState == down && cmd->arguments == args)
                    return true;
            }
        }
        return false;
    }
    bool LoadConfiguredBindsFromXML(CXMLNode* node)
    {
        if (node && node->count)
            AddGTAControl(GetBindableFromKey("a"), GetBindableFromAction(SPRINT));
        return node != nullptr;
    }
    void ClearCommandsAndControls();
    bool RemoveAllGTAControls();
    enum class ImportMode
    {
        AppendControls,
        ReplaceControls,
        ResetDefaults
    };
    bool ImportGTAControls(ImportMode);
    void AddDefaultCommandsTo(CKeyBinds&, bool);
    bool HasUserCommand(const char*, const char*, bool, std::string_view) const;
    void LoadDefaultCommands(bool);
    void DoPostFramePulse();
    bool LoadFromXML(CXMLNode*);
    void LoadDefaultBinds();
    void LoadControlsFromGTA();
};

struct CCommandFuncs
{
    static void CopyGTAControls(const char*);
};
#define _(text) text
#include "DefaultBindingMethods.inl"

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::string name(argv[1]);
    Controller        controller;
    CGame             game{SystemState::GS_LOADING, &controller};
    CCore::GetSingleton().game = &game;
    CKeyBinds binds;
    CCore::GetSingleton().binds = &binds;
    CXMLNode                          empty, saved{1};
    const std::vector<CKeyBinds::Row> configured = {{GTA_KEY_A, SPRINT}};
    const std::vector<CKeyBinds::Row> defaults = {{GTA_KEY_W, FORWARDS}, {GTA_KEY_UP, FORWARDS}, {GTA_KEY_LMOUSE, FORWARDS}, {GTA_KEY_JOY(32), FORWARDS}};

    if (name == "stale-defaults")
    {
        binds.LoadFromXML(&empty);
        binds.LoadFromXML(&saved);
        game.state = SystemState::GS_FRONTEND;
        binds.DoPostFramePulse();
        return !(binds.Controls() == configured && controller.reads == 0);
    }
    if (name == "normal" || name == "invalid-joystick")
    {
        auto expected = defaults;
        if (name == "invalid-joystick")
        {
            controller.joystick = NO_KEY_DEFINED - 500;
            expected.pop_back();
        }
        binds.LoadFromXML(&empty);
        game.state = SystemState::GS_FRONTEND;
        binds.DoPostFramePulse();
        int reads = controller.reads;
        binds.DoPostFramePulse();
        return !(binds.Controls() == expected && binds.Commands() == std::vector<std::string>{"chatboxsay"} && reads > 0 && controller.reads == reads);
    }
    binds.LoadFromXML(&saved);
    binds.AddCommand("t", "chatbox", "custom", true);
    game.state = SystemState::GS_FRONTEND;
    if (name == "append-controls")
    {
        auto expected = configured;
        expected.insert(expected.end(), defaults.begin(), defaults.end());
        binds.LoadControlsFromGTA();
        return !(binds.Controls() == expected && binds.Commands() == std::vector<std::string>{"custom"});
    }
    if (name == "command-defaults")
    {
        game.state = SystemState::GS_LOADING;
        binds.LoadFromXML(&empty);
        binds.LoadDefaultCommands(false);
        game.state = SystemState::GS_FRONTEND;
        binds.DoPostFramePulse();
        binds.LoadDefaultBinds();
        return !(binds.Controls() == defaults && binds.Commands() == std::vector<std::string>{"custom", "chatboxsay"});
    }
    if (name == "copy-unavailable" || name == "copy-success")
    {
        bool available = name == "copy-success";
        if (!available)
            CCore::GetSingleton().game = nullptr;
        CCommandFuncs::CopyGTAControls("");
        return !(binds.Controls() == (available ? defaults : configured) && binds.Commands() == std::vector<std::string>{"custom"} &&
                 (CCore::GetSingleton().console.last == "Bound all controls from GTA") == available);
    }
    if (name == "missing-game")
        CCore::GetSingleton().game = nullptr;
    else if (name == "missing-controller")
        game.controller = nullptr;
    else if (name == "interrupted")
        controller.interrupt = true;
    else
        return 2;
    try
    {
        binds.LoadFromXML(&empty);
    }
    catch (const std::runtime_error&)
    {
        return 1;
    }
    if (binds.Controls() != configured || binds.Commands() != std::vector<std::string>{"custom"})
        return 1;
    CCore::GetSingleton().game = &game;
    game.controller = &controller;
    controller.interrupt = false;
    binds.DoPostFramePulse();
    return !(binds.Controls() == defaults && binds.Commands() == std::vector<std::string>{"custom", "chatboxsay"});
}

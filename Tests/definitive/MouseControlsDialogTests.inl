// Exercise the ordinary Settings button handlers and mouse sections of
// LoadData/SaveData with recording widgets and the same XML-backed CVARs.
namespace mouse_dialog
{
    struct Widget
    {
        bool               selected{}, enabled{true}, visible{true};
        float              position{};
        std::string        text{"player"};
        Widget*            operator->() { return this; }
        void               SetSelected(bool value) { selected = value; }
        bool               GetSelected() const { return selected; }
        void               SetEnabled(bool value) { enabled = value; }
        void               SetVisible(bool value) { visible = value; }
        float              GetScrollPosition() const { return position; }
        void               SetScrollPosition(float value) { position = value; }
        const std::string& GetText() const { return text; }
    };

    using CGUIElement = Widget;
    using CGameSettings = SettingsReceiver;
    struct CKeyBinds
    {
    };
    struct CMainMenu
    {
        bool m_bIsInSubWindow{true};
    };
    struct CLocalGUI : CSingleton<CLocalGUI>
    {
        CMainMenu  menu;
        CMainMenu* GetMainMenu() { return &menu; }
    };
    struct CCore : CSingleton<CCore>
    {
        Game*      game{};
        int        errors{};
        Game*      GetGame() { return game; }
        CKeyBinds* GetKeyBinds() { std::abort(); }
        bool       IsValidNick(const char* nick) { return *nick != '\0'; }
        void       ShowMessageBox(const char*, const char*, int) { ++errors; }
    };
    CCore* g_pCore = &CCore::GetSingleton();
    struct Joystick
    {
        void SetDefaults() {}
    };
    Joystick* GetJoystickManager()
    {
        static Joystick joystick;
        return &joystick;
    }
    struct CSettings
    {
        Widget m_pInvertMouse, m_pFlyWithMouse, m_pSteerWithMouse, m_pStandardControls, m_pClassicControls;
        Widget m_pMouseSensitivity, m_pVerticalAimSensitivity, m_pCheckboxVerticalAimSensitivity, m_pWindow, m_pEditNick;
        Widget m_pAudioMasterVolume, m_pAudioRadioVolume, m_pAudioSFXVolume, m_pAudioMTAVolume, m_pAudioVoiceVolume;
        float  m_fOldMasterVolume{0.2f}, m_fOldRadioVolume{}, m_fOldSFXVolume{}, m_fOldMTAVolume{0.3f}, m_fOldVoiceVolume{0.4f};
        float  m_fRadioVolume{0.25f}, m_fSFXVolume{0.75f};
        int    binds{}, joypad{}, saves{};
        bool   m_bCaptureKey{};
        bool   IsFrontendBindings() { return false; }
        void   SetVisible(bool visible) { m_pWindow.SetVisible(visible); }
        void   UpdateJoypadTab() {}
        bool   StageFrontendBindingEdits(CKeyBinds&) { std::abort(); }
        void   ProcessKeyBinds() { ++binds; }
        void   ProcessJoypad() { ++joypad; }
        void   LoadData()
        {
            using std::max;
            using std::min;
            // The preceding General tab can leave its most recent boolean set.
            [[maybe_unused]] bool bVar = true;
#include "DialogLoad.generated.inl"
        }
        void SaveData()
        {
            ++saves;
            auto* gameSettings = g_pCore->game->GetSettings();
#include "DialogSave.generated.inl"
        }
        bool OnControlsDefaultClick(CGUIElement*);
        bool OnOKButtonClick(CGUIElement*);
    };
    constexpr int MB_BUTTON_OK = 1, MB_ICON_INFO = 2;
#define _(value) value
#include "DialogButtons.generated.inl"
#undef _
}

bool TestMouseDialog(std::string_view name, Game& game)
{
    using mouse_dialog::g_pCore;
    g_pCore->game = &game;
    auto& c = game.controller;
    c.inverted = c.flight = c.steering = c.classic = c.sameAim = true;
    c.aim = 0.002f;
    mouse_dialog::CSettings ui;

    if (name == "controls-volumes")
    {
        ui.LoadData();
        return ui.m_pAudioMasterVolume.position == 0.0f && ui.m_pAudioRadioVolume.position == 0.25f && ui.m_pAudioSFXVolume.position == 0.75f &&
               ui.m_pAudioMTAVolume.position == 0.6f && ui.m_pAudioVoiceVolume.position == 1.0f;
    }
    if (name == "controls-numbers")
    {
        const auto loads = [&](float expected)
        {
            ui.LoadData();
            return std::abs(ui.m_pVerticalAimSensitivity.position - expected) < 1e-5f && c.aim == 0.002f;
        };
        if (!loads(0.5f) || CClientVariables::GetSingleton().Exists("vertical_aim_sensitivity"))
            return false;
        CVARS_SET("vertical_aim_sensitivity", "0.001bad");
        if (!loads(0.5f))
            return false;
        CVARS_SET("vertical_aim_sensitivity", 0.0f);
        if (!loads(0.0f))
            return false;
        CVARS_SET("vertical_aim_sensitivity", 0.002f);
        return loads((0.002f - 0.000312f) / (0.002688f - 0.000312f));
    }
    if (name == "controls-load")
    {
        ui.LoadData();
        return ui.m_pInvertMouse.selected && !ui.m_pSteerWithMouse.selected && !ui.m_pFlyWithMouse.selected && !ui.m_pClassicControls.selected &&
               ui.m_pStandardControls.selected && !ui.m_pCheckboxVerticalAimSensitivity.selected && ui.m_pVerticalAimSensitivity.enabled &&
               std::abs(ui.m_pVerticalAimSensitivity.position - 0.5f) < 1e-5f && ui.m_pMouseSensitivity.position == 0.8f && c.aim == 0.002f;
    }
    ui.LoadData();
    if (name == "controls-invalid-nick")
    {
        ui.m_pEditNick.text.clear();
        return ui.OnOKButtonClick(nullptr) && g_pCore->errors == 1 && ui.m_pWindow.visible && !ui.binds && !ui.joypad && !ui.saves;
    }
    if (!ui.OnControlsDefaultClick(nullptr) || !ui.m_pStandardControls.selected || ui.m_pClassicControls.selected || ui.m_pInvertMouse.selected ||
        ui.m_pFlyWithMouse.selected || ui.m_pSteerWithMouse.selected || ui.m_pCheckboxVerticalAimSensitivity.selected ||
        !ui.m_pVerticalAimSensitivity.enabled || ui.m_pMouseSensitivity.position != 0.5f || ui.m_pVerticalAimSensitivity.position != 0.5f)
        return false;
    for (auto key : {"invert_mouse", "steer_with_mouse", "fly_with_mouse", "classic_controls", "use_mouse_sensitivity_for_aiming"})
        if (!CVARS_GET_VALUE<bool>(key, false))
            return false;
    if (!c.inverted || !c.flight || !c.steering || !c.classic || !c.sameAim || c.aim != 0.002f ||
        CVARS_GET_VALUE<float>("vertical_aim_sensitivity", 0) != 0.002f || game.settings.mouse != 0.8f)
        return false;
    if (!ui.OnOKButtonClick(nullptr) || g_pCore->errors || ui.m_pWindow.visible || ui.binds != 1 || ui.joypad != 1 || ui.saves != 1)
        return false;
    for (auto key : {"invert_mouse", "steer_with_mouse", "fly_with_mouse", "classic_controls", "use_mouse_sensitivity_for_aiming"})
        if (CVARS_GET_VALUE<bool>(key, true))
            return false;
    return !c.inverted && !c.flight && !c.steering && !c.classic && !c.sameAim && game.settings.mouse == 0.5f && std::abs(c.aim - 0.0015f) < 1e-7f &&
           CVARS_GET_VALUE<float>("vertical_aim_sensitivity", 0) == c.aim;
}

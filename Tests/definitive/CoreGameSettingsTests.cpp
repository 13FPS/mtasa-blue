// The runner includes the production Core settings pass and XML scalar readers.
#include <cerrno>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <list>
#include <sstream>
#include <string_view>
#include <SharedUtil.Defines.h>
#include <core/CCVarsInterface.h>
#include <game/CControllerConfigManager.h>
#include <game/CSettings.h>
#include <tinyxml2.h>

class CXMLNodeImpl
{
public:
    tinyxml2::XMLElement*   m_pNode;
    std::list<CXMLNodeImpl> children;
    explicit CXMLNodeImpl(tinyxml2::XMLElement* node = nullptr) : m_pNode(node) {}
    CXMLNodeImpl* FindSubNode(const char* key)
    {
        auto* node = m_pNode->FirstChildElement(key);
        return node ? &children.emplace_back(node) : nullptr;
    }
    CXMLNodeImpl* CreateSubNode(const char* key) { return &children.emplace_back(m_pNode->InsertNewChildElement(key)); }
    std::string   GetTagContent() { return m_pNode->GetText() ? m_pNode->GetText() : ""; }
    template <typename T>
    void SetTagContent(T value)
    {
        m_pNode->SetText(value);
    }
    bool        GetTagContent(bool&);
    bool        GetTagContent(int&);
    bool        GetTagContent(unsigned int&);
    bool        GetTagContent(float&);
    void        SetTagContent(bool);
    static bool StringToLong(const char*, long&);
};
#include "XmlScalars.generated.inl"

using CXMLNode = CXMLNodeImpl;
class CVector;
class CVector2D;
class CColor;
template <class T>
struct CSingleton
{
    static T& GetSingleton()
    {
        static T instance;
        return instance;
    }
};
template <class T>
T Clamp(T low, T value, T high)
{
    return std::clamp(value, low, high);
}
#include "CClientVariables.h"
#include "ClientVariables.generated.inl"

// Supply a document root directly, so gameplay settings run without opening a GUI.
CXMLNode root;
bool     CClientVariables::Load()
{
    m_pStorage = &root;
    return true;
}

struct RecordingController : CControllerConfigManager
{
    bool  inverted{}, flight{}, steering{}, classic{}, sameAim{};
    float aim{};
    void  SetMouseInverted(bool value) { inverted = value; }
    void  SetFlyWithMouse(bool value) { flight = value; }
    void  SetSteerWithMouse(bool value) { steering = value; }
    void  SetClassicControls(bool value) { classic = value; }
    void  SetVerticalAimSensitivityRawValue(float value) { aim = value; }
    void  SetVerticalAimSensitivitySameAsHorizontal(bool value) { sameAim = value; }
    float GetVerticalAimSensitivityRawValue() override { return aim; }
    void  SetControllerKeyAssociatedWithAction(eControllerAction, int, eControllerType) override { std::abort(); }
    int   GetControllerKeyAssociatedWithAction(eControllerAction, eControllerType) override { std::abort(); }
    void  SuspendSteerAndFlyWithMouse(bool) override { std::abort(); }
};
struct SettingsReceiver
{
    eAspectRatio  aspect{};
    bool          adjust{}, shadows{}, grass{}, pedShadows{};
    unsigned char radio{38}, effects{42};
    float         mouse{0.8f};
    float         GetMouseSensitivity() const { return mouse; }
    void          SetMouseSensitivity(float value) { mouse = value; }
    void          SetVolumetricShadowsEnabled(bool value) { shadows = value; }
    void          SetAspectRatio(eAspectRatio ratio, bool value)
    {
        aspect = ratio;
        adjust = value;
    }
    void SetGrassEnabled(bool value) { grass = value; }
    void UpdateFieldOfViewFromSettings() {}
    void ResetBlurEnabled() {}
    void ResetVehiclesLODDistance() {}
    void ResetPedsLODDistance() {}
    void ResetCoronaReflectionsEnabled() {}
    void SetDynamicPedShadowsEnabled(bool value) { pedShadows = value; }
    void SetRadioVolume(unsigned char value) { radio = value; }
    void SetSFXVolume(unsigned char value) { effects = value; }
};
struct CMultiplayer
{
    enum EFastClothesLoading
    {
        FAST_CLOTHES_OFF,
        FAST_CLOTHES_AUTO,
        FAST_CLOTHES_ON
    };
    EFastClothesLoading clothes{};
    bool                heat{}, smoke{};
    void                SetHeatHazeEnabled(bool value) { heat = value; }
    void                SetFastClothesLoading(EFastClothesLoading value) { clothes = value; }
    void                SetTyreSmokeEnabled(bool value) { smoke = value; }
};
struct Game
{
    RecordingController       controller;
    SettingsReceiver          settings;
    CControllerConfigManager* GetControllerConfigManager() { return &controller; }
    SettingsReceiver*         GetSettings() { return &settings; }
};
struct CConsole
{
    CVector2D position{24, 48}, size{400, 300};
    void      SetPosition(CVector2D& value) { position = value; }
    void      SetSize(CVector2D& value) { size = value; }
};
struct ConsoleGui
{
    CConsole  console;
    CConsole* GetConsole() { return &console; }
};
struct CCore
{
    Game*         m_pGame;
    CMultiplayer* m_pMultiplayer;
    ConsoleGui*   m_pLocalGUI = nullptr;
    void          ApplyConsoleSettings();
    void          ApplyGameSettings();
};
#define CGameSettings SettingsReceiver
#include "ApplyGameSettings.generated.inl"
#include "ApplyConsoleSettings.generated.inl"
#undef CGameSettings
#include "MouseControlsDialogTests.inl"

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::string_view name = argv[1];
    const char*            xml = nullptr;
    if (name == "saved")
        xml =
            "<c><invert_mouse>1</invert_mouse><fly_with_mouse>0</fly_with_mouse><steer_with_mouse>1</steer_with_mouse>"
            "<classic_controls>1</classic_controls><volumetric_shadows>1</volumetric_shadows><aspect_ratio>3</aspect_ratio>"
            "<hud_match_aspect_ratio>0</hud_match_aspect_ratio><grass>0</grass><heat_haze>1</heat_haze>"
            "<fast_clothes_loading>2</fast_clothes_loading><tyre_smoke_enabled>0</tyre_smoke_enabled>"
            "<dynamic_ped_shadows>1</dynamic_ped_shadows><vertical_aim_sensitivity>0.002</vertical_aim_sensitivity>"
            "<use_mouse_sensitivity_for_aiming>1</use_mouse_sensitivity_for_aiming><mastervolume>0.5</mastervolume>"
            "<radiovolume>0.25</radiovolume><sfxvolume>0.75</sfxvolume></c>";
    else if (name == "unreadable")
        xml =
            "<c><invert_mouse>1</invert_mouse><fly_with_mouse>bad</fly_with_mouse><steer_with_mouse/>"
            "<aspect_ratio>3</aspect_ratio><fast_clothes_loading/><mastervolume>1</mastervolume></c>";
    else if (name == "ranges")
        xml =
            "<c><invert_mouse>0</invert_mouse><aspect_ratio>-1</aspect_ratio><fast_clothes_loading>99</fast_clothes_loading>"
            "<mastervolume>1</mastervolume></c>";
    else if (name == "migration")
        xml = "<c><mastervolume>0.5</mastervolume><sfxvolume>0.25</sfxvolume><limit>91</limit><invalid>nan</invalid></c>";
    else if (name == "controls-load")
        xml =
            "<c><invert_mouse>1</invert_mouse><steer_with_mouse>bad</steer_with_mouse><classic_controls>0</classic_controls>"
            "<vertical_aim_sensitivity>bad</vertical_aim_sensitivity></c>";
    else if (name == "integer-startup")
        xml =
            "<c><invert_mouse>1bad</invert_mouse><grass>0bad</grass><aspect_ratio>3bad</aspect_ratio>"
            "<fast_clothes_loading>broken</fast_clothes_loading><steer_with_mouse> 1 </steer_with_mouse></c>";
    else if (name == "controls-numbers" || name == "profile-floats" || name == "profile-integers" || name == "console-layout" || name == "profile-vector2d")
        xml = "<c/>";
    else if (name == "controls-volumes")
        xml = "<c><mastervolume>0</mastervolume><mtavolume>0.6</mtavolume><voicevolume>broken</voicevolume></c>";
    else if (name == "controls-defaults" || name == "controls-invalid-nick")
        xml =
            "<c><invert_mouse>1</invert_mouse><steer_with_mouse>1</steer_with_mouse><fly_with_mouse>1</fly_with_mouse>"
            "<classic_controls>1</classic_controls><vertical_aim_sensitivity>0.002</vertical_aim_sensitivity>"
            "<use_mouse_sensitivity_for_aiming>1</use_mouse_sensitivity_for_aiming></c>";
    else
        return 2;
    tinyxml2::XMLDocument document;
    if (document.Parse(xml) != tinyxml2::XML_SUCCESS)
        return 2;
    root.m_pNode = document.RootElement();
    auto& variables = CClientVariables::GetSingleton();
    variables.Load();
    if (name == "profile-vector2d")
    {
        const CVector2D original(50, 40);
        CVector2D       value = original;
        if (variables.Get("pair", value) || variables.Exists("pair") || !(value == original))
            return 1;
        for (const char* text : {"25", "25 bad", "25 40bad", "25 40 60", "", "nan 4", "1e99 8"})
        {
            variables.Set("pair", text);
            variables.ClampValue("pair", CVector2D(0, 0), CVector2D(100, 100));
            if (variables.Get("pair", value) || !(value == original) || root.FindSubNode("pair")->GetTagContent() != text)
                return 1;
        }
        variables.Set("pair", "  -1.25\t+2e2   ");
        if (!variables.Get("pair", value) || !(value == CVector2D(-1.25f, 200)))
            return 1;
        variables.ClampValue("pair", CVector2D(0, 0), CVector2D(100, 100));
        return variables.Get("pair", value) && value == CVector2D(0, 100) ? 0 : 1;
    }
    if (name == "console-layout")
    {
        ConsoleGui gui;
        CCore      core{nullptr, nullptr, &gui};
        core.ApplyConsoleSettings();
        if (!(gui.console.position == CVector2D(24, 48)) || !(gui.console.size == CVector2D(400, 300)) || variables.Exists("console_size"))
            return 1;
        variables.Set("console_pos", CVector2D(80, 160));
        variables.Set("console_size", "123 broken");
        core.ApplyConsoleSettings();
        if (!(gui.console.position == CVector2D(80, 160)) || !(gui.console.size == CVector2D(400, 300)))
            return 1;
        variables.Set("console_pos", "40");
        variables.Set("console_size", CVector2D(600, 500));
        core.ApplyConsoleSettings();
        return gui.console.position == CVector2D(80, 160) && gui.console.size == CVector2D(600, 500) ? 0 : 1;
    }
    if (name == "profile-integers")
    {
        int      number = -13;
        unsigned whole = 17;
        bool     toggle = true;
        if (variables.Get("n", number) || variables.Get("n", whole) || variables.Get("n", toggle) || variables.Exists("n"))
            return 1;
        for (const char* text : {"", " ", "junk", "3bad", "2.5", "1 0", "0x", "999999999999999999999999"})
        {
            variables.Set("n", text);
            if (variables.Get("n", number) || number != -13 || variables.Get("n", whole) || whole != 17 || variables.Get("n", toggle) || !toggle)
                return 1;
        }
        for (const auto& [text, expected] : {std::pair{"  +0x20\t", 32}, {"010", 8}, {"-2147483648", INT_MIN}, {"2147483647", INT_MAX}})
        {
            variables.Set("n", text);
            if (!variables.Get("n", number) || number != expected)
                return 1;
        }
        for (const char* text : {"2147483648", "-2147483649"})
        {
            variables.Set("n", text);
            if (variables.Get("n", number) || number != INT_MAX)
                return 1;
        }
        variables.Set("n", UINT_MAX);
        if (!variables.Get("n", whole) || whole != UINT_MAX)
            return 1;
        for (const char* text : {"4294967296", "-1"})
        {
            variables.Set("n", text);
            if (variables.Get("n", whole) || whole != UINT_MAX)
                return 1;
        }
        for (const char* text : {"0bad", "10", "2", "-1"})
        {
            variables.Set("n", text);
            if (variables.Get("n", toggle) || !toggle)
                return 1;
        }
        variables.Set("n", " 0\t");
        if (!variables.Get("n", toggle) || toggle)
            return 1;
        variables.Set("n", "1");
        return variables.Get("n", toggle) && toggle ? 0 : 1;
    }
    if (name == "profile-floats")
    {
        float value = 0.375f;
        if (variables.Get("scalar", value) || variables.Exists("scalar") || value != 0.375f)
            return 1;
        for (const char* text : {"", "   ", "bad", "0.001bad", "nan", "-inf", "1e99"})
        {
            variables.Set("scalar", text);
            if (variables.Get("scalar", value) || value != 0.375f)
                return 1;
        }
        for (const auto& [text, expected] : {std::pair{"0", 0.0f}, {" +1.25e-1\t", 0.125f}, {"-0.5", -0.5f}})
        {
            variables.Set("scalar", text);
            if (!variables.Get("scalar", value) || value != expected)
                return 1;
        }
        return 0;
    }
    Game game;
    if (name.substr(0, 9) == "controls-")
        return TestMouseDialog(name, game) ? 0 : 1;
    CMultiplayer multiplayer;
    CCore        core{&game, &multiplayer};
    if (name == "migration")
    {
        float value = 0.75f;
        if (variables.Get("radiovolume", value) || value != 0.75f || variables.Exists("radiovolume"))
            return 1;
        variables.ClampValue("radiovolume", 0.0f, 1.0f);
        variables.ClampValue("limit", 0, 10);
        core.ApplyGameSettings();
        return !variables.Exists("radiovolume") && game.settings.radio == 38 && game.settings.effects == 8 && !variables.Get("invalid", value) &&
                       value == 0.75f && variables.GetValue<int>("limit") == 10
                   ? 0
                   : 1;
    }
    core.ApplyGameSettings();
    const auto& c = game.controller;
    const auto& s = game.settings;
    bool        ok;
    if (name == "integer-startup")
        ok = !c.inverted && c.steering && s.grass && s.aspect == ASPECT_RATIO_AUTO && multiplayer.clothes == CMultiplayer::FAST_CLOTHES_AUTO;
    else if (name == "saved")
        ok = c.inverted && !c.flight && c.steering && c.classic && c.sameAim && c.aim == 0.002f && s.shadows && s.aspect == ASPECT_RATIO_16_9 && !s.adjust &&
             !s.grass && s.pedShadows && s.radio == 8 && s.effects == 24 && multiplayer.heat && !multiplayer.smoke &&
             multiplayer.clothes == CMultiplayer::FAST_CLOTHES_ON;
    else if (name == "unreadable")
        ok = c.inverted && !c.flight && !c.steering && !c.classic && !c.sameAim && c.aim == 0.0015f && !s.shadows && s.aspect == ASPECT_RATIO_16_9 &&
             s.adjust && s.grass && !s.pedShadows && multiplayer.heat && multiplayer.smoke && multiplayer.clothes == CMultiplayer::FAST_CLOTHES_AUTO;
    else
        ok = s.aspect == ASPECT_RATIO_AUTO && multiplayer.clothes == CMultiplayer::FAST_CLOTHES_AUTO;
    if (!ok)
        std::cerr << name << " settings received incorrect values\n";
    return ok ? 0 : 1;
}

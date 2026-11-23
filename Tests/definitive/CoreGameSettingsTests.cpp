// The runner includes the production Core settings pass and XML scalar readers.
#include <cerrno>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <list>
#include <string_view>
#include <core/CCVarsInterface.h>
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

struct CControllerConfigManager
{
    bool  inverted{}, flight{}, steering{}, classic{}, sameAim{};
    float aim{};
    void  SetMouseInverted(bool value) { inverted = value; }
    void  SetFlyWithMouse(bool value) { flight = value; }
    void  SetSteerWithMouse(bool value) { steering = value; }
    void  SetClassicControls(bool value) { classic = value; }
    void  SetVerticalAimSensitivityRawValue(float value) { aim = value; }
    void  SetVerticalAimSensitivitySameAsHorizontal(bool value) { sameAim = value; }
};
struct SettingsReceiver
{
    eAspectRatio  aspect{};
    bool          adjust{}, shadows{}, grass{}, pedShadows{};
    unsigned char radio{38}, effects{42};
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
    CControllerConfigManager  controller;
    SettingsReceiver          settings;
    CControllerConfigManager* GetControllerConfigManager() { return &controller; }
    SettingsReceiver*         GetSettings() { return &settings; }
};
struct CCore
{
    Game*         m_pGame;
    CMultiplayer* m_pMultiplayer;
    void          ApplyGameSettings();
};
#define CGameSettings SettingsReceiver
#include "ApplyGameSettings.generated.inl"
#undef CGameSettings

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
    else
        return 2;
    tinyxml2::XMLDocument document;
    if (document.Parse(xml) != tinyxml2::XML_SUCCESS)
        return 2;
    root.m_pNode = document.RootElement();
    auto& variables = CClientVariables::GetSingleton();
    variables.Load();
    Game         game;
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
    if (name == "saved")
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

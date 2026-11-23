// test_game_settings_video_modes.py builds this with the actual SDK, classic selection methods and Core list population.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>
#include "CSettings.h"

using uint = unsigned int;
class CSettingsSA : public CGameSettings
{
public:
#include "OtherSettings.generated.inl"
    uint       GetNumVideoModes() override { return static_cast<uint>(modes.size()); }
    uint       GetCurrentVideoMode() override { return current; }
    VideoMode* GetVideoModeInfo(VideoMode* out, uint index) override
    {
        if (index < modes.size() && modes[index])
        {
            *out = *modes[index];
            return out;
        }
        // Failed providers are free to leave arbitrary data in their output.
        *out = {3840, 2160, 32, rwVIDEOMODEEXCLUSIVE, 60, 0};
        return nullptr;
    }
    bool IsUnsafeResolution(int width, int height) override { return width > 1920 || height > 1080; }
    bool HasUnsafeResolutions() override;
    uint FindVideoMode(int, int, int);
    void SetAspectRatio(eAspectRatio, bool = true) override;

    std::vector<std::optional<VideoMode>> modes;
    uint                                  current = 0;
    eAspectRatio                          m_AspectRatio = ASPECT_RATIO_4_3;
};

static CSettingsSA settings;
static float       appliedRatio = 4.f / 3.f;
static int         fallbackCount = 0;
template <class T>
static void MemPutFast(unsigned, T value)
{
    appliedRatio = value;
}
enum
{
    EXIT_GAME_FIRST = 1,
    ASK_GO_ONLINE = 2
};
static const char* _(const char* message)
{
    return message;
}
static void BrowseToSolution(const char*, int, const char*)
{
    ++fallbackCount;
}
struct Hud
{
    void  AdjustComponents(float ratio) { adjustment = ratio; }
    void  ResetComponentAdjustment() { adjustment = 0; }
    float adjustment = 0;
};
struct Game
{
    CGameSettings* GetSettings() { return &settings; }
    Hud*           GetHud() { return &hud; }
    Hud            hud;
} game;
static auto* pGame = &game;
struct CCore
{
    static CCore& GetSingleton()
    {
        static CCore core;
        return core;
    }
    Game* GetGame() { return &game; }
};

struct SString : std::string
{
    SString() = default;
    SString(const char* pattern, int width, int height, int depth)
    {
        std::array<char, 80> label{};
        std::snprintf(label.data(), label.size(), pattern, width, height, depth);
        assign(label.data());
    }
};
struct CGUIListItem
{
    void          SetData(void* data) { index = reinterpret_cast<std::intptr_t>(data); }
    std::intptr_t index = -1;
    std::string   text;
};
struct Combo
{
    void Clear()
    {
        items.clear();
        text.clear();
    }
    CGUIListItem* AddItem(const SString& label)
    {
        items.emplace_back();
        items.back().text = label;
        return &items.back();
    }
    void                      SetText(const SString& label) { text = label; }
    std::string               text;
    std::vector<CGUIListItem> items;
};
struct CheckBox
{
    bool GetSelected() const { return false; }
};
struct CSettings
{
    CSettings* GetVideoModeManager() { return this; }
    void       GetNextVideoMode(int& mode, bool&, bool&, int&) { mode = static_cast<int>(settings.current); }
    void       PopulateResolutionComboBox();
    CheckBox   checkBox;
    Combo      combo;
    CheckBox*  m_pCheckBoxShowUnsafeResolutions = &checkBox;
    Combo*     m_pComboResolution = &combo;
};
#include "VideoModeMethods.generated.inl"

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::string scenario = argv[1];
    CGameSettings&    sdk = settings;
    VideoMode         normal{1280, 720, 32, rwVIDEOMODEEXCLUSIVE, 60, 0};
    VideoMode         fullHd{1920, 1080, 32, rwVIDEOMODEEXCLUSIVE, 60, 0};
    VideoMode         huge{3840, 2160, 32, rwVIDEOMODEEXCLUSIVE, 60, 0};
    VideoMode         windowed = huge;
    windowed.flags = static_cast<VideoModeFlag>(0);
    settings.modes = {std::nullopt, normal, windowed, fullHd, normal};
    if (scenario == "unsafe")
    {
        if (sdk.HasUnsafeResolutions())
            return 1;
        settings.modes.push_back(huge);
        return sdk.HasUnsafeResolutions() ? 0 : 1;
    }
    if (scenario == "select")
    {
        if (settings.FindVideoMode(3840, 2160, 32) != 3 || settings.FindVideoMode(1280, 720, 32) != 1)
            return 1;
        settings.modes.clear();
        return settings.FindVideoMode(800, 600, 32) == 1 && fallbackCount == 1 ? 0 : 1;
    }
    if (scenario == "auto-aspect")
    {
        sdk.SetAspectRatio(ASPECT_RATIO_16_10);
        sdk.SetAspectRatio(ASPECT_RATIO_AUTO);
        if (settings.m_AspectRatio != ASPECT_RATIO_16_10 || appliedRatio != 1.6f || game.hud.adjustment != 1.6f)
            return 1;
        normal.height = 0;
        settings.modes[0] = normal;
        sdk.SetAspectRatio(ASPECT_RATIO_AUTO, false);
        if (settings.m_AspectRatio != ASPECT_RATIO_16_10 || game.hud.adjustment != 1.6f)
            return 1;
        settings.current = 3;
        sdk.SetAspectRatio(ASPECT_RATIO_AUTO, false);
        return settings.m_AspectRatio == ASPECT_RATIO_AUTO && appliedRatio == 1920.f / 1080 && game.hud.adjustment == 0 ? 0 : 1;
    }
    if (scenario == "choices")
    {
        CSettings view;
        settings.current = 999;
        view.PopulateResolutionComboBox();
        const auto& items = view.combo.items;
        if (items.size() != 2 || items[0].index != 3 || items[1].index != 1 || !view.combo.text.empty())
            return 1;
        settings.current = 4;
        view.PopulateResolutionComboBox();
        return view.combo.items.size() == 2 && view.combo.text == "1280 x 720 x 32" ? 0 : 1;
    }
    return 2;
}

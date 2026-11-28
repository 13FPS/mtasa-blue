// Exercise the profile rules against Core's XML-backed variables. Only the
// renderer endpoints and the destinations of Classic's four resets are recorded.
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>
#include <game/CGameSettingsDE.h>

namespace VideoProfileTests
{
    struct RenderSettings
    {
        int             blur = -1;
        bool            reflections = true;
        void            SetBlurLevel(int value) { blur = value; }
        void            SetCoronaReflectionsEnabled(bool value) { reflections = value; }
        RenderSettings* GetGame() { return this; }
        RenderSettings* GetCoronas() { return this; }
    } render;
    static auto* pGame = &render;
    static auto* g_pCore = &render;

    class Receiver : public mta::game_de::CGameSettingsDE
    {
    public:
        explicit Receiver(CCVarsInterface& profile) : CGameSettingsDE(profile) {}
#include "OtherProfileMethods.generated.inl"
        bool shadows = true, pedShadows = true;
        bool refuse = false;
        void SetVolumetricShadowsEnabled(bool value) override
        {
            if (refuse)
                throw std::runtime_error("shadow renderer unavailable");
            shadows = value;
        }
        void SetDynamicPedShadowsEnabled(bool value) override { pedShadows = value; }

        struct FieldOfView
        {
            unsigned int channel;
            float        angle;
            bool         script, instant;
        };
        std::vector<FieldOfView> fov;
        void                     SetFieldOfViewPlayer(float v, bool script, bool instant) override { fov.push_back({0, v, script, instant}); }
        void                     SetFieldOfViewVehicle(float v, bool script, bool instant) override { fov.push_back({1, v, script, instant}); }
        void                     SetFieldOfViewVehicleMax(float v, bool script, bool instant) override { fov.push_back({2, v, script, instant}); }
        bool                     CheckFov(float value)
        {
            fov.clear();
            UpdateFieldOfViewFromSettings();
            if (fov.size() != 3)
                return false;
            for (size_t i = 0; i < fov.size(); ++i)
                if (fov[i].channel != i || fov[i].angle != (i == 2 ? 100.f : value) || fov[i].script || fov[i].instant)
                    return false;
            return true;
        }

        bool  ms_bMaxVehicleLODDistanceFromScript{}, ms_bMaxPedsLODDistanceFromScript{}, m_bBlurViaScript{}, m_bCoronaReflectionsViaScript{};
        float ms_fVehicleLODDistance{}, ms_fTrainPlaneLODDistance{}, ms_fPedsLODDistance{};
        void  ResetVehiclesLODDistance(bool bForceDefault) override;
        void  ResetPedsLODDistance(bool bForceDefault) override;
        void  ResetBlurEnabled() override;
        void  ResetCoronaReflectionsEnabled() override;
    };

    using CSettingsSA = Receiver;
#include "ClassicVideoResets.generated.inl"

    class UnavailableProfile : public CClientVariables
    {
    public:
        bool throwing = false, nonfinite = false;
        bool Get(const std::string&, bool& out) override
        {
            if (throwing)
                throw std::runtime_error("profile unavailable");
            out = true;
            return false;
        }
        bool Get(const std::string&, float& out) override
        {
            out = nonfinite ? std::numeric_limits<float>::infinity() : 99.f;
            return nonfinite;
        }
    };

    int CheckDE(CClientVariables& profile)
    {
        Receiver       receiver(profile);
        CGameSettings& sdk = receiver;
        if (sdk.GetVolumetricShadowsEnabledByVideoSetting() || sdk.IsDynamicPedShadowsEnabledByVideoSetting() || !sdk.ResetVolumetricShadows() ||
            !sdk.ResetDynamicPedShadows() || receiver.shadows || receiver.pedShadows || !receiver.CheckFov(70.f) || profile.Exists("fov"))
            return 1;
        profile.Set("volumetric_shadows", true);
        profile.Set("dynamic_ped_shadows", "1bad");
        if (!sdk.GetVolumetricShadowsEnabledByVideoSetting() || sdk.IsDynamicPedShadowsEnabledByVideoSetting() || !sdk.ResetVolumetricShadows() ||
            !sdk.ResetDynamicPedShadows() || !receiver.shadows || receiver.pedShadows)
            return 1;
        profile.Set("volumetric_shadows", false);
        profile.Set("dynamic_ped_shadows", true);
        if (!sdk.ResetVolumetricShadows() || !sdk.ResetDynamicPedShadows() || receiver.shadows || !receiver.pedShadows)
            return 1;
        receiver.refuse = true;
        if (sdk.ResetVolumetricShadows())
            return 1;
        for (const auto& [entry, expected] :
             std::array<std::pair<const char*, float>, 6>{{{"48", 70.f}, {"77.5", 77.5f}, {"103", 100.f}, {"95bad", 70.f}, {"nan", 70.f}, {"inf", 70.f}}})
        {
            profile.Set("fov", entry);
            if (!receiver.CheckFov(expected))
                return 1;
        }
        UnavailableProfile unavailable;
        Receiver           other(unavailable);
        if (other.GetVolumetricShadowsEnabledByVideoSetting() || other.IsDynamicPedShadowsEnabledByVideoSetting() || !other.ResetVolumetricShadows() ||
            !other.ResetDynamicPedShadows() || other.shadows || other.pedShadows || !other.CheckFov(70.f))
            return 1;
        unavailable.nonfinite = unavailable.throwing = true;
        other.shadows = other.pedShadows = true;
        return !other.GetVolumetricShadowsEnabledByVideoSetting() && !other.IsDynamicPedShadowsEnabledByVideoSetting() && !other.ResetVolumetricShadows() &&
                       !other.ResetDynamicPedShadows() && other.shadows && other.pedShadows && other.CheckFov(70.f)
                   ? 0
                   : 1;
    }

    int CheckClassic(CClientVariables& profile)
    {
        Receiver       receiver(profile);
        CGameSettings& sdk = receiver;
        const auto     reset = [&](bool forced)
        {
            sdk.ResetVehiclesLODDistance(forced);
            sdk.ResetPedsLODDistance(forced);
            sdk.ResetBlurEnabled();
            sdk.ResetCoronaReflectionsEnabled();
        };
        const auto defaults = [&]
        {
            return receiver.ms_fVehicleLODDistance == DEFAULT_VEHICLE_LOD_DISTANCE &&
                   receiver.ms_fTrainPlaneLODDistance == DEFAULT_VEHICLE_LOD_DISTANCE * TRAIN_LOD_DISTANCE_MULTIPLIER &&
                   receiver.ms_fPedsLODDistance == DEFAULT_PEDS_LOD_DISTANCE && render.blur == DEFAULT_BLUR_LEVEL && !render.reflections;
        };
        reset(false);
        if (!defaults())
            return 1;
        for (const char* key : {"high_detail_vehicles", "high_detail_peds", "blur", "corona_reflections"})
            profile.Set(key, "1bad");
        reset(false);
        if (!defaults())
            return 1;
        profile.Set("high_detail_vehicles", true);
        profile.Set("high_detail_peds", true);
        profile.Set("blur", false);
        profile.Set("corona_reflections", true);
        receiver.ms_bMaxVehicleLODDistanceFromScript = receiver.ms_bMaxPedsLODDistanceFromScript = true;
        receiver.m_bBlurViaScript = receiver.m_bCoronaReflectionsViaScript = true;
        reset(false);
        if (!defaults() || !receiver.ms_bMaxVehicleLODDistanceFromScript || !receiver.ms_bMaxPedsLODDistanceFromScript)
            return 1;
        reset(true);
        if (receiver.ms_bMaxVehicleLODDistanceFromScript || receiver.ms_bMaxPedsLODDistanceFromScript || render.blur != DEFAULT_BLUR_LEVEL ||
            render.reflections || receiver.ms_fVehicleLODDistance != MAX_VEHICLE_LOD_DISTANCE ||
            receiver.ms_fTrainPlaneLODDistance != MAX_VEHICLE_LOD_DISTANCE || receiver.ms_fPedsLODDistance != MAX_PEDS_LOD_DISTANCE)
            return 1;
        receiver.m_bBlurViaScript = receiver.m_bCoronaReflectionsViaScript = false;
        reset(false);
        return render.blur == 0 && render.reflections ? 0 : 1;
    }
}

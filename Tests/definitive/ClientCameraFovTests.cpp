// Compile the actual Lua camera matrix/FOV functions over controlled SDK replies.
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <SharedUtil.Defines.h>
enum eBone
{
};
#include <CMatrix.h>
#include <game/CCam.h>
#include <game/CCamera.h>
#include <CLuaMultiReturn.h>
#include "CameraTransitionPoseChecks.h"

namespace classic_transition
{
    struct CCameraSAInterface
    {
        std::uint8_t m_uiTransitionState;
        CVector      SourceDuringInter, TargetDuringInter, UpDuringInter;
    };
    struct CCameraSA
    {
        CCameraSAInterface* data = nullptr;
        CCameraSAInterface* GetInterface() const { return data; }
        bool                IsInTransition() const { return data && data->m_uiTransitionState != 0; }
        bool                GetTransitionMatrix(CMatrix&) const;
    };
    // Historical source used this helper for its three finite-input checks.
    [[maybe_unused]] static bool IsFiniteVector(const CVector& value)
    {
        return value.IsValid();
    }
#include "ClassicTransition.inl"
    void Check()
    {
        CCameraSA  camera;
        CMatrix    output(CVector(1, 2, 3));
        const auto before = output.To4x4Array();
        if (camera.GetTransitionMatrix(output) || output.To4x4Array() != before)
            throw std::runtime_error("absent classic camera changed the output");
        CCameraSAInterface sample{};
        camera.data = &sample;
        if (camera.GetTransitionMatrix(output) || output.To4x4Array() != before)
            throw std::runtime_error("inactive classic transition changed the output");
        CheckCameraTransitionPoses(
            [&](CVector source, CVector target, CVector up, CMatrix& matrix)
            {
                sample = {1, source, target, up};
                return camera.GetTransitionMatrix(matrix);
            });
        std::puts("Passed Classic transition roll, vertical, fallback and invalid input checks");
    }
}

static constexpr float classicFov = 56.25f;
// The trace distinguishes transition state (I), transition FOV (T), active row (A),
// row lookup (C) and row FOV (F), including where a provider refusal escapes.
static std::string calls;
static unsigned    checks = 0;
static char        refuse = 0;

static void Call(char op)
{
    calls += op;
    if (refuse == op)
        throw std::runtime_error(std::string(1, op));
}

struct Row : CCam
{
    mutable CVector front{0, 0, 2}, up{0, 3, 1}, source{17, -3, 5};
    CVector*        GetFront() const override
    {
        Call('D');
        return &front;
    }
    CVector* GetUp() const override
    {
        Call('U');
        return &up;
    }
    CVector* GetSource() const override
    {
        Call('S');
        return &source;
    }
    float value = 0;
    float GetFOV() const override
    {
        Call('F');
        return value;
    }
#include "UnusedCCam.inl"
};

struct Camera : CCamera
{
    bool     transition = false;
    bool     transitionPoseAvailable = true;
    BYTE     selector = 0;
    BYTE     vehicle = 1;
    BYTE     ped = 1;
    void     SetCameraVehicleViewMode(BYTE mode) override { vehicle = mode; }
    void     SetCameraPedViewMode(BYTE mode) override { ped = mode; }
    BYTE     GetCameraVehicleViewMode() override { return vehicle; }
    BYTE     GetCameraPedViewMode() override { return ped; }
    CMatrix* GetMatrix(CMatrix* output) override
    {
        Call('M');
        *output = CMatrix(CVector(9, 2, 1));
        return output;
    }
    bool GetTransitionMatrix(CMatrix& output) const override
    {
        Call('B');
        if (!transition || !transitionPoseAvailable)
            return false;
        output = CMatrix(CVector(8, 4, 2));
        return true;
    }
    Row  rows[3];
    bool IsInTransition() const override
    {
        Call('I');
        return transition;
    }
    float GetTransitionFOV() const override
    {
        Call('T');
        return 32.5f;
    }
    BYTE GetActiveCam() override
    {
        Call('A');
        return selector;
    }
    CCam* GetCam(BYTE row) override
    {
        Call('C');
        return row < 3 ? &rows[row] : nullptr;
    }
#include "UnusedCCamera.inl"
};

namespace SystemState
{
    enum Enum
    {
        GS_INIT,
        GS_PLAYING_GAME
    };
}
struct Game
{
    CCamera*          camera = nullptr;
    SystemState::Enum state = SystemState::GS_INIT;
    CCamera*          GetCamera() { return camera; }
    SystemState::Enum GetSystemState() const { return state; }
};
struct Settings
{
    std::string vehicle{"4"}, ped{"3"};
    unsigned    reads = 0, writes = 0, saves = 0;
    Settings*   GetCVars() { return this; }
    void        Get(const std::string& key, std::string& output)
    {
        ++reads;
        output = key == "camera_vehicle_view" ? vehicle : ped;
    }
    void Set(const std::string& key, int value)
    {
        ++writes;
        (key == "camera_vehicle_view" ? vehicle : ped) = std::to_string(value);
    }
    void SaveConfig() { ++saves; }
};
static Game*     g_pGame = nullptr;
static Settings* g_pCore = nullptr;

struct CClientCamera
{
    CCamera*        m_pCamera = nullptr;
    bool            m_viewModesRestored = false;
    std::uint8_t    m_lastVehicleViewMode = 0, m_lastPedViewMode = 0;
    void            PersistViewModes();
    void            SetCameraVehicleViewMode(eVehicleCamMode);
    void            SetCameraPedViewMode(ePedCamMode);
    eVehicleCamMode GetCameraVehicleViewMode();
    ePedCamMode     GetCameraPedViewMode();
    CMatrix         GetGtaMatrix() const;
    CMatrix         GetInterpolatedCameraMatrix() const;
    bool            IsInCameraTransition() const;
    float           GetAccurateFOV() const;
    void            GetPosition(CVector& p) const { p = CVector(1, 2, 3); }
    void            GetFixedTarget(CVector& p, float* roll) const
    {
        p = CVector(4, 5, 6);
        *roll = 7;
    }
    bool GetMatrix(CMatrix&) const { std::abort(); }
};
struct CStaticFunctionDefinitions
{
    static inline CClientCamera* m_pCamera;
    static bool                  GetCameraMatrix(CVector&, CVector&, float&, float&);
};
struct CLuaCameraDefs
{
    static CLuaMultiReturn<float, float, float, float, float, float, float, float> GetCameraMatrix();
};
#include "CameraMethods.inl"

static void Check(float value, const std::string& expectedCalls)
{
    calls.clear();
    const auto result = CLuaCameraDefs::GetCameraMatrix();
    if (result.values != std::make_tuple(1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, value) || calls != expectedCalls)
        throw std::runtime_error("unexpected Lua camera matrix or dispatch: " + calls + ", expected " + expectedCalls);
    ++checks;
}

static void CheckStartupAndPose()
{
    Game          game;
    Settings      settings;
    CClientCamera client;
    Camera        camera;
    g_pCore = &settings;
    client.PersistViewModes();
    g_pGame = &game;
    game.state = SystemState::GS_PLAYING_GAME;
    client.PersistViewModes();
    if (settings.reads || settings.saves || client.m_viewModesRestored)
        throw std::runtime_error("settings accessed before camera publication");
    game.state = SystemState::GS_INIT;
    game.camera = &camera;
    client.PersistViewModes();
    if (settings.reads || client.m_viewModesRestored)
        throw std::runtime_error("settings accessed before playing");
    game.state = SystemState::GS_PLAYING_GAME;
    client.PersistViewModes();
    if (settings.reads != 2 || settings.saves || settings.writes || camera.vehicle != 4 || camera.ped != 3 || !client.m_viewModesRestored)
        throw std::runtime_error("stored camera views were lost or not restored");
    camera.vehicle = 2;
    client.PersistViewModes();
    client.PersistViewModes();
    if (settings.saves != 1 || settings.vehicle != "2" || settings.ped != "3")
        throw std::runtime_error("live view change was not persisted exactly once");
    calls.clear();
    const CMatrix matrix = client.GetGtaMatrix();
#if defined(_WIN64)
    const std::string poseCalls = "BACDUS";
#else
    const std::string poseCalls = "BACMDUS";
#endif
    if (calls != poseCalls || matrix.vPos != CVector(17, -3, 5) || matrix.vFront != CVector(0, 0, 1) || matrix.vUp != CVector(0, 1, 0) ||
        matrix.vRight != CVector(-1, 0, 0))
        throw std::runtime_error("incorrect active camera pose: " + calls);
    camera.transition = true;
    calls.clear();
    if (client.GetGtaMatrix().vPos != CVector(8, 4, 2) || calls != "B")
        throw std::runtime_error("transition matrix dispatch changed");
    camera.transitionPoseAvailable = false;
    calls.clear();
    if (client.GetGtaMatrix().vPos != matrix.vPos || calls != poseCalls)
        throw std::runtime_error("missing transition pose lost the active row");
    refuse = 'B';
    calls.clear();
    bool propagated = false;
    try
    {
        client.GetGtaMatrix();
    }
    catch (const std::runtime_error& error)
    {
        propagated = error.what() == std::string("B");
    }
    refuse = 0;
    if (!propagated || calls != "B")
        throw std::runtime_error("transition source refusal was masked");
    camera.transition = false;
    camera.selector = 255;
    calls.clear();
    if (client.GetGtaMatrix().vPos != CVector() || calls != "BAC")
        throw std::runtime_error("absent row produced a pose");
    g_pGame = nullptr;
    g_pCore = nullptr;
    std::puts("Passed late camera publication, view persistence and active/transition pose checks");
}

int main()
{
    try
    {
        CheckStartupAndPose();
        classic_transition::Check();
        CClientCamera client;
        CStaticFunctionDefinitions::m_pCamera = &client;
        Check(DEFAULT_FOV, "");
        Camera camera;
        client.m_pCamera = &camera;
        camera.transition = true;
        Check(32.5f, "IT");
        camera.transition = false;
#if defined(_WIN64)
        for (BYTE i = 0; i < 3; ++i)
        {
            camera.selector = i;
            camera.rows[i].value = 15.25f + i * 91.5f;
            Check(camera.rows[i].value, "IACF");
        }
        camera.selector = 255;
        Check(DEFAULT_FOV, "IAC");
        camera.selector = 0;
        const std::string failureCalls = "IACFT";
#else
        Check(classicFov, "I");
        const std::string failureCalls = "IT";
#endif
        for (char op : failureCalls)
        {
            refuse = op;
            camera.transition = op == 'T';
            calls.clear();
            try
            {
                CLuaCameraDefs::GetCameraMatrix();
            }
            catch (const std::runtime_error& ex)
            {
                const std::string route = camera.transition ? "IT" : "IACF";
                if (ex.what() == std::string(1, op) && calls == route.substr(0, route.find(op) + 1))
                {
                    ++checks;
                    continue;
                }
            }
            throw std::runtime_error("provider refusal changed at " + std::string(1, op));
        }
        std::printf("Passed %u Lua camera matrix FOV scenarios\n", checks);
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}

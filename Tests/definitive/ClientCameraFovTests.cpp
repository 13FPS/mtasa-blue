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
#include <game/CCameraTransitionFOV.h>
#include <CLuaMultiReturn.h>
#include "CameraTransitionPoseChecks.h"
#include "FixedCameraMath.inl"

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
    bool            suppliesFront = true;
    CVector*        GetFront() const override
    {
        Call('D');
        return suppliesFront ? &front : nullptr;
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
    std::array<BYTE, 3> color{};
    void                SetFadeColor(BYTE red, BYTE green, BYTE blue) override
    {
        Call('K');
        color = {red, green, blue};
    }
    void Fade(float time, int direction) override
    {
        Call('E');
        if (time != 0.5f || (direction != FADE_IN && direction != FADE_OUT))
            throw std::runtime_error("wrong fade arguments");
        lastFade = direction;
    }
    int      lastFade = -1;
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

struct SampledCamera : Camera, CCameraTransitionFOV
{
    bool TryGetActiveTransitionFOV(float& value) const override
    {
        Call('Q');
        if (!transition)
            return false;
        value = 43.75f;
        return true;
    }
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
    struct Hud
    {
        bool visible = false;
        void SetComponentVisible(int, bool value)
        {
            Call('H');
            visible = value;
        }
    };
    Hud*              hud = nullptr;
    Hud*              GetHud() { return hud; }
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
struct ClientGame
{
    auto* GetManager();
    bool  initiallyFaded = true, hideAreaName = false;
    void  SetInitiallyFadedOut(bool value)
    {
        Call('X');
        initiallyFaded = value;
    }
    bool GetHudAreaNameDisabled() const { return hideAreaName; }
};
static ClientGame*   g_pClientGame = nullptr;
static constexpr int HUD_AREA_NAME = 1;

enum class EFixedCameraMode
{
    TARGET,
    MATRIX
};

struct CClientCamera
{
    void     FadeIn(float time);
    void     FadeOut(float time, BYTE red, BYTE green, BYTE blue);
    bool     ApplyFade(bool fadeIn, float time, BYTE red, BYTE green, BYTE blue);
    CCamera* m_pCamera = nullptr;
#include "CameraAvailability.inl"
    bool                        m_viewModesRestored = false;
    std::uint8_t                m_lastVehicleViewMode = 0, m_lastPedViewMode = 0;
    void                        PersistViewModes();
    void                        SetCameraVehicleViewMode(eVehicleCamMode);
    void                        SetCameraPedViewMode(ePedCamMode);
    eVehicleCamMode             GetCameraVehicleViewMode();
    ePedCamMode                 GetCameraPedViewMode();
    CMatrix                     GetGtaMatrix() const;
    bool                        GetGtaMatrix(CMatrix&) const;
    const CMatrix&              AcquirePulseMatrix() const;
    void                        InvalidateCachedTransforms() const;
    mutable std::optional<bool> m_pulseMatrixAvailable;
    mutable CMatrix             m_cachedPulseMatrix;
    mutable bool                m_hasCachedPulseEuler = false;
    mutable bool                m_hasCachedPulseMatrix = false;  // Historical sources use a single cache flag.
    bool                        m_bFixed = false;
    CMatrix                     m_matFixedMatrix;
    CVector                     m_vecFixedTarget;
    float                       m_fRoll = 0;
    EFixedCameraMode            m_FixedCameraMode{};
    bool                        IsInFixedMode() const { return m_bFixed; }
    void                        ToggleCameraFixedMode(bool enabled) { m_bFixed = enabled; }
    void                        UpdateCenterOfWorldFromFixedMatrix() {}
    void                        SetFixedTarget(const CVector&, float);
    CMatrix                     GetInterpolatedCameraMatrix() const;
    bool                        IsInCameraTransition() const;
    float                       GetAccurateFOV() const;
    void                        GetPosition(CVector& p) const;
    void                        GetFixedTarget(CVector& p, float* roll) const;
    bool                        GetMatrix(CMatrix&) const;
};
struct CStaticFunctionDefinitions
{
    struct Manager
    {
        CClientCamera* GetCamera() { return m_pCamera; }
    };
    static inline Manager        manager;
    static inline Manager*       m_pManager = &manager;
    static bool                  FadeCamera(bool fadeIn, float time, BYTE red, BYTE green, BYTE blue);
    static inline CClientCamera* m_pCamera;
    static bool                  GetCameraMatrix(CVector&, CVector&, float&, float&);
};
auto* ClientGame::GetManager()
{
    return CStaticFunctionDefinitions::m_pManager;
}

// Supply decoded fields to the production handler without a connection.
struct NetBitStreamInterface
{
    std::array<BYTE, 4> bytes{};
    size_t              count = 4, next = 0;
    bool                Read(BYTE& value)
    {
        if (next == count)
            return false;
        value = bytes[next++];
        return true;
    }
    bool Read(float& value)
    {
        value = 0.5f;
        return true;
    }
};
struct CCameraRPCs
{
    static inline CClientCamera* m_pCamera;
    static void                  FadeCamera(NetBitStreamInterface&);
};
struct CLuaCameraDefs
{
    static inline auto* m_pManager = CStaticFunctionDefinitions::m_pManager;
    static bool         SetCameraViewMode(std::optional<unsigned char>, std::optional<unsigned char>);
    static CLuaMultiReturn<float, float, float, float, float, float, float, float> GetCameraMatrix();
};
#include "CameraMethods.inl"

static void CheckFade()
{
    Game          game;
    Game::Hud     hud;
    ClientGame    clientGame;
    CClientCamera client;
    Camera        camera;
    g_pClientGame = &clientGame;
    g_pGame = &game;
    game.hud = &hud;
    CStaticFunctionDefinitions::m_pCamera = CCameraRPCs::m_pCamera = &client;
    calls.clear();
    if (CStaticFunctionDefinitions::FadeCamera(true, 0.5f, 0, 0, 0) || !clientGame.initiallyFaded || !calls.empty())
        throw std::runtime_error("unpublished camera reported a fade or changed the HUD");
    if (CLuaCameraDefs::SetCameraViewMode(4, 3) || !CLuaCameraDefs::SetCameraViewMode({}, {}))
        throw std::runtime_error("view mode request lost its camera availability or optional arguments");
    client.m_pCamera = &camera;
    if (!CLuaCameraDefs::SetCameraViewMode({}, 3) || camera.ped != 3 || camera.vehicle != 1 || !CLuaCameraDefs::SetCameraViewMode(4, {}) ||
        camera.vehicle != 4 || camera.ped != 3)
        throw std::runtime_error("requested view mode did not reach its camera");
    NetBitStreamInterface incomplete{{0, 12, 24, 36}, 3};
    CCameraRPCs::FadeCamera(incomplete);
    if (!clientGame.initiallyFaded || !calls.empty())
        throw std::runtime_error("incomplete fade-out changed the initial fade");
    if (!CStaticFunctionDefinitions::FadeCamera(true, 0.5f, 0, 0, 0) || camera.lastFade != FADE_IN || calls != "EXH" || !hud.visible ||
        clientGame.initiallyFaded)
        throw std::runtime_error("accepted fade-in did not restore the area name");
    calls.clear();
    CStaticFunctionDefinitions::m_pCamera = nullptr;
    NetBitStreamInterface fadeOut{{0, 12, 24, 36}};
    CCameraRPCs::FadeCamera(fadeOut);
    if (camera.lastFade != FADE_OUT || camera.color != std::array<BYTE, 3>{12, 24, 36} || calls != "KEXH" || hud.visible)
        throw std::runtime_error("fade-out lost its color, order or HUD state");
    calls.clear();
    clientGame.hideAreaName = true;
    NetBitStreamInterface fadeIn{{1}, 1};
    CCameraRPCs::FadeCamera(fadeIn);
    if (camera.lastFade != FADE_IN || calls != "EXH" || hud.visible)
        throw std::runtime_error("fade-in overrode the disabled area name");
    calls.clear();
    game.hud = nullptr;
    CStaticFunctionDefinitions::m_pCamera = &client;
    if (!CStaticFunctionDefinitions::FadeCamera(true, 0.5f, 0, 0, 0) || calls != "EX")
        throw std::runtime_error("unavailable HUD prevented the camera fade");
    g_pClientGame = nullptr;
    g_pGame = nullptr;
    CStaticFunctionDefinitions::m_pCamera = CCameraRPCs::m_pCamera = nullptr;
    std::puts("Passed camera publication, fade color and HUD side effects for script and RPC requests");
}

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
    CMatrix matrix(CVector(91, 72, 53));
    if (client.GetMatrix(matrix) || matrix.To4x4Array() != CMatrix().To4x4Array())
        throw std::runtime_error("absent game camera reported a pose");
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
    if (client.GetMatrix(matrix) || !calls.empty())
        throw std::runtime_error("missing pose was not cached for this pulse");
    client.InvalidateCachedTransforms();
    if (!client.GetMatrix(matrix))
        throw std::runtime_error("active row was not available after invalidating the pulse");
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
    if (!client.GetMatrix(matrix) || matrix.vPos != CVector(17, -3, 5) || !calls.empty())
        throw std::runtime_error("valid pose was not cached for this pulse");
    client.InvalidateCachedTransforms();
    if (!client.GetMatrix(matrix) || matrix.vPos != CVector(8, 4, 2) || calls != "B")
        throw std::runtime_error("transition matrix dispatch changed");
    camera.transitionPoseAvailable = false;
    calls.clear();
    client.InvalidateCachedTransforms();
    if (!client.GetMatrix(matrix) || matrix.vPos != CVector(17, -3, 5) || calls != poseCalls)
        throw std::runtime_error("missing transition pose lost the active row");
    refuse = 'B';
    calls.clear();
    client.InvalidateCachedTransforms();
    bool propagated = false;
    try
    {
        client.GetMatrix(matrix);
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
    if (client.GetMatrix(matrix) || matrix.To4x4Array() != CMatrix().To4x4Array() || calls != "BAC")
        throw std::runtime_error("absent row produced a pose");
    camera.selector = 0;
    camera.rows[0].suppliesFront = false;
    client.InvalidateCachedTransforms();
    if (client.GetMatrix(matrix) || matrix.To4x4Array() != CMatrix().To4x4Array())
        throw std::runtime_error("incomplete row produced a pose");
    camera.rows[0].suppliesFront = true;
    camera.rows[0].source.fX = std::numeric_limits<float>::quiet_NaN();
    client.InvalidateCachedTransforms();
    if (client.GetMatrix(matrix) || matrix.To4x4Array() != CMatrix().To4x4Array())
        throw std::runtime_error("nonfinite row produced a pose");
    client.m_bFixed = true;
    client.m_matFixedMatrix = CMatrix(CVector(11, 12, 13));
    client.m_matFixedMatrix.vRight = CVector(-1, 0, 0);
    calls.clear();
    if (!client.GetMatrix(matrix) || matrix.To4x4Array() != client.m_matFixedMatrix.To4x4Array() || !calls.empty())
        throw std::runtime_error("fixed camera did not preserve its supplied matrix");
    CStaticFunctionDefinitions::m_pCamera = &client;
    client.m_pCamera = nullptr;
    for (const auto direction : {CVector(0, 1, 0), CVector(0.4f, 0.8f, 0.9f), CVector(0.00001f, 0, 1), CVector(0, 0, 1), CVector(0, 0, -1)})
    {
        for (float roll : {0.f, 45.f, 90.f, 180.f, -35.f})
        {
            client.SetFixedTarget(client.m_matFixedMatrix.vPos + direction, roll);
            if (!client.GetMatrix(matrix))
                throw std::runtime_error("fixed target did not provide a matrix");
            CVector right = matrix.vFront;
            right.CrossProduct(&matrix.vUp);
            if ((right - matrix.vRight).Length() > 0.0001f || std::abs(right.Length() - 1.0f) > 0.0001f)
                throw std::runtime_error("fixed camera basis changed handedness");
            // Read it back as a matrix, with no retained target or roll.
            client.m_FixedCameraMode = EFixedCameraMode::MATRIX;
            const float observed = std::get<6>(CLuaCameraDefs::GetCameraMatrix().values);
            if (!std::isfinite(observed) || std::abs(std::remainder(observed - roll, 360.0f)) > 0.001f)
                throw std::runtime_error("Lua lost matrix roll: expected " + std::to_string(roll) + ", read " + std::to_string(observed));
        }
    }
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
        CStaticFunctionDefinitions::m_pCamera = nullptr;
        if (CLuaCameraDefs::GetCameraMatrix().values != std::make_tuple(0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, DEFAULT_FOV))
            throw std::runtime_error("unpublished camera did not return the identity view");
        CStaticFunctionDefinitions::m_pCamera = &client;
        client.m_bFixed = true;
        client.m_matFixedMatrix.vPos = CVector(1, 2, 3);
        client.SetFixedTarget(CVector(4, 5, 6), 7.0f);
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
        SampledCamera sampled;
        client.m_pCamera = &sampled;
        sampled.transition = true;
        Check(43.75f, "Q");
        sampled.transition = false;
#if defined(_WIN64)
        sampled.rows[0].value = 68.25f;
        Check(68.25f, "QACF");
#else
        Check(classicFov, "Q");
#endif
        refuse = 'Q';
        calls.clear();
        try
        {
            CLuaCameraDefs::GetCameraMatrix();
        }
        catch (const std::runtime_error& ex)
        {
            if (calls == "Q" && std::string(ex.what()) == "Q")
                refuse = 0;
        }
        if (refuse)
            throw std::runtime_error("sample refusal was masked");
        CheckFade();
        std::printf("Passed %u Lua camera matrix FOV scenarios, roll round trips and absent camera defaults\n", checks);
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}

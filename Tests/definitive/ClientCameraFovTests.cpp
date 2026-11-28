// Compile the actual Lua camera matrix/FOV functions over controlled SDK replies.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <stdexcept>
#include <string>
constexpr float FLOAT_EPSILON = 0.0001f;
enum eBone
{
};
#include <CMatrix.h>
#include <game/CCam.h>
#include <game/CCamera.h>
#include <CLuaMultiReturn.h>

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
    bool transition = false;
    BYTE selector = 0;
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

struct CClientCamera
{
    CCamera* m_pCamera = nullptr;
    bool     IsInCameraTransition() const;
    float    GetAccurateFOV() const;
    void     GetPosition(CVector& p) const { p = CVector(1, 2, 3); }
    void     GetFixedTarget(CVector& p, float* roll) const
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

int main()
{
    try
    {
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

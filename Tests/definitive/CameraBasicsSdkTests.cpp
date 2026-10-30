/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual camera Restore/selector and Cam finite FOV SDK policies
 ****************************************************************************/
#include "../../Client/game_de/CCameraSphereVisibleDE.h"
#include "../../Client/game_de/CCamFovDE.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "CameraBasicsFixture.generated.h"
#include "CamFovFixture.generated.h"
using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&CameraBasicsObserver::Restore), decltype(&CCameraSphereVisibleDE::Restore)>);
static_assert(std::is_same_v<decltype(&CameraBasicsObserver::GetActiveCam), decltype(&CCameraSphereVisibleDE::GetActiveCam)>);
static_assert(std::is_same_v<decltype(&CameraBasicsObserver::IsSphereVisible), decltype(&CCameraSphereVisibleDE::IsSphereVisible)>);
static_assert(std::is_same_v<decltype(&CamFovObserver::GetFOV), decltype(&CCamFovDE::GetFOV)>);
static_assert(std::is_abstract_v<CCameraSphereVisibleDE> && std::is_abstract_v<CCamFovDE>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    struct Access final : NativeCameraSphereVisibleAccess
    {
        ClockResult   status = ClockResult::Ok;
        unsigned      activeCalls = 0, fovCalls = 0, restoreCalls = 0, sphereCalls = 0;
        std::uint32_t row = 0, rawFov = 0;
        std::uint8_t  active = 0;
        bool          visible = false;
        ClockResult   ReadCameraActiveIndex(std::uint8_t& value) noexcept override
        {
            ++activeCalls;
            value = active;
            return status;
        }
        ClockResult ReadCameraFov(std::uint32_t index, std::uint32_t& value) noexcept override
        {
            ++fovCalls;
            row = index;
            value = rawFov;
            return status;
        }
        ClockResult RestoreCamera() noexcept override
        {
            ++restoreCalls;
            return status;
        }
        ClockResult ReadCameraSphereVisible(const CVector&, float, bool& value) noexcept override
        {
            ++sphereCalls;
            value = visible;
            return status;
        }
    };
    struct Missing final : NativeCameraSphereVisibleAccess
    {
    };
    template <class F>
    void Refuses(F&& operation, const char* method, ClockResult status)
    {
        bool refused = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text = error.what(), head = std::string("DE camera owner refused ") + method,
                              tail = ": " + std::to_string(static_cast<unsigned>(status));
            refused = text.find(head) != std::string::npos && text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }
        Check(refused, "camera SDK propagates exact current provider refusal, never a default value or native retry");
    }
    void Cases()
    {
        Access               access;
        CameraBasicsObserver camera(access);
        CCamera&             sdk = camera;
        for (unsigned value = 0; value != 256; ++value)
        {
            access.active = static_cast<std::uint8_t>(value);
            const auto calls = access.activeCalls;
            Check(sdk.GetActiveCam() == value && access.activeCalls == calls + 1 && !access.fovCalls && !access.restoreCalls,
                  "actual GetActiveCam forwards every rawBYTE once without clamp or GetCam/FOV shortcut");
        }
        for (unsigned count = 0; count != 3; ++count)
        {
            const auto calls = access.restoreCalls;
            const auto active = access.activeCalls;
            sdk.Restore();
            Check(access.restoreCalls == calls + 1 && access.activeCalls == active && !access.fovCalls,
                  "ordinary actual Restore dispatches once with no SDK extras, JumpCut or scalar repair");
        }
        constexpr std::array<std::uint32_t, 22> values{0,           0x80000000u, 1,           0x80000001u, 0x007fffffu, 0x807fffffu, 0x00800000u, 0x80800000u,
                                                       0x3f800000u, 0xbf800000u, 0x428c0000u, 0x43340000u, 0x437a0000u, 0xc37a0000u, 0x7f7fffffu, 0xff7fffffu,
                                                       0x7f800000u, 0xff800000u, 0x7fc00001u, 0xffc00001u, 0x7f800001u, 0xff800001u};
        for (const auto row : std::array<std::uint32_t, 6>{0, 1, 2, 3, 0x10000u, 0xffffffffu})
        {
            CamFovObserver cam(row, access);
            const CCam&    view = cam;
            for (const auto bits : values)
            {
                access.rawFov = bits;
                const auto calls = access.fovCalls, active = access.activeCalls, restores = access.restoreCalls;
                const auto expected = (bits & 0x7f800000u) == 0x7f800000u ? 0x428c0000u : bits;
                Check(Bits(view.GetFOV()) == expected && access.fovCalls == calls + 1 && access.row == row && access.activeCalls == active &&
                          access.restoreCalls == restores,
                      "actual const GetFOV forwards full logicalDWORD once, preserves finite bits and maps admitted nonfinite only to70");
            }
        }
        CamFovObserver cam(0xffffffffu, access);
        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            access.status = status;
            access.active = 255;
            access.rawFov = 0x7fc00001u;
            auto active = access.activeCalls;
            auto fov = access.fovCalls;
            auto restores = access.restoreCalls;
            Refuses([&] { sdk.GetActiveCam(); }, "GetActiveCam", status);
            Check(access.activeCalls == active + 1 && access.fovCalls == fov && access.restoreCalls == restores,
                  "failed active read makes exactly one selected provider call");
            active = access.activeCalls;
            fov = access.fovCalls;
            restores = access.restoreCalls;
            Refuses([&] { cam.GetFOV(); }, "GetFOV", status);
            Check(access.fovCalls == fov + 1 && access.row == 0xffffffffu && access.activeCalls == active && access.restoreCalls == restores,
                  "failed rawFOV never becomes70 and never truncates or resolves the immutable logical index");
            active = access.activeCalls;
            fov = access.fovCalls;
            restores = access.restoreCalls;
            Refuses([&] { sdk.Restore(); }, "Restore", status);
            Check(access.restoreCalls == restores + 1 && access.activeCalls == active && access.fovCalls == fov,
                  "failed Restore makes no replay or alternate camera operation");
        }
        Missing              missing;
        CameraBasicsObserver oldCamera(missing);
        CamFovObserver       oldCam(0, missing);
        Refuses([&] { oldCamera.GetActiveCam(); }, "GetActiveCam", ClockResult::NotInitialized);
        Refuses([&] { oldCam.GetFOV(); }, "GetFOV", ClockResult::NotInitialized);
        Refuses([&] { oldCamera.Restore(); }, "Restore", ClockResult::NotInitialized);
        access.status = ClockResult::Ok;
        CVector center{};
        for (const bool visible : {false, true})
        {
            access.visible = visible;
            const auto calls = access.sphereCalls;
            Check(static_cast<const CCamera&>(camera).IsSphereVisible(&center, 1.0f) == visible && access.sphereCalls == calls + 1,
                  "existing actual const sphere method stays on this same provider");
        }
        bool       invalid = false;
        const auto sphere = access.sphereCalls;
        try
        {
            camera.IsSphereVisible(nullptr, 1.0f);
        }
        catch (const std::invalid_argument&)
        {
            invalid = true;
        }
        Check(invalid && access.sphereCalls == sphere, "existing null-center refusal stays before any native provider call");
        std::puts("Actual camera basics SDK fixture completed");
        std::puts("Actual Cam finite FOV SDK fixture completed");
        std::puts("Actual camera provider refusal transport fixture completed");
    }
}
int main()
{
    try
    {
        Cases();
        std::printf("Passed %u camera basics SDK checks (%u-bit; transport only)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

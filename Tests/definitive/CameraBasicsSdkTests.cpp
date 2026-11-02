/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual camera Restore/selector and Cam finite FOV SDK policies
 ****************************************************************************/
#include "../../Client/game_de/CCameraSphereVisibleDE.h"
#include "../../Client/game_de/CCameraCamsDE.h"
#include "../../Client/game_de/CCamFovDE.h"
#include "../../Client/game_de/CCamRowDE.h"
#include "../../Client/game_de/CCamTargetDE.h"
#include "../../Client/game_de/CCamTargetSetterDE.h"
#include "../../Client/game_de/CPedPooledDE.h"
#include "../../Client/game_de/CPoolsDE.h"
#include "../../Client/game_de/CPedVitalsDE.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include "../../Client/game_de/NativeClassChainAccess.h"
#include <SharedUtil.IntTypes.h>
#include <game/CPedIK.h>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <thread>
#include "CameraBasicsFixture.generated.h"
#include "CameraCamsFixture.generated.h"
#include "CamFovFixture.generated.h"
#include "CamRowFixture.generated.h"
#include "CamTargetFixture.generated.h"
#include "CamTargetSetterFixture.generated.h"
#include "PedPooledFixture.generated.h"
using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&CameraBasicsObserver::RestoreWithJumpCut), decltype(&CCameraSphereVisibleDE::RestoreWithJumpCut)>);
static_assert(std::is_same_v<decltype(&CameraBasicsObserver::Restore), decltype(&CCameraSphereVisibleDE::Restore)>);
static_assert(std::is_same_v<decltype(&CameraBasicsObserver::GetActiveCam), decltype(&CCameraSphereVisibleDE::GetActiveCam)>);
static_assert(std::is_same_v<decltype(&CameraBasicsObserver::IsSphereVisible), decltype(&CCameraSphereVisibleDE::IsSphereVisible)>);
static_assert(std::is_same_v<decltype(&CamFovObserver::GetMode), decltype(&CCamFovDE::GetMode)>);
static_assert(std::is_same_v<decltype(&CamFovObserver::GetDirection), decltype(&CCamFovDE::GetDirection)>);
static_assert(std::is_same_v<decltype(&CamFovObserver::SetDirection), decltype(&CCamFovDE::SetDirection)>);
static_assert(std::is_same_v<decltype(&CamFovObserver::SetFOV), decltype(&CCamFovDE::SetFOV)>);
static_assert(std::is_same_v<decltype(&CamFovObserver::GetFOV), decltype(&CCamFovDE::GetFOV)>);
static_assert(std::is_abstract_v<CCameraSphereVisibleDE> && std::is_abstract_v<CCamFovDE>);
static_assert(std::is_same_v<decltype(&CamRowObserver::GetTargetHistoryPos), decltype(&CCamRowDE::GetTargetHistoryPos)>);
static_assert(std::is_abstract_v<CCamRowDE> && !std::is_abstract_v<CamRowObserver>);
static_assert(!std::is_copy_constructible_v<CamRowObserver> && !std::is_copy_assignable_v<CamRowObserver> && !std::is_move_constructible_v<CamRowObserver> &&
              !std::is_move_assignable_v<CamRowObserver>);
static_assert(!std::has_virtual_destructor_v<CCamRowDE> && std::is_nothrow_destructible_v<CamRowObserver>);
static_assert(std::is_standard_layout_v<CVector> && sizeof(CVector) == 3 * sizeof(float) && alignof(CVector) == alignof(float));
static_assert(std::is_same_v<decltype(&CamRowObserver::GetFront), decltype(&CCamRowDE::GetFront)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::GetUp), decltype(&CCamRowDE::GetUp)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::GetSource), decltype(&CCamRowDE::GetSource)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::GetFixedModeSource), decltype(&CCamRowDE::GetFixedModeSource)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::GetFixedModeVector), decltype(&CCamRowDE::GetFixedModeVector)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::GetFOV), decltype(&CCamFovDE::GetFOV)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::SetFOV), decltype(&CCamFovDE::SetFOV)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::GetMode), decltype(&CCamFovDE::GetMode)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::GetDirection), decltype(&CCamFovDE::GetDirection)>);
static_assert(std::is_same_v<decltype(&CamRowObserver::SetDirection), decltype(&CCamFovDE::SetDirection)>);
static_assert(std::is_same_v<decltype(&CamTargetObserver::GetTargetEntity), decltype(&CCamTargetDE::GetTargetEntity)>);
static_assert(std::is_same_v<decltype(&camera_target_fixture::Ped::GetInterface), decltype(&CPedPooledDE::GetInterface)>);
static_assert(std::is_abstract_v<CCamTargetDE> && std::is_abstract_v<CPedPooledDE> && !std::is_abstract_v<CamTargetObserver>);
static_assert(std::is_convertible_v<camera_target_fixture::Ped*, CPed*> && !std::is_copy_constructible_v<CamTargetObserver> &&
              !std::is_move_constructible_v<CamTargetObserver>);
static_assert(std::is_same_v<decltype(&CamTargetSetterObserver::SetTargetEntity), decltype(&CCamTargetSetterDE::SetTargetEntity)>);
static_assert(std::is_abstract_v<CCamTargetDE> && !std::is_abstract_v<CCamTargetSetterDE> && !std::is_abstract_v<CamTargetSetterObserver>);
static_assert(!std::is_copy_constructible_v<CamTargetSetterObserver> && !std::is_move_constructible_v<CamTargetSetterObserver> &&
              !std::has_virtual_destructor_v<CamTargetSetterObserver>);
static_assert(std::is_same_v<decltype(&CameraCamsObserver::GetCam), decltype(&CCameraCamsDE::GetCam)>);
static_assert(std::is_abstract_v<CCameraCamsDE> && !std::is_abstract_v<CameraCamsObserver> && !std::has_virtual_destructor_v<CameraCamsObserver>);
static_assert(!std::is_copy_constructible_v<CameraCamsObserver> && !std::is_move_constructible_v<CameraCamsObserver>);
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
        unsigned      activeCalls = 0, fovCalls = 0, restoreCalls = 0, jumpCutCalls = 0, sphereCalls = 0;
        std::uint32_t row = 0, rawFov = 0, writeRow = 0, writeBits = 0;
        unsigned      fovWriteCalls = 0;
        std::uint8_t  active = 0;
        bool          visible = false;
        ClockResult   modeStatus = ClockResult::Ok, directionReadStatus = ClockResult::Ok, directionRepairStatus = ClockResult::Ok,
                    directionWriteStatus = ClockResult::Ok;
        unsigned      modeCalls = 0, directionReads = 0, directionRepairs = 0, directionWrites = 0;
        std::uint16_t mode = 0;
        std::uint32_t modeRow = 0, directionReadRow = 0, directionRepairRow = 0, directionWriteRow = 0;
        std::uint32_t originalH = 0, originalV = 0, repairExpectedH = 0, repairExpectedV = 0, repairH = 0, repairV = 0, writtenH = 0, writtenV = 0;
        float*        outputH = nullptr;
        float*        outputV = nullptr;
        std::uint32_t watchedH = 0, watchedV = 0;
        bool          outputsUnchanged = true, changeRoundingOnRepair = false, roundingChanged = false;
        void          Watch(float& h, float& v) noexcept
        {
            outputH = &h;
            outputV = &v;
            watchedH = Bits(h);
            watchedV = Bits(v);
            outputsUnchanged = true;
        }
        void ObserveOutputs() noexcept
        {
            if (outputH && outputV)
                outputsUnchanged = outputsUnchanged && Bits(*outputH) == watchedH && Bits(*outputV) == watchedV;
        }
        ClockResult ReadCameraMode(std::uint32_t index, std::uint16_t& value) noexcept override
        {
            ++modeCalls;
            modeRow = index;
            value = mode;
            return modeStatus;
        }
        ClockResult ReadCameraDirection(std::uint32_t index, std::uint32_t& h, std::uint32_t& v) noexcept override
        {
            ++directionReads;
            directionReadRow = index;
            ObserveOutputs();
            h = originalH;
            v = originalV;
            ObserveOutputs();
            return directionReadStatus;
        }
        ClockResult ConditionalRepairCameraDirection(std::uint32_t index, std::uint32_t expectedH, std::uint32_t expectedV, std::uint32_t h,
                                                     std::uint32_t v) noexcept override
        {
            ++directionRepairs;
            directionRepairRow = index;
            repairExpectedH = expectedH;
            repairExpectedV = expectedV;
            repairH = h;
            repairV = v;
            ObserveOutputs();
            if (changeRoundingOnRepair)
                roundingChanged = std::fesetround(FE_UPWARD) == 0;
            return directionRepairStatus;
        }
        ClockResult WriteCameraDirection(std::uint32_t index, std::uint32_t h, std::uint32_t v) noexcept override
        {
            ++directionWrites;
            directionWriteRow = index;
            writtenH = h;
            writtenV = v;
            return directionWriteStatus;
        }

        ClockResult ReadCameraActiveIndex(std::uint8_t& value) noexcept override
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
        ClockResult WriteCameraFov(std::uint32_t index, std::uint32_t bits) noexcept override
        {
            ++fovWriteCalls;
            writeRow = index;
            writeBits = bits;
            return status;
        }
        ClockResult RestoreCamera() noexcept override
        {
            ++restoreCalls;
            return status;
        }
        ClockResult RestoreCameraWithJumpCut() noexcept override
        {
            ++jumpCutCalls;
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
    float FromBits(std::uint32_t bits)
    {
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::array<unsigned, 5> OtherCalls(const Access& access)
    {
        return {access.activeCalls, access.fovCalls, access.restoreCalls, access.jumpCutCalls, access.sphereCalls};
    }
    void FovWrites()
    {
        // These are shared SDK policy tests, not native row admission. Full
        // logical indices must reach the selected owner without truncation,
        // clamping or consulting the active camera or the read-only getter.
        constexpr std::array<std::uint32_t, 6>  rows{0, 1, 2, 3, 0x10000u, 0xffffffffu};
        constexpr std::array<std::uint32_t, 10> valid{0x00800000u, 0x00800001u, 0x3e800000u, 0x3f000001u, 0x3f800000u,
                                                      0x3f800001u, 0x42480000u, 0x428c0000u, 0x4333fffeu, 0x4333ffffu};
        constexpr std::array<std::uint32_t, 18> invalid{0,           0x80000000u, 0x80000001u, 0xbf800000u, 0xc3340000u, 0x43340000u,
                                                        0x43340001u, 0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc00001u,
                                                        0xffc00001u, 0x7f800001u, 0xff800001u, 0x437a0000u, 0xc37a0000u, 0xc28c0000u};
        constexpr std::array<ClockResult, 14>   refusals{ClockResult::InvalidArgument, ClockResult::NotInitialized,   ClockResult::WrongThread,
                                                       ClockResult::BindingChanged,  ClockResult::ReadFailure,      ClockResult::WriteFailure,
                                                       ClockResult::RestoreFailure,  ClockResult::PatchFailure,     ClockResult::Busy,
                                                       ClockResult::Expired,         ClockResult::UnsupportedBuild, ClockResult::OwnerConflict,
                                                       ClockResult::Stopped,         ClockResult::OriginalException};
        Access                                  access;
        for (const auto row : rows)
        {
            CamFovObserver camera(row, access);
            CCam&          sdk = camera;
            access.status = ClockResult::Ok;
            for (const auto bits : valid)
            {
                float      value = FromBits(bits);
                const auto calls = access.fovWriteCalls;
                const auto other = OtherCalls(access);
                sdk.SetFOV(value);
                Check(access.fovWriteCalls == calls + 1 && access.writeRow == row && access.writeBits == bits && OtherCalls(access) == other &&
                          Bits(value) == bits,
                      "actual base CCam SetFOV forwards valid raw float bits and the immutable full row once, without a read/selector/alternate operation");
            }
            for (const auto status : {ClockResult::Ok, ClockResult::NotInitialized, ClockResult::OriginalException})
            {
                access.status = status;
                for (const auto bits : invalid)
                {
                    const auto calls = access.fovWriteCalls;
                    const auto other = OtherCalls(access);
                    const auto priorRow = access.writeRow, priorBits = access.writeBits;
                    const auto value = FromBits(bits);
                    sdk.SetFOV(value);
                    Check(access.fovWriteCalls == calls && OtherCalls(access) == other && access.writeRow == priorRow && access.writeBits == priorBits,
                          "zero, negative, boundary180 and nonfinite SDK inputs are no-provider no-ops even when the retained provider would fail");
                }
            }
            for (const auto status : refusals)
            {
                access.status = status;
                for (const auto bits : {valid.front(), valid.back()})
                {
                    const auto calls = access.fovWriteCalls;
                    const auto other = OtherCalls(access);
                    Refuses([&] { sdk.SetFOV(FromBits(bits)); }, "SetFOV", status);
                    Check(access.fovWriteCalls == calls + 1 && access.writeRow == row && access.writeBits == bits && OtherCalls(access) == other,
                          "every valid SetFOV refusal propagates after exactly one selected owner call, without a readback, native retry or fallback");
                }
            }
            Missing        missing;
            CamFovObserver oldCamera(row, missing);
            CCam&          oldSdk = oldCamera;
            for (const auto bits : invalid)
            {
                bool completed = false;
                try
                {
                    oldSdk.SetFOV(FromBits(bits));
                    completed = true;
                }
                catch (const std::exception&)
                {
                }
                Check(completed, "invalid SetFOV remains a no-op before the default-refusing missing provider could throw");
            }
            for (const auto bits : {valid.front(), valid.back()})
                Refuses([&] { oldSdk.SetFOV(FromBits(bits)); }, "SetFOV", ClockResult::NotInitialized);
        }
        std::puts("Actual Cam SetFOV SDK policy fixture completed");
    }

    std::array<unsigned, 10> CameraCalls(const Access& access)
    {
        return {access.activeCalls,   access.fovCalls,  access.restoreCalls,   access.jumpCutCalls,     access.sphereCalls,
                access.fovWriteCalls, access.modeCalls, access.directionReads, access.directionRepairs, access.directionWrites};
    }
    bool OnlyCalled(const Access& access, std::array<unsigned, 10> before, unsigned index)
    {
        ++before[index];
        return CameraCalls(access) == before;
    }
    // This reference is the saved actual CCamSA source policy, independent of
    // the DE implementation. In particular, huge finite inputs are not given
    // an invented clamp if the original float Wrap arithmetic overflows.
    __declspec(noinline) float ClassicWrap(float angle)
    {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = 6.28318530717958647692f;
        angle -= kTwoPi * std::floor((angle + kPi) / kTwoPi);
        if (angle <= -kPi)
            angle += kTwoPi;
        else if (angle > kPi)
            angle -= kTwoPi;
        return angle;
    }
    std::uint32_t WrappedBits(std::uint32_t original)
    {
        const float value = FromBits(original);
        return Bits(std::isfinite(value) ? ClassicWrap(value) : 0.0f);
    }
    struct LegacyDirectionAccess final : NativeCameraSphereVisibleAccess
    {
        unsigned    calls = 0;
        ClockResult ReadCameraDirection(std::uint32_t, std::uint32_t& h, std::uint32_t& v) noexcept override
        {
            ++calls;
            h = 0x41a00000u;
            v = 0x7fc12345u;
            return ClockResult::Ok;
        }
    };
    void Directions()
    {
        constexpr std::array<std::uint32_t, 6>                 rows{0, 1, 2, 3, 0x10000u, 0xffffffffu};
        constexpr std::array<std::array<std::uint32_t, 2>, 12> modes{{{0, 0},
                                                                      {1, 1},
                                                                      {4, 4},
                                                                      {0x7fu, 0x7fu},
                                                                      {0x100u, 0x100u},
                                                                      {0x1234u, 0x1234u},
                                                                      {0x7fffu, 0x7fffu},
                                                                      {0x8000u, 0xffff8000u},
                                                                      {0x8001u, 0xffff8001u},
                                                                      {0xff00u, 0xffffff00u},
                                                                      {0xfffeu, 0xfffffffeu},
                                                                      {0xffffu, 0xffffffffu}}};
        constexpr std::array<std::array<std::uint32_t, 2>, 10> finite{{{0, 0x80000000u},
                                                                       {0x40490fdbu, 0xc0490fdbu},
                                                                       {0x40c90fdbu, 0xc0c90fdbu},
                                                                       {0x40490fdau, 0x40490fdcu},
                                                                       {0x3f800000u, 0xbf800000u},
                                                                       {0x41a00000u, 0xc1a00000u},
                                                                       {0x47c35000u, 0xc7c35000u},
                                                                       {0x7f7fffffu, 0xff7fffffu},
                                                                       {0x7f000000u, 0xff000000u},
                                                                       {0x00800000u, 0x80800000u}}};
        constexpr std::array<std::array<std::uint32_t, 2>, 6>  repair{{{0x7fc12345u, 0x3f800000u},
                                                                       {0x41a00000u, 0xffc12345u},
                                                                       {0x7f800000u, 0xc1a00000u},
                                                                       {0, 0xff800000u},
                                                                       {0x7f800001u, 0x80000000u},
                                                                       {0x7fc00001u, 0xffc00001u}}};
        constexpr std::array<std::array<std::uint32_t, 2>, 8>  invalidSet{{{0x7fc12345u, 0x41a00000u},
                                                                           {0x41a00000u, 0xffc12345u},
                                                                           {0xff800000u, 0x3f800000u},
                                                                           {0x3f800000u, 0x7f800000u},
                                                                           {0x7f800001u, 0xff800001u},
                                                                           {0x7f7fffffu, 0xffc00001u},
                                                                           {0x80000000u, 0xff800000u},
                                                                           {0x7fc00001u, 0x80000000u}}};
        constexpr std::array<ClockResult, 14>                  statuses{ClockResult::InvalidArgument, ClockResult::NotInitialized,   ClockResult::WrongThread,
                                                       ClockResult::BindingChanged,  ClockResult::ReadFailure,      ClockResult::WriteFailure,
                                                       ClockResult::RestoreFailure,  ClockResult::PatchFailure,     ClockResult::Busy,
                                                       ClockResult::Expired,         ClockResult::UnsupportedBuild, ClockResult::OwnerConflict,
                                                       ClockResult::Stopped,         ClockResult::OriginalException};
        for (const auto row : rows)
        {
            Access         access;
            CamFovObserver camera(row, access);
            CCam&          sdk = camera;
            for (const auto& sample : modes)
            {
                access.mode = static_cast<std::uint16_t>(sample[0]);
                const auto before = CameraCalls(access);
                Check(static_cast<const CCam&>(sdk).GetMode() == sample[1] && OnlyCalled(access, before, 6) && access.modeRow == row,
                      "actual GetMode preserves the signed-short source conversion, full WORD patterns and immutable uint32 row without enum clamping");
            }
            for (const auto status : statuses)
            {
                access.modeStatus = status;
                const auto before = CameraCalls(access);
                Refuses([&] { sdk.GetMode(); }, "GetMode", status);
                Check(OnlyCalled(access, before, 6) && access.modeRow == row, "mode refusal stays one raw observation, never a default mode0");
            }
            Missing        missing;
            CamFovObserver oldCamera(row, missing);
            Refuses([&] { oldCamera.GetMode(); }, "GetMode", ClockResult::NotInitialized);
            for (const auto& sample : finite)
            {
                access.originalH = sample[0];
                access.originalV = sample[1];
                const auto expectedH = WrappedBits(sample[0]), expectedV = WrappedBits(sample[1]);
                float      h = -11.0f, v = 12.0f;
                access.Watch(h, v);
                const auto before = CameraCalls(access);
                sdk.GetDirection(h, v);
                Check(
                    OnlyCalled(access, before, 7) && access.directionReadRow == row && access.outputsUnchanged && Bits(h) == expectedH && Bits(v) == expectedV,
                    "finite native originals wrap only in SDK outputs with one raw read and no native repair, including huge finite source arithmetic");
            }
            for (const auto& sample : repair)
            {
                access.originalH = sample[0];
                access.originalV = sample[1];
                const auto expectedH = WrappedBits(sample[0]), expectedV = WrappedBits(sample[1]);
                float      h = -11.0f, v = 12.0f;
                access.Watch(h, v);
                auto expectedCalls = CameraCalls(access);
                ++expectedCalls[7];
                ++expectedCalls[8];
                sdk.GetDirection(h, v);
                Check(CameraCalls(access) == expectedCalls && access.directionReadRow == row && access.directionRepairRow == row &&
                          access.repairExpectedH == sample[0] && access.repairExpectedV == sample[1] && access.repairH == expectedH &&
                          access.repairV == expectedV && access.outputsUnchanged && Bits(h) == expectedH && Bits(v) == expectedV,
                      "either original nonfinite computes both targets once and sends one original-pair conditional repair before either output is assigned");
                float alias = 19.0f;
                access.Watch(alias, alias);
                expectedCalls = CameraCalls(access);
                ++expectedCalls[7];
                ++expectedCalls[8];
                sdk.GetDirection(alias, alias);
                Check(CameraCalls(access) == expectedCalls && access.outputsUnchanged && Bits(alias) == expectedV,
                      "aliased repair outputs retain actual H-then-V assignment after repair acknowledgement");
            }
            access.originalH = 0x41a00000u;
            access.originalV = 0xc1a00000u;
            float alias = 19.0f;
            access.Watch(alias, alias);
            const auto beforeAlias = CameraCalls(access);
            sdk.GetDirection(alias, alias);
            Check(OnlyCalled(access, beforeAlias, 7) && access.outputsUnchanged && Bits(alias) == WrappedBits(access.originalV),
                  "finite aliased outputs also finish with the wrapped vertical value without native repair");
            access.outputH = access.outputV = nullptr;
            for (const auto& sample : finite)
            {
                const auto expectedH = WrappedBits(sample[0]), expectedV = WrappedBits(sample[1]);
                const auto before = CameraCalls(access);
                sdk.SetDirection(FromBits(sample[0]), FromBits(sample[1]));
                Check(OnlyCalled(access, before, 9) && access.directionWriteRow == row && access.writtenH == expectedH && access.writtenV == expectedV,
                      "valid finite setter inputs forward exact original Wrap results, even generated nonfinite bits, without a read or second filter");
            }
            for (const auto status : {ClockResult::Ok, ClockResult::NotInitialized, ClockResult::OriginalException})
            {
                access.directionWriteStatus = status;
                for (const auto& sample : invalidSet)
                {
                    const auto before = CameraCalls(access);
                    sdk.SetDirection(FromBits(sample[0]), FromBits(sample[1]));
                    Check(CameraCalls(access) == before, "either nonfinite setter argument is a no-provider no-op before source wrapping");
                }
            }
            for (const auto& sample : invalidSet)
            {
                bool completed = false;
                try
                {
                    oldCamera.SetDirection(FromBits(sample[0]), FromBits(sample[1]));
                    completed = true;
                }
                catch (const std::exception&)
                {
                }
                Check(completed, "invalid setter arguments do not consult the default-refusing missing direction provider");
            }
            for (const auto status : statuses)
            {
                access.directionReadStatus = status;
                access.originalH = 0x41a00000u;
                access.originalV = 0x7fc12345u;
                float h = -11.0f, v = 12.0f;
                access.Watch(h, v);
                const auto before = CameraCalls(access);
                Refuses([&] { sdk.GetDirection(h, v); }, "GetDirection", status);
                Check(OnlyCalled(access, before, 7) && access.outputsUnchanged && h == -11.0f && v == 12.0f,
                      "raw read failure preserves both caller outputs even when the refusing provider wrote its local raw output pair");
            }
            access.directionReadStatus = ClockResult::Ok;
            for (const auto status : statuses)
            {
                access.directionRepairStatus = status;
                float h = -11.0f, v = 12.0f;
                access.Watch(h, v);
                auto expectedCalls = CameraCalls(access);
                ++expectedCalls[7];
                ++expectedCalls[8];
                Refuses([&] { sdk.GetDirection(h, v); }, "GetDirection", status);
                Check(CameraCalls(access) == expectedCalls && access.repairExpectedH == access.originalH && access.repairExpectedV == access.originalV &&
                          access.outputsUnchanged && h == -11.0f && v == 12.0f,
                      "unacknowledged conditional repair preserves both outputs and never retries, falls back to zero or enters the independent setter");
            }
            access.outputH = access.outputV = nullptr;
            LegacyDirectionAccess legacy;
            CamFovObserver        legacyCamera(row, legacy);
            float                 legacyH = -11.0f, legacyV = 12.0f;
            Refuses([&] { legacyCamera.GetDirection(legacyH, legacyV); }, "GetDirection", ClockResult::NotInitialized);
            Check(legacy.calls == 1 && legacyH == -11.0f && legacyV == 12.0f,
                  "an older raw reader cannot pretend its missing conditional-repair channel acknowledged the nonfinite originals");
            Refuses([&] { oldCamera.GetDirection(legacyH, legacyV); }, "GetDirection", ClockResult::NotInitialized);
            Check(legacyH == -11.0f && legacyV == 12.0f, "a missing raw direction provider is a refusal rather than the classic null-interface fallback");
            for (const auto status : statuses)
            {
                access.directionWriteStatus = status;
                const auto before = CameraCalls(access);
                Refuses([&] { sdk.SetDirection(20.0f, -20.0f); }, "SetDirection", status);
                Check(OnlyCalled(access, before, 9) && access.directionWriteRow == row && access.writtenH == WrappedBits(0x41a00000u) &&
                          access.writtenV == WrappedBits(0xc1a00000u),
                      "valid setter failure propagates after exactly one selected pair write without getter repair or replay");
            }
            Refuses([&] { oldCamera.SetDirection(20.0f, -20.0f); }, "SetDirection", ClockResult::NotInitialized);
        }
        {
            struct RoundingScope
            {
                int previous = std::fegetround();
                ~RoundingScope() { std::fesetround(previous); }
            } rounding;
            Check(std::fesetround(FE_TONEAREST) == 0, "focused output-order check owns and later restores the test process rounding state");
            Access access;
            access.originalH = 0x41a00000u;
            access.originalV = 0x7fc12345u;
            access.changeRoundingOnRepair = true;
            CamFovObserver camera(2, access);
            float          h = -11.0f, v = 12.0f;
            access.Watch(h, v);
            camera.GetDirection(h, v);
            Check(access.directionReads == 1 && access.directionRepairs == 1 && access.roundingChanged && access.outputsUnchanged &&
                      Bits(h) == access.repairH && Bits(v) == access.repairV,
                  "repair ACK changes FP rounding, but SDK publishes the already computed replacement bits rather than recomputing policy afterward");
        }
        std::puts("Actual Cam mode and direction SDK policy fixture completed");
    }

    struct RowAccess final : NativeCameraSphereVisibleAccess
    {
        NativeCameraRowBorrow           ledger;
        NativeCameraRowBorrow::Identity identity;
        std::array<CVector, 5>          vectors{}, replacements{};
        std::array<CVector, 4>          history{}, historyReplacements{};
        ClockResult   opened = ClockResult::NotInitialized, acquireStatus = ClockResult::Ok, vectorStatus = ClockResult::Ok, releaseStatus = ClockResult::Ok;
        unsigned      acquireCalls = 0, vectorCalls = 0, releaseCalls = 0, scalarCalls = 0;
        std::uint32_t acquireRow = 0, vectorRow = 0, scalarRow = 0, writtenFov = 0, writtenH = 0, writtenV = 0;
        std::uint64_t acquiredId = 0, vectorId = 0, releasedId = 0;
        CameraRowVectorKind vectorKind = CameraRowVectorKind::Front;
        bool                emptySuccess = false, nullSuccess = false, useReplacements = false;
        RowAccess() noexcept
        {
            identity = {1, 1, reinterpret_cast<std::uintptr_t>(vectors.data()), GetCurrentThreadId()};
            opened = ledger.Open(identity);
        }
        static ClockResult Current() noexcept { return ClockResult::Ok; }
        ClockResult        AcquireCameraRowBorrow(std::uint32_t row, NativeCameraRowBorrow::Token& token) noexcept override
        {
            ++acquireCalls;
            acquireRow = row;
            if (acquireStatus != ClockResult::Ok || emptySuccess)
                return acquireStatus;
            const auto result = ledger.Acquire(identity, row, &Current, token);
            if (result == ClockResult::Ok)
                acquiredId = token.Id();
            return result;
        }
        ClockResult ReleaseCameraRowBorrow(NativeCameraRowBorrow::Token& token) noexcept override
        {
            ++releaseCalls;
            releasedId = token.Id();
            return releaseStatus == ClockResult::Ok ? ledger.Release(token) : releaseStatus;
        }
        ClockResult ReadCameraRowVector(const NativeCameraRowBorrow::Token& token, CameraRowVectorKind kind, CVector*& output) noexcept override
        {
            ++vectorCalls;
            vectorId = token.Id();
            vectorRow = token.Row();
            vectorKind = kind;
            if (vectorStatus != ClockResult::Ok)
                return vectorStatus;
            const auto result = ledger.Validate(token, identity);
            if (result != ClockResult::Ok)
                return result;
            if (kind == CameraRowVectorKind::TargetHistory)
            {
                output = nullSuccess ? nullptr : (useReplacements ? historyReplacements.data() : history.data());
                return ClockResult::Ok;
            }
            const auto index = static_cast<std::uint32_t>(kind);
            if (index >= vectors.size())
                return ClockResult::InvalidArgument;
            output = nullSuccess ? nullptr : &(useReplacements ? replacements : vectors)[index];
            return ClockResult::Ok;
        }
        ClockResult ReadCameraFov(std::uint32_t row, std::uint32_t& bits) noexcept override
        {
            ++scalarCalls;
            scalarRow = row;
            bits = 0x428c0000u;
            return ClockResult::Ok;
        }
        ClockResult WriteCameraFov(std::uint32_t row, std::uint32_t bits) noexcept override
        {
            ++scalarCalls;
            scalarRow = row;
            writtenFov = bits;
            return ClockResult::Ok;
        }
        ClockResult ReadCameraMode(std::uint32_t row, std::uint16_t& word) noexcept override
        {
            ++scalarCalls;
            scalarRow = row;
            word = 0xffffu;
            return ClockResult::Ok;
        }
        ClockResult ReadCameraDirection(std::uint32_t row, std::uint32_t& h, std::uint32_t& v) noexcept override
        {
            ++scalarCalls;
            scalarRow = row;
            h = 0x3f800000u;
            v = 0xbf800000u;
            return ClockResult::Ok;
        }
        ClockResult WriteCameraDirection(std::uint32_t row, std::uint32_t h, std::uint32_t v) noexcept override
        {
            ++scalarCalls;
            scalarRow = row;
            writtenH = h;
            writtenV = v;
            return ClockResult::Ok;
        }
    };
    void RowBorrows()
    {
        // SDK transport uses owned vectors and the real CPU hold ledger. The
        // native owner suite separately qualifies engine storage and End gates.
        const auto before = checks;
        using Getter = CVector* (CCam::*)() const;
        struct Method
        {
            Getter              call;
            CameraRowVectorKind kind;
            const char*         name;
        };
        constexpr std::array<Method, 5>       methods{{{&CCam::GetFront, CameraRowVectorKind::Front, "GetFront"},
                                                       {&CCam::GetUp, CameraRowVectorKind::Up, "GetUp"},
                                                       {&CCam::GetSource, CameraRowVectorKind::Source, "GetSource"},
                                                       {&CCam::GetFixedModeSource, CameraRowVectorKind::FixedModeSource, "GetFixedModeSource"},
                                                       {&CCam::GetFixedModeVector, CameraRowVectorKind::FixedModeVector, "GetFixedModeVector"}}};
        constexpr std::array<ClockResult, 14> refusals{ClockResult::InvalidArgument, ClockResult::NotInitialized,   ClockResult::WrongThread,
                                                       ClockResult::BindingChanged,  ClockResult::ReadFailure,      ClockResult::WriteFailure,
                                                       ClockResult::RestoreFailure,  ClockResult::PatchFailure,     ClockResult::Busy,
                                                       ClockResult::Expired,         ClockResult::UnsupportedBuild, ClockResult::OwnerConflict,
                                                       ClockResult::Stopped,         ClockResult::OriginalException};
        for (std::uint32_t row = 0; row != 3; ++row)
        {
            RowAccess access;
            Check(access.opened == ClockResult::Ok, "the SDK fixture owns its producer CPU ledger and buffers");
            {
                CamRowObserver camera(row, access);
                CCam&          sdk = camera;
                Check(access.acquireCalls == 1 && access.acquireRow == row && access.acquiredId && access.ledger.IsRetained() && !access.vectorCalls,
                      "the actual row wrapper acquires its immutable row once before exposing any pointer");
                for (std::size_t index = 0; index != methods.size(); ++index)
                {
                    const auto& method = methods[index];
                    const auto  calls = access.vectorCalls;
                    auto*       pointer = (sdk.*method.call)();
                    Check(pointer == &access.vectors[index] && access.vectorCalls == calls + 1 && access.vectorId == access.acquiredId &&
                              access.vectorKind == method.kind && access.vectorRow == row && access.acquireCalls == 1,
                          "each base SDK getter borrows the selected provider vector and exact issued token once");
                    const std::array<std::uint32_t, 3> raw{0x80000000u, 0x7fc12345u, 0x7f800000u};
                    std::memcpy(&pointer->fX, &raw[0], sizeof(float));
                    std::memcpy(&pointer->fY, &raw[1], sizeof(float));
                    std::memcpy(&pointer->fZ, &raw[2], sizeof(float));
                    Check(Bits(access.vectors[index].fX) == raw[0] && Bits(access.vectors[index].fY) == raw[1] && Bits(access.vectors[index].fZ) == raw[2],
                          "returned pointers alias writable provider components without caches or finite-vector normalization");
                    access.useReplacements = true;
                    Check((sdk.*method.call)() == &access.replacements[index] && access.vectorCalls == calls + 2,
                          "every getter reconsults its owner instead of returning a saved pointer");
                    access.useReplacements = false;
                }
                const auto scalars = access.scalarCalls;
                Check(Bits(sdk.GetFOV()) == 0x428c0000u && access.scalarCalls == scalars + 1 && access.scalarRow == row,
                      "the retained row preserves the same inherited GetFOV owner and logical row");
                sdk.SetFOV(80.0f);
                Check(access.writtenFov == Bits(80.0f) && access.scalarCalls == scalars + 2 && access.scalarRow == row,
                      "the retained row preserves the inherited FOV setter");
                Check(sdk.GetMode() == 0xffffffffu && access.scalarCalls == scalars + 3 && access.scalarRow == row,
                      "the retained row preserves inherited signed mode transport");
                float h = 0, v = 0;
                sdk.GetDirection(h, v);
                Check(Bits(h) == 0x3f800000u && Bits(v) == 0xbf800000u && access.scalarCalls == scalars + 4,
                      "the retained row preserves inherited finite direction observation");
                sdk.SetDirection(1.0f, -1.0f);
                Check(access.writtenH == 0x3f800000u && access.writtenV == 0xbf800000u && access.scalarCalls == scalars + 5,
                      "the retained row preserves inherited ordered direction delegation");
                access.ledger.CloseIssuance();
                const auto vectors = access.vectorCalls;
                Refuses([&] { sdk.GetFront(); }, "GetFront", ClockResult::Stopped);
                Check(access.vectorCalls == vectors + 1 && access.ledger.IsRetained() && !access.releaseCalls,
                      "Stop revokes pointer publication while preserving the existing wrapper's hold");
                Refuses([&] { CamRowObserver later(row, access); }, "AcquireCameraRowBorrow", ClockResult::Stopped);
                Check(access.acquireCalls == 2 && access.releaseCalls == 0 && access.ledger.IsRetained(),
                      "a refused constructor neither releases the live row nor invents another token");
            }
            Check(access.releaseCalls == 1 && access.releasedId == access.acquiredId && !access.ledger.IsRetained(),
                  "typed same-producer destruction releases exactly the issued hold after Stop");
        }
        for (const auto row : {3u, 0x10000u, 0xffffffffu})
        {
            RowAccess access;
            Refuses([&] { CamRowObserver camera(row, access); }, "AcquireCameraRowBorrow", ClockResult::InvalidArgument);
            Check(access.acquireCalls == 1 && access.acquireRow == row && !access.vectorCalls && !access.releaseCalls && !access.ledger.IsRetained(),
                  "the full invalid logical DWORD reaches acquisition unchanged, with no partially constructed client");
        }
        {
            Missing missing;
            Refuses([&] { CamRowObserver camera(0, missing); }, "AcquireCameraRowBorrow", ClockResult::NotInitialized);
        }
        {
            RowAccess access;
            access.emptySuccess = true;
            Refuses([&] { CamRowObserver camera(0, access); }, "AcquireCameraRowBorrow", ClockResult::BindingChanged);
            Check(access.acquireCalls == 1 && !access.vectorCalls && !access.releaseCalls && !access.ledger.IsRetained(),
                  "an empty successful token cannot manufacture a retained camera client");
        }
        for (const auto status : refusals)
        {
            RowAccess access;
            access.acquireStatus = status;
            Refuses([&] { CamRowObserver camera(1, access); }, "AcquireCameraRowBorrow", status);
            Check(access.acquireCalls == 1 && access.acquireRow == 1 && !access.vectorCalls && !access.releaseCalls && !access.ledger.IsRetained(),
                  "each acquisition refusal propagates once before pointer or destructor activity");
        }
        {
            RowAccess access;
            {
                CamRowObserver camera(0, access);
                const CCam&    sdk = camera;
                const auto     original = access.vectors;
                for (const auto status : refusals)
                {
                    access.vectorStatus = status;
                    for (const auto& method : methods)
                    {
                        const auto calls = access.vectorCalls;
                        Refuses([&] { (sdk.*method.call)(); }, method.name, status);
                        Check(access.vectorCalls == calls + 1 && access.vectorId == access.acquiredId && access.vectorKind == method.kind &&
                                  access.releaseCalls == 0 && access.ledger.IsRetained() &&
                                  std::memcmp(original.data(), access.vectors.data(), sizeof(original)) == 0,
                              "each getter refusal preserves native buffers and its live token without replay, release or fallback");
                    }
                }
                access.vectorStatus = ClockResult::Ok;
                access.nullSuccess = true;
                for (const auto& method : methods)
                {
                    const auto calls = access.vectorCalls;
                    Refuses([&] { (sdk.*method.call)(); }, method.name, ClockResult::BindingChanged);
                    Check(access.vectorCalls == calls + 1 && !access.releaseCalls && access.ledger.IsRetained(),
                          "an acquired row refuses a null successful pointer without fabricating vector storage");
                }
            }
            Check(access.releaseCalls == 1 && access.releasedId == access.acquiredId && !access.ledger.IsRetained(),
                  "getter errors do not bypass or duplicate the final producer release");
        }
        {
            RowAccess access;
            {
                CamRowObserver camera(2, access);
                access.releaseStatus = ClockResult::WrongThread;
            }
            Check(access.releaseCalls == 1 && access.releasedId == access.acquiredId && access.ledger.IsRetained(),
                  "a refused destructor release is not retried or converted into a false dropped hold");
        }
        if (checks - before != 270)
            throw std::runtime_error("row SDK case-count contract changed");
        std::puts("Actual retained Cam row vector SDK fixture completed");
    }

    struct TargetAccess final : NativeGameTimingAccess
    {
        NativeCameraRowBorrow           ledger;
        NativeCameraRowBorrow::Identity identity;
        NativePedReferenceToken         reference{19, 77, 37};
        NativePedPoolReader::Value      nativePed;
        std::array<unsigned char, 64>   nativeBytes{};
        ClockResult    opened = ClockResult::NotInitialized, targetStatus = ClockResult::Ok, poolStatus = ClockResult::Ok, referenceStatus = ClockResult::Ok;
        unsigned       acquires = 0, releases = 0, targetReads = 0, poolChecks = 0, resolutions = 0, nativeAcquires = 0, nativeReleases = 0;
        std::uint32_t  acquiredRow = 0, targetRow = 0;
        std::uint64_t  acquiredId = 0, targetId = 0, releasedId = 0;
        std::uintptr_t target = 0;
        unsigned       targetSeam = 0;
        std::function<void()>   afterTarget;
        ClockResult             setterStatus = ClockResult::Ok;
        unsigned                setCalls = 0, resolutionSeam = 0;
        std::uint32_t           setRow = 0;
        std::uint64_t           setId = 0;
        bool                    clearIntent = false;
        NativePedReferenceToken setIntent;
        std::function<void()>   afterReference;
        std::function<void()>   afterAcquire;
        ClockResult             publicationStatus = ClockResult::Ok, drainStatus = ClockResult::Ok;
        unsigned                publicationChecks = 0, drainChecks = 0;
        std::uint32_t           publicationRow = 0, drainRefusedRow = 0xffffffffu;
        std::uint64_t           publicationId = 0;
        TargetAccess() noexcept
        {
            identity = {1, 1, reinterpret_cast<std::uintptr_t>(nativeBytes.data()), GetCurrentThreadId()};
            opened = ledger.Open(identity);
            nativePed.handle = 1;
            nativePed.slot = 0;
            nativePed.pool = reinterpret_cast<std::uintptr_t>(this);
            nativePed.objects = reinterpret_cast<std::uintptr_t>(nativeBytes.data());
            nativePed.flags = nativePed.objects + 1;
            nativePed.nativePed = nativePed.objects;
            nativePed.vtable = nativePed.objects + 8;
            nativePed.firstMethod = nativePed.objects + 16;
            nativePed.capacity = static_cast<std::int32_t>(NativePedPoolProfile::NativeCapacity);
            nativePed.cursor = -1;
            nativePed.generation = 1;
            nativePed.ownsAllocations = 1;
        }
        static bool SameToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b) noexcept
        {
            return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
        }
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        ClockResult CheckPedPoolAccess() noexcept override
        {
            ++poolChecks;
            return poolStatus;
        }
        ClockResult ResolvePedReference(const NativePedReferenceToken& token, NativePedPoolReader::Value& output) noexcept override
        {
            ++resolutions;
            if (poolStatus != ClockResult::Ok)
                return poolStatus;
            if (referenceStatus != ClockResult::Ok)
                return referenceStatus;
            if (!SameToken(token, reference))
                return ClockResult::OwnerConflict;
            output = nativePed;
            if (afterReference && resolutions == resolutionSeam)
            {
                auto action = std::move(afterReference);
                afterReference = {};
                action();
            }
            return ClockResult::Ok;
        }
        ClockResult RecheckPedPoolHandle(const NativePedPoolReader::Value& expected) noexcept override
        {
            if (poolStatus != ClockResult::Ok)
                return poolStatus;
            return expected.handle == nativePed.handle && expected.nativePed == nativePed.nativePed ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        ClockResult AcquirePedReference(std::uint32_t, NativePedReferenceToken&) noexcept override
        {
            ++nativeAcquires;
            return ClockResult::NotInitialized;
        }
        ClockResult ReleasePedReference(const NativePedReferenceToken&) noexcept override
        {
            ++nativeReleases;
            return ClockResult::NotInitialized;
        }
        ClockResult AcquireCameraRowBorrow(std::uint32_t row, NativeCameraRowBorrow::Token& token) noexcept override
        {
            ++acquires;
            acquiredRow = row;
            const auto result = ledger.AcquireChecked(identity, row, []() noexcept { return ClockResult::Ok; }, token);
            if (result == ClockResult::Ok)
            {
                acquiredId = token.Id();
                if (afterAcquire)
                {
                    auto action = std::move(afterAcquire);
                    afterAcquire = {};
                    action();
                }
            }
            return result;
        }
        ClockResult ReleaseCameraRowBorrow(NativeCameraRowBorrow::Token& token) noexcept override
        {
            ++releases;
            releasedId = token.Id();
            return ledger.Release(token);
        }
        ClockResult ValidateCameraRowBorrow(const NativeCameraRowBorrow::Token& token) noexcept override
        {
            ++publicationChecks;
            publicationRow = token.Row();
            publicationId = token.Id();
            return publicationStatus == ClockResult::Ok ? ledger.Validate(token, identity) : publicationStatus;
        }
        ClockResult CheckCameraRowBorrowDrain(const NativeCameraRowBorrow::Token& token) noexcept override
        {
            ++drainChecks;
            if (GetCurrentThreadId() != identity.producerThread)
                return ClockResult::WrongThread;
            if (token.Row() == drainRefusedRow && drainStatus != ClockResult::Ok)
                return drainStatus;
            const auto result = ledger.Validate(token, identity);
            return result == ClockResult::Stopped ? ClockResult::Ok : (result == ClockResult::Ok ? ClockResult::Busy : result);
        }
        ClockResult SetCameraRowTarget(const NativeCameraRowBorrow::Token& token, const NativePedReferenceToken* intent) noexcept override
        {
            ++setCalls;
            setRow = token.Row();
            setId = token.Id();
            clearIntent = intent == nullptr;
            setIntent = intent ? *intent : NativePedReferenceToken{};
            if (setterStatus != ClockResult::Ok)
                return setterStatus;
            const auto result = ledger.Validate(token, identity);
            if (result != ClockResult::Ok)
                return result;
            if (intent && !SameToken(*intent, reference))
                return ClockResult::OwnerConflict;
            // This is only SDK transport over opaque owned tags. The native
            // owner suite exercises actual reference surgery and effect debt.
            target = intent ? nativePed.nativePed : 0;
            return ClockResult::Ok;
        }
        ClockResult ReadCameraRowTarget(const NativeCameraRowBorrow::Token& token, std::uintptr_t& output) noexcept override
        {
            ++targetReads;
            targetRow = token.Row();
            targetId = token.Id();
            if (targetStatus != ClockResult::Ok)
                return targetStatus;
            const auto result = ledger.Validate(token, identity);
            if (result != ClockResult::Ok)
                return result;
            output = target;
            if (afterTarget && targetReads == targetSeam)
            {
                auto action = std::move(afterTarget);
                afterTarget = {};
                action();
            }
            return ClockResult::Ok;
        }
    };
    template <class Call>
    void PedInterfaceRefuses(Call call, ClockResult status)
    {
        bool refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            refused = error.what() == std::string("DE ped pool owner refused GetInterface: ") + std::to_string(static_cast<unsigned>(status));
        }
        Check(refused, "actual CEntity GetInterface preserves exact inverse-registry refusal");
    }
    unsigned sdkClassChainReads = 0;
    bool     DenySdkClassChainRead(std::uintptr_t, void*, std::size_t) noexcept
    {
        ++sdkClassChainReads;
        return false;
    }
    void ClassChainIncludeBoundary()
    {
        NativeClassChainAccess chain(&DenySdkClassChainRead);
        for (bool sentinel : {false, true})
        {
            sdkClassChainReads = 0;
            bool output = sentinel;
            Check(!chain.IsChildOf(0x1000, 0x2000, output) && output == sentinel && sdkClassChainReads == (sizeof(std::uintptr_t) == 8 ? 1u : 0u),
                  "portable inclusion grants no x86 chain reads; x64 retains guarded refusal and both preserve outputs");
        }
    }
    void PooledInterfaces()
    {
        const auto                        before = checks;
        TargetAccess                      owner, foreign;
        camera_target_fixture::Pools      pools(owner), foreignPools(foreign);
        camera_target_fixture::Ped        ped(owner, owner.reference, pools), foreignPed(owner, owner.reference, foreignPools);
        CPoolsDE::BorrowedPedRegistration registration;
        CEntity&                          entity = ped;
        Check(owner.opened == ClockResult::Ok && pools.HasNativeOwner(owner) && !pools.HasNativeOwner(foreign),
              "pool owner identity compares the same typed endpoint, not numeric admission or different base addresses");  // 1
        PedInterfaceRefuses([&] { entity.GetInterface(); }, ClockResult::NotInitialized);                                  // 2
        Check(pools.BindBorrowedPed(ped, nullptr, registration) == ClockResult::Ok && registration.IsBound(),
              "a genuine composed SDK ped reuses the controlled existing registration");  // 3
        Check(static_cast<void*>(static_cast<CPed*>(&ped)) != static_cast<void*>(&ped),
              "the concrete receiver exercises actual C++ virtual-base adjustment");  // 4
        Check(entity.GetInterface() == reinterpret_cast<CEntitySAInterface*>(owner.nativePed.nativePed),
              "actual entity virtual dispatch returns only the freshly checked opaque native tag");  // 5
        Check(owner.nativeAcquires == 0 && owner.nativeReleases == 0 && pools.GetPedCount() == 1,
              "GetInterface does not acquire a cell, adopt an entity, or add a registry row");  // 6
        PedInterfaceRefuses([&] { foreignPed.GetInterface(); }, ClockResult::OwnerConflict);    // 7
        for (auto status : {ClockResult::WrongThread, ClockResult::Stopped, ClockResult::ReadFailure, ClockResult::BindingChanged})
        {
            owner.referenceStatus = status;
            CEntitySAInterface* output = nullptr;
            PedInterfaceRefuses([&] { output = entity.GetInterface(); }, status);  // 4 * 2
            Check(output == nullptr && registration.IsBound() && pools.GetPedCount() == 1,
                  "a refused interface read publishes no pointer and leaves its registration owned by the caller");
        }
        owner.referenceStatus = ClockResult::Ok;
        Check(registration.Reset() == ClockResult::Ok && !registration.IsBound(), "CPU withdrawal precedes typed ped destruction");  // 16
        PedInterfaceRefuses([&] { entity.GetInterface(); }, ClockResult::NotInitialized);                                            // 17
        Check(checks - before == 17, "pooled-interface transport case count remains explicit");                                      // 18
        std::puts("Actual ped pooled interface SDK fixture completed");
    }
    void TargetEntities()
    {
        const auto                            before = checks;
        constexpr std::array<ClockResult, 14> refusals{ClockResult::InvalidArgument, ClockResult::NotInitialized,   ClockResult::WrongThread,
                                                       ClockResult::BindingChanged,  ClockResult::ReadFailure,      ClockResult::WriteFailure,
                                                       ClockResult::RestoreFailure,  ClockResult::PatchFailure,     ClockResult::Busy,
                                                       ClockResult::Expired,         ClockResult::UnsupportedBuild, ClockResult::OwnerConflict,
                                                       ClockResult::Stopped,         ClockResult::OriginalException};
        for (std::uint32_t row = 0; row < 3; ++row)
        {
            TargetAccess                      owner;
            camera_target_fixture::Pools      pools(owner);
            camera_target_fixture::Ped        ped(owner, owner.reference, pools);
            CPoolsDE::BorrowedPedRegistration registration;
            Check(owner.opened == ClockResult::Ok, "target transport owns its CPU ledger and opaque native tags");                                       // 1
            Check(pools.BindBorrowedPed(ped, nullptr, registration) == ClockResult::Ok, "target fixture registers the actual shared-CPed composition");  // 2
            {
                CamTargetObserver camera(row, pools, owner);
                const CCam&       sdk = camera;
                Check(owner.acquires == 1 && owner.acquiredRow == row && owner.acquiredId && owner.ledger.IsRetained(),
                      "the target slice acquires exactly one existing row hold");  // 3
                const auto poolChecks = owner.poolChecks;
                Check(sdk.GetTargetEntity() == nullptr && owner.targetReads == 2 && owner.poolChecks == poolChecks,
                      "only stable qualified raw zero returns null, without a pool lookup");  // 4
                owner.target = owner.nativePed.nativePed;
                owner.targetReads = 0;
                CEntity* const expected = static_cast<CEntity*>(static_cast<CPed*>(&ped));
                Check(sdk.GetTargetEntity() == expected, "known targets return the actual registered SDK entity base, never native bits as an object");  // 5
                Check(owner.targetReads == 3 && owner.targetRow == row && owner.targetId == owner.acquiredId,
                      "all target bookends reuse the same current retained row and owner token");  // 6
                Check(expected->GetInterface() == reinterpret_cast<CEntitySAInterface*>(owner.target),
                      "target SDK identity and actual GetInterface share the current inverse registration");  // 7
                Check(owner.nativeAcquires == 0 && owner.nativeReleases == 0 && pools.GetPedCount() == 1,
                      "target observation has no native-reference, entity-factory or registration effect");  // 8
                owner.target += 1;
                CEntity* output = expected;
                Refuses([&] { output = sdk.GetTargetEntity(); }, "GetTargetEntity", ClockResult::NotInitialized);                 // 9
                Check(output == expected, "unknown nonzero target refuses without null success or foreign-pointer dereference");  // 10
                for (bool startsNull : {true, false})
                {
                    owner.target = startsNull ? 0 : owner.nativePed.nativePed;
                    owner.targetReads = 0;
                    owner.targetSeam = 1;
                    owner.afterTarget = [&] { owner.target = startsNull ? owner.nativePed.nativePed : 0; };
                    output = expected;
                    Refuses([&] { output = sdk.GetTargetEntity(); }, "GetTargetEntity", ClockResult::BindingChanged);  // 2 * 2
                    Check(output == expected, "zero/nonzero transition cannot become a plausible null or mismatched SDK entity");
                }
                owner.target = owner.nativePed.nativePed;
                for (auto status : refusals)
                {
                    owner.targetStatus = status;
                    owner.targetReads = 0;
                    output = expected;
                    Refuses([&] { output = sdk.GetTargetEntity(); }, "GetTargetEntity", status);  // 14 * 2
                    Check(output == expected && owner.targetReads == 1 && owner.ledger.IsRetained(),
                          "raw target provider refusal publishes nothing and retains the caller's existing row hold");
                }
                owner.targetStatus = ClockResult::Ok;
                owner.poolStatus = ClockResult::WrongThread;
                output = expected;
                Refuses([&] { output = sdk.GetTargetEntity(); }, "GetTargetEntity", ClockResult::WrongThread);  // 43
                Check(output == expected, "pool authority refusal is not unknown-target absence");              // 44
                owner.poolStatus = ClockResult::Ok;
                owner.referenceStatus = ClockResult::NotInitialized;
                Refuses([&] { output = sdk.GetTargetEntity(); }, "GetTargetEntity", ClockResult::NotInitialized);                                     // 45
                Check(output == expected && registration.IsBound(), "stale or freed native reference refuses while CPU ownership remains explicit");  // 46
                owner.referenceStatus = ClockResult::Ok;
                owner.ledger.CloseIssuance();
                Refuses([&] { sdk.GetTargetEntity(); }, "GetTargetEntity", ClockResult::Stopped);                                                    // 47
                Check(owner.ledger.IsRetained() && owner.releases == 0, "Stop closes observation but does not discard the live typed row wrapper");  // 48
            }
            Check(owner.releases == 1 && owner.releasedId == owner.acquiredId && !owner.ledger.IsRetained(),
                  "same-producer typed destruction releases the one retained row after Stop");  // 49
        }
        {
            TargetAccess                 owner, foreign;
            camera_target_fixture::Pools pools(owner);
            Refuses([&] { CamTargetObserver camera(0, pools, foreign); }, "CCamTargetDE constructor", ClockResult::OwnerConflict);  // 148
            Check(owner.acquires == 0 && foreign.acquires == 0 && !owner.ledger.IsRetained() && !foreign.ledger.IsRetained(),
                  "foreign registry owner is rejected before either owner can acquire a row");  // 149
        }
        {
            TargetAccess                              owner;
            camera_target_fixture::Pools              pools(owner);
            std::optional<camera_target_fixture::Ped> ped;
            ped.emplace(owner, owner.reference, pools);
            CPoolsDE::BorrowedPedRegistration registration;
            Check(pools.BindBorrowedPed(*ped, nullptr, registration) == ClockResult::Ok, "ABA fixture starts with one actual controlled SDK object");  // 150
            CPed* const original = &*ped;
            const auto  version = owner.reference.version;
            const auto  native = owner.nativePed.nativePed;
            bool        reset = false, rebound = false;
            {
                CamTargetObserver camera(0, pools, owner);
                owner.target = native;
                owner.targetSeam = 3;
                owner.afterTarget = [&]
                {
                    // Reuse the real concrete observer's storage only after
                    // retiring its CPU guard. Same SDK/native addresses cannot
                    // hide the new immutable reference token at the last seam.
                    reset = registration.Reset() == ClockResult::Ok;
                    ped.reset();
                    ++owner.reference.version;
                    ped.emplace(owner, owner.reference, pools);
                    rebound = pools.BindBorrowedPed(*ped, nullptr, registration) == ClockResult::Ok;
                };
                CEntity* output = nullptr;
                Refuses([&] { output = static_cast<const CCam&>(camera).GetTargetEntity(); }, "GetTargetEntity", ClockResult::BindingChanged);  // 151
                Check(reset && rebound && static_cast<CPed*>(&*ped) == original && owner.nativePed.nativePed == native && owner.reference.version != version,
                      "the seam replaces only registration/token identity while genuine SDK/native address values recur");  // 152
                Check(output == nullptr && owner.targetReads == 3 && owner.ledger.IsRetained(),
                      "the final inverse-token bookend refuses ABA before publishing an SDK entity");  // 153
            }
            Check(owner.releases == 1 && !owner.ledger.IsRetained(), "ABA refusal still releases the original camera hold once");  // 154
            Check(registration.Reset() == ClockResult::Ok && !registration.IsBound(),
                  "replacement registration is withdrawn before its typed observer dies");  // 155
        }
        Check(checks - before == 155, "bounded target transport case count preserves all prior camera cases");  // 156
        std::puts("Actual camera target SDK fixture completed");
    }

    void TargetSetters()
    {
        const auto                            before = checks;
        constexpr std::array<ClockResult, 14> refusals{ClockResult::InvalidArgument, ClockResult::NotInitialized,   ClockResult::WrongThread,
                                                       ClockResult::BindingChanged,  ClockResult::ReadFailure,      ClockResult::WriteFailure,
                                                       ClockResult::RestoreFailure,  ClockResult::PatchFailure,     ClockResult::Busy,
                                                       ClockResult::Expired,         ClockResult::UnsupportedBuild, ClockResult::OwnerConflict,
                                                       ClockResult::Stopped,         ClockResult::OriginalException};
        for (std::uint32_t row = 0; row < 3; ++row)
        {
            TargetAccess                      owner;
            camera_target_fixture::Pools      pools(owner);
            camera_target_fixture::Ped        ped(owner, owner.reference, pools), unregistered(owner, owner.reference, pools);
            CPoolsDE::BorrowedPedRegistration registration;
            Check(owner.opened == ClockResult::Ok, "setter transport owns its ledger and opaque native tags");  // 1
            Check(pools.BindBorrowedPed(ped, nullptr, registration) == ClockResult::Ok,
                  "setter receiver uses the existing actual controlled ped registry");  // 2
            CEntity* const entity = static_cast<CEntity*>(static_cast<CPed*>(&ped));
            {
                CamTargetSetterObserver camera(row, pools, owner);
                CCam&                   sdk = camera;
                Check(owner.acquires == 1 && owner.acquiredRow == row && owner.acquiredId && owner.ledger.IsRetained(),
                      "setter composition retains exactly the base's one row hold");  // 3
                sdk.SetTargetEntity(nullptr);
                Check(owner.setCalls == 1 && owner.clearIntent && owner.target == 0 && owner.setRow == row && owner.setId == owner.acquiredId,
                      "actual null setter dispatches an explicit clear with the same held row token");  // 4
                sdk.SetTargetEntity(entity);
                Check(owner.setCalls == 2 && !owner.clearIntent && TargetAccess::SameToken(owner.setIntent, owner.reference) &&
                          owner.target == owner.nativePed.nativePed,
                      "actual CEntity identity becomes a copied current CPU token rather than a raw native interface");  // 5
                sdk.SetTargetEntity(entity);
                Check(owner.setCalls == 3 && owner.nativeAcquires == 0 && owner.nativeReleases == 0 && pools.GetPedCount() == 1,
                      "same-target intent still reaches the native owner; SDK invents neither a reference effect nor a lease");  // 6
                sdk.SetTargetEntity(nullptr);
                Check(owner.setCalls == 4 && owner.clearIntent && owner.target == 0,
                      "clearing follows the actual MTA null request instead of treating it as a no-op");  // 7
                auto        calls = owner.setCalls;
                auto* const unknown = reinterpret_cast<CEntity*>(owner.nativeBytes.data() + 17);
                Refuses([&] { sdk.SetTargetEntity(unknown); }, "SetTargetEntity", ClockResult::NotInitialized);  // 8
                Check(owner.setCalls == calls && owner.target == 0 && pools.GetPedCount() == 1,
                      "an opaque unknown/nonped entity identity is never dereferenced or silently turned into clear intent");  // 9
                Refuses([&] { sdk.SetTargetEntity(static_cast<CEntity*>(static_cast<CPed*>(&unregistered))); }, "SetTargetEntity",
                        ClockResult::NotInitialized);                                                                                            // 10
                Check(owner.setCalls == calls, "an actual but unregistered SDK ped cannot manufacture setter provenance through GetInterface");  // 11
                for (auto status : refusals)
                {
                    owner.setterStatus = status;
                    calls = owner.setCalls;
                    Refuses([&] { sdk.SetTargetEntity(entity); }, "SetTargetEntity", status);  // 14 * 2
                    Check(owner.setCalls == calls + 1 && owner.target == 0 && owner.ledger.IsRetained() && registration.IsBound(),
                          "exact provider refusal propagates once without replay or changing CPU registration/row ownership");
                }
                owner.setterStatus = ClockResult::Ok;
                owner.poolStatus = ClockResult::WrongThread;
                calls = owner.setCalls;
                Refuses([&] { sdk.SetTargetEntity(entity); }, "SetTargetEntity", ClockResult::WrongThread);     // 40
                Check(owner.setCalls == calls, "registry authority refusal stays before native setter entry");  // 41
                owner.poolStatus = ClockResult::Ok;
                owner.referenceStatus = ClockResult::NotInitialized;
                Refuses([&] { sdk.SetTargetEntity(entity); }, "SetTargetEntity", ClockResult::NotInitialized);   // 42
                Check(owner.setCalls == calls, "withdrawn/freed native reference cannot become setter intent");  // 43
                owner.referenceStatus = ClockResult::Ok;
                auto* const pair = pools.GetPed(std::size_t{0});
                pair->pEntity = reinterpret_cast<CPed*>(owner.nativeBytes.data() + 23);
                Refuses([&] { sdk.SetTargetEntity(entity); }, "SetTargetEntity", ClockResult::BindingChanged);                      // 44
                Check(owner.setCalls == calls, "mutable canonical pair provenance is checked before any virtual-base conversion");  // 45
                pair->pEntity = &ped;
                CPed*                   selected = &unregistered;
                NativePedReferenceToken token{999, 999, 999};
                Check(pools.ReadBorrowedPedEntityReference(entity, selected, token) == ClockResult::Ok && selected == static_cast<CPed*>(&ped) &&
                          TargetAccess::SameToken(token, owner.reference),
                      "bounded actual-entity inverse returns the current controlled SDK/token pair");  // 46
                selected = &unregistered;
                token = {999, 999, 999};
                Check(pools.ReadBorrowedPedEntityReference(unknown, selected, token) == ClockResult::NotInitialized &&
                          selected == static_cast<CPed*>(&unregistered) && token.version == 999 && token.ownerGeneration == 999 && token.slot == 999,
                      "unknown entity inverse preserves both outputs on refusal");  // 47
                Check(pools.ReadBorrowedPedEntityReference(nullptr, selected, token) == ClockResult::InvalidArgument &&
                          selected == static_cast<CPed*>(&unregistered) && token.version == 999 && token.ownerGeneration == 999 && token.slot == 999,
                      "null inverse is invalid; only the actual SDK setter selects explicit clear");  // 48
                Check(registration.Reset() == ClockResult::Ok && !registration.IsBound(),
                      "caller withdraws the existing CPU registration before destroying its wrapper");               // 49
                Refuses([&] { sdk.SetTargetEntity(entity); }, "SetTargetEntity", ClockResult::NotInitialized);       // 50
                Check(owner.setCalls == calls, "a previously registered SDK identity is refused after withdrawal");  // 51
                sdk.SetTargetEntity(nullptr);
                Check(owner.setCalls == calls + 1 && owner.clearIntent && owner.target == 0,
                      "explicit clear does not require inventing a replacement registered ped");  // 52
                owner.ledger.CloseIssuance();
                Refuses([&] { sdk.SetTargetEntity(nullptr); }, "SetTargetEntity", ClockResult::Stopped);                                             // 53
                Check(owner.ledger.IsRetained() && owner.releases == 0, "Stop refuses new effects while the typed camera still owns its row hold");  // 54
            }
            Check(owner.releases == 1 && owner.releasedId == owner.acquiredId && !owner.ledger.IsRetained(),
                  "typed setter receiver destruction releases one hold without unregistering the engine target cell");  // 55
        }
        {
            TargetAccess                 owner, foreign;
            camera_target_fixture::Pools pools(owner);
            Refuses([&] { CamTargetSetterObserver camera(0, pools, foreign); }, "CCamTargetDE constructor", ClockResult::OwnerConflict);  // 166
            Check(owner.acquires == 0 && foreign.acquires == 0, "foreign owner fails before either receiver can acquire a row");          // 167
        }
        {
            TargetAccess                      owner, foreign;
            camera_target_fixture::Pools      pools(owner), foreignPools(foreign);
            camera_target_fixture::Ped        foreignPed(foreign, foreign.reference, foreignPools);
            CPoolsDE::BorrowedPedRegistration registration;
            Check(foreignPools.BindBorrowedPed(foreignPed, nullptr, registration) == ClockResult::Ok,
                  "foreign test entity is a genuine controlled SDK ped owned by its distinct registry");  // 168
            {
                CamTargetSetterObserver camera(0, pools, owner);
                Refuses([&] { camera.SetTargetEntity(static_cast<CEntity*>(static_cast<CPed*>(&foreignPed))); }, "SetTargetEntity",
                        ClockResult::NotInitialized);  // 169
                Check(owner.setCalls == 0 && foreign.setCalls == 0 && pools.GetPedCount() == 0,
                      "a registered foreign entity never crosses the exact owner/registry boundary");  // 170
            }
            Check(owner.releases == 1 && !owner.ledger.IsRetained(), "foreign-entity refusal still retires the receiver hold normally");  // 171
        }
        {
            Missing                      missing;
            NativeCameraRowBorrow::Token token;
            Check(missing.SetCameraRowTarget(token, nullptr) == ClockResult::NotInitialized,
                  "older providers default-refuse the new effectful endpoint instead of inventing a clear");  // 172
        }
        {
            TargetAccess                              owner;
            camera_target_fixture::Pools              pools(owner);
            std::optional<camera_target_fixture::Ped> ped;
            ped.emplace(owner, owner.reference, pools);
            CPoolsDE::BorrowedPedRegistration registration;
            Check(pools.BindBorrowedPed(*ped, nullptr, registration) == ClockResult::Ok,
                  "setter ABA test starts from the existing genuine SDK registration");  // 173
            CPed* const    original = &*ped;
            CEntity* const entity = static_cast<CEntity*>(original);
            const auto     version = owner.reference.version;
            bool           reset = false, rebound = false;
            {
                CamTargetSetterObserver camera(0, pools, owner);
                owner.resolutionSeam = owner.resolutions + 1;
                owner.afterReference = [&]
                {
                    reset = registration.Reset() == ClockResult::Ok;
                    ped.reset();
                    ++owner.reference.version;
                    ped.emplace(owner, owner.reference, pools);
                    rebound = pools.BindBorrowedPed(*ped, nullptr, registration) == ClockResult::Ok;
                };
                Refuses([&] { camera.SetTargetEntity(entity); }, "SetTargetEntity", ClockResult::BindingChanged);  // 174
                Check(reset && rebound && static_cast<CPed*>(&*ped) == original && owner.reference.version != version,
                      "CPU/native address recurrence does not hide replacement registration during inverse qualification");          // 175
                Check(owner.setCalls == 0 && owner.ledger.IsRetained(), "registration ABA refuses before any native setter entry");  // 176
            }
            Check(owner.releases == 1 && !owner.ledger.IsRetained(), "setter ABA refusal releases only its existing camera hold");  // 177
            Check(registration.Reset() == ClockResult::Ok && !registration.IsBound(),
                  "replacement CPU registration is withdrawn before its typed wrapper dies");  // 178
        }
        Check(checks - before == 178, "setter SDK transport adds a finite case group and preserves all earlier camera cases");  // 179
        std::puts("Actual camera target setter SDK fixture completed");
    }

    void CameraProducts()
    {
        const auto before = checks;
        {
            TargetAccess                 owner;
            camera_target_fixture::Pools pools(owner);
            CameraCamsObserver           camera(pools, owner);
            CCamera&                     sdk = camera;
            Check(owner.acquires == 0 && !owner.ledger.IsRetained(), "partial camera construction is CPU-only and eagerly acquires no row");  // 1
            for (unsigned id = 3; id <= 255; ++id)
                Check(sdk.GetCam(static_cast<BYTE>(id)) == nullptr && owner.acquires == 0 && owner.publicationChecks == 0,
                      "every out-of-range actual BYTE ID preserves classic null bounds without provider access");                                       // 253
            Check(camera.DrainCamsOnProducer() == ClockResult::Busy && owner.releases == 0, "publication must close before producer drain can start");  // 255
            std::array<CCam*, 3> products{};
            for (BYTE row = 0; row < 3; ++row)
            {
                products[row] = sdk.GetCam(row);
                const auto acquireCalls = owner.acquires, validationCalls = owner.publicationChecks;
                Check(products[row] && acquireCalls == static_cast<unsigned>(row) + 1 && owner.publicationRow == row && owner.publicationId == owner.acquiredId,
                      "each actual CCam product owns its row once and validates the held token before publication");  // 3 * 3
                Check(sdk.GetCam(row) == products[row] && owner.acquires == acquireCalls && owner.publicationChecks == validationCalls + 1,
                      "cached GetCam returns stable SDK identity only after fresh same-owner validation");
                const auto sets = owner.setCalls, reads = owner.targetReads;
                products[row]->SetTargetEntity(nullptr);
                Check(products[row]->GetTargetEntity() == nullptr && owner.setCalls == sets + 1 && owner.targetReads == reads + 2 && owner.setRow == row,
                      "products are the actual completed target-setter SDK rows, not forwarding test products or raw native casts");
            }
            Check(products[0] != products[1] && products[0] != products[2] && products[1] != products[2],
                  "three logical rows retain three distinct actual SDK objects");  // 265
            owner.publicationStatus = ClockResult::Stopped;
            Refuses([&] { sdk.GetCam(0); }, "GetCam", ClockResult::Stopped);  // 266
            Check(owner.acquires == 3 && owner.releases == 0 && owner.ledger.IsRetained(),
                  "refused cached publication does not destroy or reacquire its row");  // 267
            owner.publicationStatus = ClockResult::Ok;
            camera.CloseCamPublication();
            const auto validations = owner.publicationChecks;
            Refuses([&] { sdk.GetCam(0); }, "GetCam", ClockResult::Stopped);  // 268
            Check(owner.publicationChecks == validations && owner.releases == 0,
                  "closed publication refuses without another native read or implicit destruction");           // 269
            Check(sdk.GetCam(255) == nullptr, "bounds-first invalid IDs stay null after publication closes");  // 270
            Check(camera.DrainCamsOnProducer() == ClockResult::Busy && owner.releases == 0,
                  "close alone cannot manufacture native Stop/quiescence drain admission");  // 271
            owner.ledger.CloseIssuance();
            owner.drainRefusedRow = 1;
            owner.drainStatus = ClockResult::WrongThread;
            Check(camera.DrainCamsOnProducer() == ClockResult::WrongThread && owner.releases == 0 && owner.ledger.IsRetained(),
                  "a later cached-token refusal preserves all earlier products and all holds");  // 272
            owner.drainStatus = ClockResult::Ok;
            ClockResult foreignResult = ClockResult::Ok;
            std::thread foreign([&] { foreignResult = camera.DrainCamsOnProducer(); });
            foreign.join();
            Check(foreignResult == ClockResult::WrongThread && owner.releases == 0 && owner.ledger.IsRetained(),
                  "foreign thread cannot turn closed issuance into producer release authority");  // 273
            Check(camera.DrainCamsOnProducer() == ClockResult::Ok && owner.releases == 3 && !owner.ledger.IsRetained(),
                  "producer prechecks all three held rows before exactly three typed CPU releases");  // 274
            Check(camera.DrainCamsOnProducer() == ClockResult::Ok && owner.releases == 3,
                  "completed producer drain is idempotent without replaying releases");  // 275
            Refuses([&] { sdk.GetCam(0); }, "GetCam", ClockResult::Stopped);             // 276
        }
        {
            Missing                      missing;
            NativeCameraRowBorrow::Token token;
            Check(missing.ValidateCameraRowBorrow(token) == ClockResult::NotInitialized &&
                      missing.CheckCameraRowBorrowDrain(token) == ClockResult::NotInitialized,
                  "older providers refuse both publication and producer drain rather than invent authority");  // 277
        }
        {
            TargetAccess                           owner;
            camera_target_fixture::Pools           pools(owner);
            CameraCamsObserver                     camera(pools, owner);
            std::optional<CamTargetSetterObserver> blocker;
            blocker.emplace(1, pools, owner);
            Check(owner.acquires == 1 && owner.ledger.IsRetained(), "an independent actual row product occupies the one native row slot");  // 278
            CCam* const first = camera.GetCam(0);
            Check(first && owner.acquires == 2, "lazy row zero can publish independently of occupied row one");  // 279
            Refuses([&] { camera.GetCam(1); }, "AcquireCameraRowBorrow", ClockResult::Busy);                     // 280
            Check(camera.GetCam(0) == first && camera.GetCam(2) && owner.acquires == 4 && owner.releases == 0,
                  "a later row constructor failure neither unwinds earlier products nor prevents a different free row");  // 281
            owner.ledger.CloseIssuance();
            Refuses([&] { camera.GetCam(0); }, "GetCam", ClockResult::Stopped);                                                        // 282
            Check(owner.releases == 0 && owner.ledger.IsRetained(), "actual ledger Stop is never hidden by a cached GetCam pointer");  // 283
            camera.CloseCamPublication();
            blocker.reset();
            Check(owner.releases == 1 && owner.ledger.IsRetained(), "independent product retires on the producer while facade products remain owned");  // 284
            Check(camera.DrainCamsOnProducer() == ClockResult::Ok && owner.releases == 3 && !owner.ledger.IsRetained(),
                  "facade drains exactly its own two products after the independent hold retires");  // 285
        }
        {
            TargetAccess                 owner;
            camera_target_fixture::Pools pools(owner);
            CameraCamsObserver           camera(pools, owner);
            owner.publicationStatus = ClockResult::ReadFailure;
            Refuses([&] { camera.GetCam(0); }, "GetCam", ClockResult::ReadFailure);  // 286
            const auto id = owner.acquiredId;
            Check(owner.acquires == 1 && owner.releases == 0 && owner.ledger.IsRetained(),
                  "post-acquisition validation refusal leaves the new product safely cached for producer drain");  // 287
            owner.publicationStatus = ClockResult::Ok;
            Check(camera.GetCam(0) && owner.acquires == 1 && owner.publicationId == id,
                  "later admitted publication reuses the already owned product without a second acquisition");  // 288
            camera.CloseCamPublication();
            owner.ledger.CloseIssuance();
            Check(camera.DrainCamsOnProducer() == ClockResult::Ok && owner.releases == 1 && !owner.ledger.IsRetained(),
                  "initial validation failure does not strand its cached hold");  // 289
        }
        {
            TargetAccess                 owner;
            camera_target_fixture::Pools pools(owner);
            CameraCamsObserver           camera(pools, owner);
            owner.afterAcquire = [&] { camera.CloseCamPublication(); };
            Refuses([&] { camera.GetCam(0); }, "GetCam", ClockResult::Stopped);  // 290
            Check(owner.acquires == 1 && owner.publicationChecks == 1 && owner.releases == 0 && owner.ledger.IsRetained(),
                  "close during acquisition refuses raw publication only after noexcept transfer into typed cache ownership");  // 291
            Check(camera.GetCam(255) == nullptr, "close-race refusal preserves the independent invalid-ID null policy");        // 292
            owner.ledger.CloseIssuance();
            Check(camera.DrainCamsOnProducer() == ClockResult::Ok && owner.releases == 1 && !owner.ledger.IsRetained(),
                  "producer drain retires the unpublished product retained across the close race");  // 293
        }
        {
            TargetAccess                 owner, foreign;
            camera_target_fixture::Pools pools(owner);
            Refuses([&] { CameraCamsObserver camera(pools, foreign); }, "CCameraCamsDE constructor", ClockResult::OwnerConflict);  // 294
            Check(owner.acquires == 0 && foreign.acquires == 0 && owner.releases == 0 && foreign.releases == 0,
                  "foreign facade construction is refused while still CPU-only");  // 295
        }
        Check(checks - before == 295, "GetCam SDK cases preserve every earlier camera and target test");  // 296
        std::puts("Actual camera GetCam SDK fixture completed");
    }

    void HistoryPointers()
    {
        // The actual SDK exposes four writable CVector elements, not a copy
        // of a currently populated prefix. Native admission is tested by the
        // owner fixture; this transport fixture owns the entire 48-byte array.
        const auto before = checks;
        static_assert(sizeof(std::array<CVector, 4>) == 48);
        constexpr std::array<std::array<std::uint32_t, 3>, 4> patterns{
            {{0x80000000u, 0x7fc12345u, 0x7f800000u}, {1u, 0xff800000u, 0x3f800000u}, {0x007fffffu, 0xbf800001u, 0xffc12345u}, {0x7f7fffffu, 0x80800000u, 0u}}};
        for (std::uint32_t row = 0; row != 3; ++row)
        {
            RowAccess access;
            Check(access.opened == ClockResult::Ok, "history transport owns its CPU row ledger and full four-vector buffers");
            {
                CamRowObserver camera(row, access);
                const CCam&    sdk = camera;
                Check(access.acquireCalls == 1 && access.acquireRow == row && access.acquiredId && access.ledger.IsRetained(),
                      "history borrows the already acquired row without another lifetime token");
                const auto  vectorsBefore = access.vectors, replacementsBefore = access.replacements;
                const auto  untouchedHistory = access.historyReplacements;
                const auto  calls = access.vectorCalls;
                auto* const history = sdk.GetTargetHistoryPos();
                Check(history == access.history.data() && access.vectorCalls == calls + 1 && access.vectorKind == CameraRowVectorKind::TargetHistory &&
                          access.vectorId == access.acquiredId && access.vectorRow == row && access.acquireCalls == 1,
                      "actual const SDK history getter returns the same provider's first vector with the exact token and appended history kind");
                for (std::size_t index = 0; index != patterns.size(); ++index)
                {
                    const auto& raw = patterns[index];
                    std::memcpy(&history[index].fX, &raw[0], sizeof(float));
                    std::memcpy(&history[index].fY, &raw[1], sizeof(float));
                    std::memcpy(&history[index].fZ, &raw[2], sizeof(float));
                    Check(Bits(access.history[index].fX) == raw[0] && Bits(access.history[index].fY) == raw[1] && Bits(access.history[index].fZ) == raw[2] &&
                              reinterpret_cast<std::uintptr_t>(history + index) == reinterpret_cast<std::uintptr_t>(history) + index * sizeof(CVector),
                          "caller elements0/1 and the full four-vector SDK extent alias all component bits without normalization or cached storage");
                }
                Check(std::memcmp(vectorsBefore.data(), access.vectors.data(), sizeof(vectorsBefore)) == 0 &&
                          std::memcmp(replacementsBefore.data(), access.replacements.data(), sizeof(replacementsBefore)) == 0 &&
                          std::memcmp(untouchedHistory.data(), access.historyReplacements.data(), sizeof(untouchedHistory)) == 0,
                      "writes through the exact48-byte history span preserve the other provider vectors and replacement buffer");
                Check(
                    sdk.GetTargetHistoryPos() == history && access.vectorCalls == calls + 2 && access.acquireCalls == 1 && access.vectorId == access.acquiredId,
                    "repeated history access revalidates the same hold instead of acquiring another token");
                access.useReplacements = true;
                Check(sdk.GetTargetHistoryPos() == access.historyReplacements.data() && access.vectorCalls == calls + 3 && access.acquireCalls == 1 &&
                          Bits(access.history[0].fY) == patterns[0][1] && Bits(access.history[3].fX) == patterns[3][0],
                      "history getter reconsults its provider and never substitutes a saved pointer or cached vector contents");
                access.useReplacements = false;
                Check(sdk.GetFront() == &access.vectors[0] && access.vectorCalls == calls + 4 && access.vectorKind == CameraRowVectorKind::Front &&
                          access.acquireCalls == 1,
                      "the appended history kind does not change the existing vector channel or row lifetime");
                access.ledger.CloseIssuance();
                Refuses([&] { sdk.GetTargetHistoryPos(); }, "GetTargetHistoryPos", ClockResult::Stopped);
                Check(access.vectorCalls == calls + 5 && access.ledger.IsRetained() && !access.releaseCalls,
                      "Stop revokes history publication while the same actual row hold stays live");
            }
            Check(access.releaseCalls == 1 && access.releasedId == access.acquiredId && !access.ledger.IsRetained(),
                  "typed row destruction releases the same hold once after history access and Stop");
        }
        {
            RowAccess access;
            {
                CamRowObserver                        camera(1, access);
                const CCam&                           sdk = camera;
                const auto                            original = access.history;
                constexpr std::array<ClockResult, 14> refusals{ClockResult::InvalidArgument, ClockResult::NotInitialized,   ClockResult::WrongThread,
                                                               ClockResult::BindingChanged,  ClockResult::ReadFailure,      ClockResult::WriteFailure,
                                                               ClockResult::RestoreFailure,  ClockResult::PatchFailure,     ClockResult::Busy,
                                                               ClockResult::Expired,         ClockResult::UnsupportedBuild, ClockResult::OwnerConflict,
                                                               ClockResult::Stopped,         ClockResult::OriginalException};
                for (const auto status : refusals)
                {
                    access.vectorStatus = status;
                    const auto calls = access.vectorCalls;
                    CVector*   output = &access.vectors[1];
                    Refuses([&] { output = sdk.GetTargetHistoryPos(); }, "GetTargetHistoryPos", status);
                    Check(output == &access.vectors[1] && access.vectorCalls == calls + 1 && access.vectorKind == CameraRowVectorKind::TargetHistory &&
                              access.vectorId == access.acquiredId && access.vectorRow == 1 && access.acquireCalls == 1 && !access.releaseCalls &&
                              access.ledger.IsRetained() && std::memcmp(original.data(), access.history.data(), sizeof(original)) == 0,
                          "every history refusal preserves caller output, full history storage and the same row hold without retry or fallback");
                }
                access.vectorStatus = ClockResult::Ok;
                access.nullSuccess = true;
                const auto calls = access.vectorCalls;
                CVector*   output = &access.vectors[1];
                Refuses([&] { output = sdk.GetTargetHistoryPos(); }, "GetTargetHistoryPos", ClockResult::BindingChanged);
                Check(output == &access.vectors[1] && access.vectorCalls == calls + 1 && !access.releaseCalls && access.ledger.IsRetained(),
                      "an acquired row refuses null history success without manufacturing a buffer or dropping its hold");
            }
            Check(access.releaseCalls == 1 && access.releasedId == access.acquiredId && !access.ledger.IsRetained(),
                  "history refusal paths do not duplicate or bypass the same producer release");
        }
        if (checks - before != 73)
            throw std::runtime_error("history SDK case-count contract changed");
        std::puts("Actual Cam target history SDK fixture completed");
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
        for (unsigned count = 0; count != 3; ++count)
        {
            const auto jumpCuts = access.jumpCutCalls, restores = access.restoreCalls, active = access.activeCalls, fov = access.fovCalls;
            sdk.RestoreWithJumpCut();
            Check(access.jumpCutCalls == jumpCuts + 1 && access.restoreCalls == restores && access.activeCalls == active && access.fovCalls == fov,
                  "actual MTA JumpCut dispatches once to its selected owner, without ordinary Restore, SDK scalar writes or replay");
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
            const auto jumpCuts = access.jumpCutCalls;
            restores = access.restoreCalls;
            active = access.activeCalls;
            fov = access.fovCalls;
            Refuses([&] { sdk.RestoreWithJumpCut(); }, "RestoreWithJumpCut", status);
            Check(access.jumpCutCalls == jumpCuts + 1 && access.restoreCalls == restores && access.activeCalls == active && access.fovCalls == fov,
                  "failed JumpCut makes one selected call and cannot hide an accepted prefix through a second camera operation");
        }
        Missing              missing;
        CameraBasicsObserver oldCamera(missing);
        CamFovObserver       oldCam(0, missing);
        Refuses([&] { oldCamera.GetActiveCam(); }, "GetActiveCam", ClockResult::NotInitialized);
        Refuses([&] { oldCam.GetFOV(); }, "GetFOV", ClockResult::NotInitialized);
        Refuses([&] { oldCamera.Restore(); }, "Restore", ClockResult::NotInitialized);
        Refuses([&] { oldCamera.RestoreWithJumpCut(); }, "RestoreWithJumpCut", ClockResult::NotInitialized);
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
        FovWrites();
        Directions();
        RowBorrows();
        HistoryPointers();
        ClassChainIncludeBoundary();
        PooledInterfaces();
        TargetEntities();
        TargetSetters();
        CameraProducts();
        std::puts("Actual camera JumpCut SDK fixture completed");
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

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual camera Restore/selector and Cam finite FOV SDK policies
 ****************************************************************************/
#include "../../Client/game_de/CCameraSphereVisibleDE.h"
#include "../../Client/game_de/CCamFovDE.h"
#include "../../Client/game_de/CCamRowDE.h"
#include <array>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "CameraBasicsFixture.generated.h"
#include "CamFovFixture.generated.h"
#include "CamRowFixture.generated.h"
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

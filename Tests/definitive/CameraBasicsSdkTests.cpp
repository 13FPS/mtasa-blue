/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual camera Restore/selector and Cam finite FOV SDK policies
 ****************************************************************************/
#include "../../Client/game_de/CCameraSphereVisibleDE.h"
#include "../../Client/game_de/CCamFovDE.h"
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

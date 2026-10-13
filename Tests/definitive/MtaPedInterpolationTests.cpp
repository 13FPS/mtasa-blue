/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  PURPOSE:     Verify shared ped correction without loading either game backend
 *
 *****************************************************************************/

#include "../../Shared/sdk/CPedPositionInterpolation.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    int checks = 0;

    void Check(bool condition, const std::string& message)
    {
        ++checks;
        // Release builds must exercise these behavioral checks too, since the
        // same helper will correct positions inside optimized game backends.
        if (!condition)
            throw std::runtime_error(message);
    }

    void Near(float actual, float expected, const std::string& message)
    {
        Check(std::isfinite(actual) && std::abs(actual - expected) <= 0.00001f, message);
    }

    void Position(const CVector& actual, const CVector& expected, const std::string& message)
    {
        Near(actual.fX, expected.fX, message + " X");
        Near(actual.fY, expected.fY, message + " Y");
        Near(actual.fZ, expected.fZ, message + " Z");
    }

    void TestEngineMotionSurvivesCorrection()
    {
        // These positions describe an engine that keeps moving between pulses.
        // A lerp from the first pose to the target would erase that movement.
        MtaPedInterpolation::State state{CVector(8.0f, -4.0f, 2.0f), CVector(8.0f, -4.0f, 2.0f), 1000, 1400, 0.0f};
        auto                       first = MtaPedInterpolation::Update(CVector(), state, 1100, 100.0f);
        Position(first.position, CVector(2.0f, -1.0f, 0.5f), "first quarter corrects one quarter of the error");
        Near(first.state.lastAlpha, 0.25f, "first quarter records consumed alpha");
        Check(first.state.finishTime == 1400 && !first.warped, "first quarter remains active");

        auto second = MtaPedInterpolation::Update(CVector(3.0f, -0.75f, 0.375f), first.state, 1200, 100.0f);
        Position(second.position, CVector(5.0f, -1.75f, 0.875f), "second quarter preserves intervening engine movement");
        Near(second.state.lastAlpha, 0.5f, "second quarter records total consumed alpha");

        auto endpoint = MtaPedInterpolation::Update(CVector(6.0f, -1.5f, 0.75f), second.state, 1400, 100.0f);
        Position(endpoint.position, CVector(10.0f, -3.5f, 1.75f), "endpoint preserves all engine movement");
        Near(endpoint.state.lastAlpha, 1.0f, "endpoint consumes the remaining error once");
        Check(endpoint.state.finishTime == 0 && !endpoint.warped, "endpoint clears active interpolation");
        Check(endpoint.state.startTime == 1000, "correction retains the original start time");
        Position(endpoint.state.error, state.error, "correction retains the captured error");
        Position(endpoint.state.target, state.target, "correction retains the target");
    }

    void TestReplacementStartsFromCurrentEnginePosition()
    {
        MtaPedInterpolation::State initial{CVector(4.0f, 0.0f, 0.0f), CVector(4.0f, 0.0f, 0.0f), 100, 500, 0.0f};
        auto                       first = MtaPedInterpolation::Update(CVector(), initial, 200, 100.0f);
        Position(first.position, CVector(1.0f, 0.0f, 0.0f), "old target has applied one quarter");

        // SetTargetPosition captures a new error and resets lastAlpha after the
        // engine has advanced another half unit. Do not retain the old origin.
        MtaPedInterpolation::State replacement{CVector(-3.5f, 1.0f, 0.0f), CVector(-2.0f, 1.0f, 0.0f), 250, 650, 0.0f};
        auto                       halfway = MtaPedInterpolation::Update(CVector(1.5f, 0.0f, 0.0f), replacement, 450, 100.0f);
        Position(halfway.position, CVector(-0.25f, 0.5f, 0.0f), "replacement corrects from the current engine pose");
        Near(halfway.state.lastAlpha, 0.5f, "replacement has its own correction progress");

        auto endpoint = MtaPedInterpolation::Update(CVector(0.25f, 0.5f, 0.0f), halfway.state, 650, 100.0f);
        Position(endpoint.position, CVector(-1.5f, 1.0f, 0.0f), "replacement retains later engine movement");
        Check(endpoint.state.finishTime == 0, "replacement finishes independently of the old target");
    }

    void TestCadenceAndCompletion()
    {
        MtaPedInterpolation::State state{CVector(3.0f, 0.0f, 0.0f), CVector(3.0f, 0.0f, 0.0f), 1000, 1200, 0.0f};
        auto                       before = MtaPedInterpolation::Update(CVector(), state, 900, 100.0f);
        Position(before.position, CVector(), "before-start time applies no error");
        Near(before.state.lastAlpha, 0.0f, "before-start alpha is clamped");
        Check(before.state.finishTime == 1200, "before-start time does not finish");

        auto first = MtaPedInterpolation::Update(CVector(), state, 1050, 100.0f);
        auto repeat = MtaPedInterpolation::Update(CVector(1.25f, 0.0f, 0.0f), first.state, 1050, 100.0f);
        Position(repeat.position, CVector(1.25f, 0.0f, 0.0f), "same-time pulse does not reapply consumed error");
        Near(repeat.state.lastAlpha, 0.25f, "same-time pulse retains consumed alpha");

        auto afterGap = MtaPedInterpolation::Update(CVector(1.25f, 0.0f, 0.0f), repeat.state, 5000, 100.0f);
        Position(afterGap.position, CVector(3.5f, 0.0f, 0.0f), "long gap consumes only the remaining error");
        Near(afterGap.state.lastAlpha, 1.0f, "long gap cannot overcompensate");
        Check(afterGap.state.finishTime == 0, "long gap completes interpolation");

        auto inactive = MtaPedInterpolation::Update(CVector(4.0f, 1.0f, 0.0f), afterGap.state, 6000, 0.0f);
        Position(inactive.position, CVector(4.0f, 1.0f, 0.0f), "finished correction leaves subsequent engine motion alone");
        Check(inactive.state.finishTime == 0 && !inactive.warped, "inactive state skips correction and threshold policy");
        Near(inactive.state.lastAlpha, 1.0f, "inactive state retains its completion alpha");

        MtaPedInterpolation::State immediate{CVector(2.0f, 0.0f, 0.0f), CVector(2.0f, 0.0f, 0.0f), 100, 100, 0.0f};
        auto                       zeroDuration = MtaPedInterpolation::Update(CVector(), immediate, 50, 100.0f);
        Position(zeroDuration.position, CVector(2.0f, 0.0f, 0.0f), "zero-duration active correction completes immediately");
        Check(zeroDuration.state.finishTime == 0, "zero-duration correction clears active state");
    }

    void TestWarpThresholdAndInvalidCurrentPosition()
    {
        // The threshold is measured from the current engine pose, not from the
        // result after compensation. An exact boundary remains interpolated.
        MtaPedInterpolation::State state{CVector(30.0f, 0.0f, 0.0f), CVector(3.0f, 4.0f, 0.0f), 100, 200, 0.0f};
        auto                       boundary = MtaPedInterpolation::Update(CVector(), state, 150, 5.0f);
        Position(boundary.position, CVector(15.0f, 0.0f, 0.0f), "exact distance threshold uses compensation");
        Check(!boundary.warped && boundary.state.finishTime == 200, "equal threshold does not warp");

        auto beyond = MtaPedInterpolation::Update(CVector(), state, 150, std::nextafter(5.0f, 0.0f));
        Position(beyond.position, CVector(3.0f, 4.0f, 0.0f), "distance above threshold warps to target");
        Check(beyond.warped && beyond.state.finishTime == 0, "warp clears active interpolation");
        Near(beyond.state.lastAlpha, 0.5f, "warp retains the alpha consumed before threshold evaluation");

        const float invalid = std::numeric_limits<float>::quiet_NaN();
        auto        nanCurrent = MtaPedInterpolation::Update(CVector(invalid, 0.0f, 0.0f), state, 150, 100.0f);
        Position(nanCurrent.position, state.target, "NaN current position takes the existing target warp");
        Check(nanCurrent.warped && nanCurrent.state.finishTime == 0, "NaN current position finishes through warp");

        auto nanThreshold = MtaPedInterpolation::Update(CVector(), state, 150, invalid);
        Position(nanThreshold.position, state.target, "NaN threshold preserves negated-comparison warp policy");
        Check(nanThreshold.warped, "NaN threshold is not silently accepted");

        auto infiniteCurrent = MtaPedInterpolation::Update(CVector(0.0f, std::numeric_limits<float>::infinity(), 0.0f), state, 150, 100.0f);
        Position(infiniteCurrent.position, state.target, "infinite current position warps to a finite target");
        Check(infiniteCurrent.warped, "infinite current position does not continue interpolation");

        // Preserve the original boundary: it repairs invalid current-to-target
        // distance, but does not add validation of the compensation itself.
        state.error = CVector(invalid, 0.0f, 0.0f);
        auto nanError = MtaPedInterpolation::Update(CVector(), state, 150, 100.0f);
        Check(std::isnan(nanError.position.fX) && !nanError.warped, "NaN captured error retains classic propagation");
        Check(nanError.state.finishTime == 200, "NaN compensation does not add an interpolation reset");

        state.error = CVector(30.0f, 0.0f, 0.0f);
        state.lastAlpha = invalid;
        auto nanAlpha = MtaPedInterpolation::Update(CVector(), state, 150, 100.0f);
        Check(std::isnan(nanAlpha.position.fX) && !nanAlpha.warped, "NaN consumed alpha retains classic propagation");
        Near(nanAlpha.state.lastAlpha, 0.5f, "current finite alpha replaces invalid previous alpha");
    }

    void TestMovingContactOriginRemainsCallerPolicy()
    {
        // The caller subtracts the current contact origin and adds it back.
        // Keeping origin motion out of error prevents a second platform offset.
        MtaPedInterpolation::State state{CVector(4.0f, 0.0f, 0.0f), CVector(6.0f, 3.0f, 1.0f), 100, 300, 0.0f};
        CVector                    origin(100.0f, -50.0f, 10.0f);
        CVector                    enginePosition(102.0f, -47.0f, 11.0f);
        auto                       first = MtaPedInterpolation::Update(enginePosition - origin, state, 200, 5.0f);
        Position(first.position + origin, CVector(104.0f, -47.0f, 11.0f), "contact correction returns to world coordinates once");
        Check(!first.warped, "large world origin does not affect relative threshold");

        origin = CVector(105.0f, -52.0f, 11.0f);
        enginePosition = CVector(109.5f, -49.0f, 12.0f);
        auto endpoint = MtaPedInterpolation::Update(enginePosition - origin, first.state, 300, 5.0f);
        Position(endpoint.position + origin, CVector(111.5f, -49.0f, 12.0f), "moving contact and native movement both survive correction");
        Check(endpoint.state.finishTime == 0, "contact correction completes normally");
    }

    void TestClassicClockContract()
    {
        // This extraction does not reinterpret classic 32-bit timestamps as
        // wrapping elapsed durations. A reversed clock retains signed Unlerp.
        MtaPedInterpolation::State state{CVector(4.0f, 0.0f, 0.0f), CVector(4.0f, 0.0f, 0.0f), 100, 300, 0.5f};
        auto                       reversed = MtaPedInterpolation::Update(CVector(2.0f, 0.0f, 0.0f), state, 50, 100.0f);
        Position(reversed.position, CVector(), "clock reversal preserves classic alpha-delta behavior");
        Near(reversed.state.lastAlpha, 0.0f, "clock reversal clamps total alpha before subtracting consumed alpha");
        Check(reversed.state.finishTime == 300, "clock reversal does not introduce a reset policy");

        const auto                 maximum = std::numeric_limits<std::uint32_t>::max();
        MtaPedInterpolation::State highClock{CVector(4.0f, 0.0f, 0.0f), CVector(4.0f, 0.0f, 0.0f), maximum - 200, maximum, 0.0f};
        auto                       halfway = MtaPedInterpolation::Update(CVector(), highClock, maximum - 100, 100.0f);
        Position(halfway.position, CVector(2.0f, 0.0f, 0.0f), "high 32-bit timestamps retain sub-interval precision");
        Near(halfway.state.lastAlpha, 0.5f, "timestamp conversion occurs before subtraction");
    }

    void TestStagedNativeReadBoundary()
    {
        // The native velocity getter may restore physics. The original caller
        // publishes consumed alpha/completion before that boundary, then uses
        // the live target for warp afterward. A composed call cannot do that.
        MtaPedInterpolation::State state{CVector(8.0f, 0.0f, 0.0f), CVector(8.0f, 0.0f, 0.0f), 100, 200, 0.0f};
        const CVector              capturedPosition(2.0f, 0.0f, 0.0f);
        auto                       correction = MtaPedInterpolation::Advance(capturedPosition, state, 150);
        state.lastAlpha = correction.state.lastAlpha;
        state.finishTime = correction.state.finishTime;
        auto nativeRead = [&]()
        {
            Near(state.lastAlpha, 0.5f, "native-read boundary sees newly consumed alpha");
            Check(state.finishTime == 200, "native-read boundary sees the unfinished interval");
            Check(!correction.warped, "native-read boundary precedes threshold evaluation");
            Position(correction.position, CVector(6.0f, 0.0f, 0.0f), "advance is computed from captured engine position");
            state.target = CVector(20.0f, 0.0f, 0.0f);
            state.finishTime = 300;
            return 3.0f;
        };
        const float threshold = nativeRead();
        correction.state.target = state.target;
        MtaPedInterpolation::ApplyWarpThreshold(correction, capturedPosition, threshold);
        if (correction.warped)
            state.finishTime = 0;
        Position(correction.position, CVector(20.0f, 0.0f, 0.0f), "post-read warp uses the live replacement target");
        Check(correction.warped && state.finishTime == 0, "post-read warp clears the caller interval");

        state = MtaPedInterpolation::State{CVector(8.0f, 0.0f, 0.0f), CVector(8.0f, 0.0f, 0.0f), 100, 200, 0.0f};
        correction = MtaPedInterpolation::Advance(capturedPosition, state, 200);
        state.lastAlpha = correction.state.lastAlpha;
        state.finishTime = correction.state.finishTime;
        Near(state.lastAlpha, 1.0f, "completion is published before a native read");
        Check(state.finishTime == 0, "completed interval is cleared before a native read");
        MtaPedInterpolation::ApplyWarpThreshold(correction, capturedPosition, 5.0f);
        Position(correction.position, CVector(8.0f, 0.0f, 0.0f), "completed advance still receives the original warp check");
        Check(correction.warped, "completion does not bypass threshold evaluation");

        state = MtaPedInterpolation::State{CVector(8.0f, 0.0f, 0.0f), CVector(8.0f, 0.0f, 0.0f), 100, 200, 0.0f};
        correction = MtaPedInterpolation::Advance(capturedPosition, state, 150);
        state.lastAlpha = correction.state.lastAlpha;
        state.finishTime = correction.state.finishTime;
        state.target = CVector(3.0f, 0.0f, 0.0f);
        state.finishTime = 500;
        correction.state.target = state.target;
        MtaPedInterpolation::ApplyWarpThreshold(correction, capturedPosition, 3.0f);
        if (correction.warped)
            state.finishTime = 0;
        Check(!correction.warped && state.finishTime == 500, "non-warp path preserves a native-boundary state change");
        Position(correction.position, CVector(6.0f, 0.0f, 0.0f), "non-warp path retains the already computed compensation");
    }
}

int main()
{
    try
    {
        TestEngineMotionSurvivesCorrection();
        TestReplacementStartsFromCurrentEnginePosition();
        TestCadenceAndCompletion();
        TestWarpThresholdAndInvalidCurrentPosition();
        TestMovingContactOriginRemainsCallerPolicy();
        TestClassicClockContract();
        TestStagedNativeReadBoundary();
        std::cout << "Passed " << checks << " reused MTA ped interpolation checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MTA ped interpolation failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}

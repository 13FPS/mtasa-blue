/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit retained-Core routing for one physical movement trial
 *****************************************************************************/
#pragma once

// The existing game SDK transitively declares Windows DWORD/BYTE interfaces.
// This optional Core header must also work in a no-PCH fixture translation unit.
#include <windows.h>
#include <game/CGameMoveAnimWorkload.h>

namespace DefinitiveMoveAnimWorkload
{
    inline constexpr std::uint32_t ApiVersion = 1;
    enum class Phase : std::uint32_t
    {
        Idle,
        Queued,
        Running,
        Retiring,
        Finished,
        Unavailable
    };
    struct ReportV1
    {
        std::uint32_t                         size = sizeof(ReportV1), version = ApiVersion;
        Phase                                 phase = Phase::Idle;
        std::uint32_t                         available = 0, queued = 0, beginAttempted = 0, attemptPending = 0;
        std::uint32_t                         stopRequested = 0, cancelRequested = 0, exceptionObserved = 0;
        mta::game_de::Status                  beginStatus = mta::game_de::Status::NotInitialized;
        mta::game_de::Status                  lastStatus = mta::game_de::Status::NotInitialized;
        mta::game_de::MoveAnimWorkloadStateV1 native;
        std::uint32_t                         reserved[4]{};
    };
    static_assert(std::is_standard_layout_v<ReportV1> && std::is_trivially_copyable_v<ReportV1>);

    // The route contains only CPU state. Its owner supplies the actual adapter
    // export calls; neither console execution nor a report read enters native
    // code. A retained attempt may disappear only after authoritative End.
    class Route final
    {
    public:
        bool Queue() noexcept
        {
            if (BlocksAdmission() || report.stopRequested)
                return false;
            report = {};
            report.queued = 1;
            report.phase = Phase::Queued;
            ending = false;
            return true;
        }
        void Cancel() noexcept
        {
            report.queued = 0;
            report.cancelRequested = 1;
            ending = report.attemptPending != 0;
            if (!report.attemptPending)
                report.phase = Phase::Finished;
        }
        bool     BlocksAdmission() const noexcept { return report.queued || report.attemptPending; }
        ReportV1 Read() const noexcept { return report; }

        template <class Owner>
        void Pulse(Owner& owner, bool hidden, bool inputRetired) noexcept
        {
            try
            {
                report.available = owner.HasMoveAnimWorkloadService() ? 1u : 0u;
                if (report.stopRequested)
                {
                    Stop(owner);
                    return;
                }
                if (!BlocksAdmission())
                    return;
                if (report.queued)
                {
                    if (!hidden || !inputRetired)
                        return;
                    report.queued = 0;
                    if (!report.available)
                    {
                        report.phase = Phase::Unavailable;
                        report.lastStatus = mta::game_de::Status::Unsupported;
                        return;
                    }
                    mta::game_de::MoveAnimWorkloadRequestV1 request;
                    auto                                    next = report.native;
                    report.beginAttempted = 1;
                    report.beginStatus = report.lastStatus = owner.BeginMoveAnimWorkload(request, next);
                    report.native = next;
                    report.attemptPending = owner.MoveAnimWorkloadPending() ? 1u : 0u;
                    if (!report.attemptPending)
                    {
                        report.phase = Phase::Finished;
                        return;
                    }
                    ending = report.beginStatus != mta::game_de::Status::Ok;
                    report.phase = ending ? Phase::Retiring : Phase::Running;
                }
                auto       latest = report.native;
                const auto read = owner.ReadMoveAnimWorkload(latest);
                report.lastStatus = read;
                if (read == mta::game_de::Status::Ok && (!report.native.owner.generation || latest.owner.generation == report.native.owner.generation))
                {
                    report.native = latest;
                    ending = ending || Terminal(latest);
                }
                else
                    ending = true;
                if (!hidden || !inputRetired || report.cancelRequested)
                    ending = true;
                if (!ending)
                {
                    report.lastStatus = owner.RenewMoveAnimWorkload(report.native.owner.generation);
                    ending = report.lastStatus != mta::game_de::Status::Ok;
                }
                if (ending)
                    Drain(owner);
            }
            catch (...)
            {
                // An exception after Begin cannot erase its provider's pending
                // bit, even if no token was returned to this CPU route.
                report.exceptionObserved = 1;
                report.attemptPending = report.attemptPending || owner.MoveAnimWorkloadPending();
                report.queued = 0;
                ending = report.attemptPending != 0;
                report.phase = ending ? Phase::Retiring : Phase::Finished;
                report.lastStatus = mta::game_de::Status::Unsupported;
            }
        }

        template <class Owner>
        bool Stop(Owner& owner) noexcept
        {
            report.stopRequested = 1;
            report.queued = 0;
            if (!report.attemptPending && !owner.MoveAnimWorkloadPending())
                return true;
            report.attemptPending = 1;
            ending = true;
            try
            {
                return Drain(owner);
            }
            catch (...)
            {
                report.exceptionObserved = 1;
                report.phase = Phase::Retiring;
                return false;
            }
        }

    private:
        static bool Terminal(const mta::game_de::MoveAnimWorkloadStateV1& state) noexcept
        {
            using P = mta::game_de::MoveAnimWorkloadPhaseV1;
            return state.phase == P::Completed || state.phase == P::Refused || state.phase == P::Unknown || state.phase == P::Retired || state.owner.stopping ||
                   state.owner.failure != mta::game_de::ClockResult::Ok;
        }
        template <class Owner>
        bool Drain(Owner& owner)
        {
            report.phase = Phase::Retiring;
            // A valid read can recover a token omitted by an earlier refused
            // Begin. Refusal preserves the previous sample and its same debt.
            auto latest = report.native;
            if (owner.ReadMoveAnimWorkload(latest) == mta::game_de::Status::Ok &&
                (!report.native.owner.generation || latest.owner.generation == report.native.owner.generation))
                report.native = latest;
            auto       ended = report.native;
            const auto generation = report.native.owner.generation;
            report.lastStatus = owner.EndMoveAnimWorkload(generation, ended);
            // A refused End can leave its output untouched or unspecified. Only
            // same-attempt progress may replace the cached token; generation0
            // needs this attempt's explicit no-entry proof, never a wildcard.
            const bool sameAttempt = generation ? ended.owner.generation == generation && !ended.notEntered : !ended.owner.generation && ended.notEntered;
            if (sameAttempt)
                report.native = ended;
            if (report.lastStatus != mta::game_de::Status::Ok || owner.MoveAnimWorkloadPending() || !sameAttempt)
                return false;
            // Generation0 is only this attempt's explicitly proven no-entry
            // retirement, not a wildcard End or a fabricated native receipt.
            // The module's process-lifetime GetModuleHandleEx(PIN) is not
            // retained native effect debt. Exact End/token/resource ACKs
            // below remain authoritative even while that permanent pin lives.
            const bool acknowledged =
                !ended.cleanupPending && !ended.owner.active && !ended.owner.hooked &&
                ((generation && ended.endAcknowledged && !ended.notEntered) || (!generation && ended.notEntered && !ended.endAcknowledged));
            if (!acknowledged)
                return false;
            report.attemptPending = 0;
            report.phase = Phase::Finished;
            return true;
        }
        ReportV1 report;
        bool     ending = false;
    };
}

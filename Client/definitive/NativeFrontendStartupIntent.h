/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Queue one loaded-world first menu without reopening after Quit
 ****************************************************************************/
#pragma once
#include <windows.h>
#include "FrontendAdmissionSample.h"
#include "FrontendRuntimeReport.h"
#include "../core/DefinitiveFrontend.h"
#include "../sdk/game/CGameFrontendAdmission.h"
namespace deonline
{
    class NativeFrontendStartupIntent final
    {
    public:
        NativeFrontendStartupIntent(bool enabled, std::uint32_t generation) noexcept : pending(enabled && generation), generation(generation) {}
        void Stop() noexcept { pending = false; }
        bool Pending() const noexcept { return pending; }
        void Observe(const FrontendRuntimeReport::Snapshot& runtime) noexcept
        {
            namespace R = FrontendRuntimeReport;
            if (Current(runtime) && (runtime[R::ShowAcknowledgements] || runtime[R::WorkerPhase] == static_cast<std::uint32_t>(R::Phase::Visible)))
                pending = false;
        }
        bool ShouldShow(const FrontendRuntimeReport::Snapshot& runtime, const FrontendAdmissionSample::Snapshot& sample, bool focused, bool manualPending,
                        std::uint64_t now) const noexcept
        {
            namespace R = FrontendRuntimeReport;
            namespace A = FrontendAdmissionSample;
            if (!pending || !focused || manualPending || !Current(runtime) || runtime[R::WorkerPhase] != static_cast<std::uint32_t>(R::Phase::Hidden) ||
                runtime[R::OutstandingSequence] || !runtime[R::HostLeaseFresh] || !runtime[R::WorkerThread] || sample[A::Version] != A::ApiVersion ||
                sample[A::RuntimeGeneration] != generation || sample[A::WorkerThread] != runtime[R::WorkerThread] ||
                sample[A::Status] != static_cast<std::uint32_t>(DefinitiveFrontend::Status::Ok) ||
                sample[A::Admission] != static_cast<std::uint32_t>(mta::game_de::FrontendAdmission::EligibleLocal) || sample[A::Reserved9] ||
                sample[A::Reserved10] || sample[A::Reserved11])
                return false;
            const auto serial = A::Pair(sample, A::SerialLow), tick = A::Pair(sample, A::TickLow);
            return serial > attemptedSerial && tick <= now && now - tick <= A::FreshnessMs;
        }
        void Submitted(const FrontendAdmissionSample::Snapshot& sample) noexcept
        {
            // A queued Show can still defer. Only real visibility/ACK consumes
            // intent; another try requires a newly observed sample serial.
            attemptedSerial = FrontendAdmissionSample::Pair(sample, FrontendAdmissionSample::SerialLow);
        }

    private:
        bool Current(const FrontendRuntimeReport::Snapshot& runtime) const noexcept
        {
            namespace R = FrontendRuntimeReport;
            return generation && runtime[R::Version] == R::ApiVersion && runtime[R::SessionGenerationLow] == generation && !runtime[R::SessionGenerationHigh];
        }
        bool                pending;
        const std::uint32_t generation;
        std::uint64_t       attemptedSerial = 0;
    };
}

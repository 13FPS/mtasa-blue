/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Defer host borrowers while the actual Core owns a movement attempt
 ****************************************************************************/
#pragma once
#include "../core/DefinitiveMoveAnimWorkload.h"
#include <algorithm>

namespace deonline
{
    class FrontendMoveAnimWorkloadGate final
    {
    public:
        using Report = DefinitiveMoveAnimWorkload::ReportV1;
        explicit FrontendMoveAnimWorkloadGate(bool optionalContract) noexcept : present(optionalContract) {}
        bool Observe(const Report& value) noexcept
        {
            if (!present || !Valid(value))
                return false;
            report = value;
            sampled = true;
            return true;
        }
        bool        Blocks() const noexcept { return present && (!sampled || Blocks(report)); }
        bool        CoreOwnsShutdown() const noexcept { return present; }
        static bool Blocks(const Report& value) noexcept
        {
            const auto& owner = value.native.owner;
            // Process-lifetime modulePinned is deliberately absent. Only the
            // actual attempt/native obligations defer another input or view.
            return value.queued || value.attemptPending || value.native.cleanupPending || value.native.tokenRegistered || value.native.overridePublished ||
                   owner.active || owner.hooked || owner.inFlight || owner.patchInstalled || owner.retainedPeerThreads || owner.patchRecoveryPending ||
                   owner.dataRecoveryPending || owner.protectionRestorePending;
        }
        static bool Valid(const Report& value) noexcept
        {
            namespace W = mta::game_de;
            const auto& native = value.native;
            if (value.size != sizeof(value) || value.version != DefinitiveMoveAnimWorkload::ApiVersion ||
                static_cast<std::uint32_t>(value.phase) > static_cast<std::uint32_t>(DefinitiveMoveAnimWorkload::Phase::Unavailable) ||
                static_cast<std::uint32_t>(value.beginStatus) > static_cast<std::uint32_t>(W::Status::Unsupported) ||
                static_cast<std::uint32_t>(value.lastStatus) > static_cast<std::uint32_t>(W::Status::Unsupported) || native.size != sizeof(native) ||
                native.version != W::MoveAnimWorkloadVersionV1 ||
                static_cast<std::uint32_t>(native.phase) > static_cast<std::uint32_t>(W::MoveAnimWorkloadPhaseV1::Retired) ||
                static_cast<std::uint32_t>(native.result) > static_cast<std::uint32_t>(W::ClockResult::OriginalException) ||
                static_cast<std::uint32_t>(native.owner.failure) > static_cast<std::uint32_t>(W::ClockResult::OriginalException) ||
                static_cast<std::uint32_t>(native.owner.lastCommandResult) > static_cast<std::uint32_t>(W::ClockResult::OriginalException) ||
                native.owner.size != sizeof(native.owner) || native.owner.version != W::ClockServiceVersionV1 || native.physicalConfirmationRequired != 1 ||
                native.fullMtaReady)
                return false;
            for (const auto flag : {value.available,           value.queued,
                                    value.beginAttempted,      value.attemptPending,
                                    value.stopRequested,       value.cancelRequested,
                                    value.exceptionObserved,   native.tokenRegistered,
                                    native.hooksEnabled,       native.overridePublished,
                                    native.writeAcknowledged,  native.reapplyAcknowledged,
                                    native.withdrawAttempted,  native.withdrawAcknowledged,
                                    native.releaseAttempted,   native.releaseAcknowledged,
                                    native.endAcknowledged,    native.cleanupPending,
                                    native.cleanupViaOwnerEnd, native.notEntered,
                                    native.owner.bound,        native.owner.hooked,
                                    native.owner.active,       native.owner.ready,
                                    native.owner.stopping,     native.owner.restored,
                                    native.owner.frozen,       native.owner.modulePinned})
                if (flag > 1)
                    return false;
            return std::all_of(std::begin(value.reserved), std::end(value.reserved), [](auto word) { return word == 0; }) &&
                   std::all_of(std::begin(native.reserved), std::end(native.reserved), [](auto word) { return word == 0; }) &&
                   std::all_of(std::begin(native.owner.reserved), std::end(native.owner.reserved), [](auto word) { return word == 0; });
        }

    private:
        const bool present;
        bool       sampled = false;
        Report     report;
    };
}

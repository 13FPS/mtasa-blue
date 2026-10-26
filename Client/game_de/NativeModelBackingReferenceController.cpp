/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain causal native acquisition receipts beyond SDK counter failures
 *****************************************************************************/
#include "NativeModelBackingReferenceController.h"
#include "NativeModelBankProfile.h"
#include <limits>

namespace mta::game_de
{
    namespace
    {
        class Operation
        {
        public:
            explicit Operation(std::atomic<bool>& value) noexcept : flag(value)
            {
                bool expected = false;
                acquired = flag.compare_exchange_strong(expected, true);
            }
            ~Operation()
            {
                if (acquired)
                    flag.store(false);
            }
            std::atomic<bool>& flag;
            bool               acquired = false;
        };
        bool Effect(const NativeModelReferenceTransactions::Receipt& receipt) noexcept
        {
            return receipt.entered || receipt.reserved || receipt.published || receipt.accepted;
        }
    }
    NativeModelBackingReferenceController::State* NativeModelBackingReferenceController::Find(std::uint64_t generation, const Token& token) noexcept
    {
        if (!generation || token.ownerGeneration != generation || !token.serial)
            return nullptr;
        for (auto& record : records)
            if (record.reference.ownerGeneration == generation && record.reference.serial == token.serial && record.reference.modelIndex == token.modelIndex)
                return &record;
        return nullptr;
    }
    void NativeModelBackingReferenceController::Publish() noexcept
    {
        bool debt = false, fault = false;
        for (const auto& record : records)
        {
            debt = debt ||
                   (record.phase != Phase::Released && (record.phase == Phase::Acquiring || record.phase == Phase::Releasing || Effect(record.acquisition)));
            fault = fault || record.phase == Phase::FaultDebt;
        }
        retained.store(debt);
        recovery.store(fault);
        pending.store(!records.empty());
    }
    ClockResult NativeModelBackingReferenceController::Reserve(std::uint64_t generation, std::uint32_t model, CheckOwner owner, Token& output) noexcept
    {
        const auto currentGeneration = generation;
        const auto modelIndex = model;
        Operation  operation(busy);
        if (!operation.acquired)
            return ClockResult::Busy;
        if (!currentGeneration || modelIndex >= NativeModelBankProfile::ModelCount)
            return ClockResult::InvalidArgument;
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (serial == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::UnsupportedBuild;
        try
        {
            // Allocate the permanent CPU record before the issued token can
            // reach a partially constructed facade or enter native acquisition.
            State prepared;
            prepared.reference = {currentGeneration, ++serial, modelIndex};
            records.push_back(prepared);
            auto& record = records.back();
            result = owner();
            if (result != ClockResult::Ok)
            {
                records.pop_back();
                Publish();
                return result;
            }
            output = record.reference;
            Publish();
            return ClockResult::Ok;
        }
        catch (...)
        {
            Publish();
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeModelBackingReferenceController::Bind(std::uint64_t generation, const Token& input, CheckOwner owner) noexcept
    {
        const auto token = input;
        Operation  operation(busy);
        if (!operation.acquired)
            return ClockResult::Busy;
        auto* record = Find(generation, token);
        if (!record)
            return ClockResult::BindingChanged;
        if (record->phase != Phase::Prepared)
            return ClockResult::OwnerConflict;
        if (!owner)
            return ClockResult::NotInitialized;
        const auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        record->phase = Phase::Bound;
        record->lastResult = ClockResult::Ok;
        // A constructor failure after this CPU claim cannot replay Bind. End
        // may still revoke it because no native acquisition has entered.
        Publish();
        return owner();
    }
    ClockResult NativeModelBackingReferenceController::Acquire(std::uintptr_t image, std::uint64_t generation, const Token& input, CheckOwner owner,
                                                               const NativeModelReferenceTransactions::Calls* calls) noexcept
    {
        const auto token = input;
        Operation  operation(busy);
        if (!operation.acquired)
            return ClockResult::Busy;
        auto* record = Find(generation, token);
        if (!record)
            return ClockResult::BindingChanged;
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (record->phase == Phase::Held)
            return owner();  // One capability already owns this backing ref; never increment it again.
        if (record->phase != Phase::Bound)
            return ClockResult::OwnerConflict;
        record->phase = Phase::Acquiring;
        Publish();
        NativeModelReferenceTransactions::Receipt acquisition;
        result = NativeModelReferenceTransactions::Acquire(image, token.modelIndex, owner, &acquisition, calls, true);
        // This assignment is nonthrowing and occurs before another owner
        // callback. Accepted count/publication effects must survive lost ACKs,
        // facade exceptions/drops and every later generation/reset attempt.
        record->acquisition = acquisition;
        record->lastResult = result;
        if (result == ClockResult::Ok)
        {
            record->phase = Phase::Held;
            result = owner();
            record->lastResult = result;
            if (result != ClockResult::Ok)
                record->phase = Phase::FaultDebt;
        }
        else
        {
            record->phase = Effect(acquisition) ? Phase::FaultDebt : Phase::Bound;
        }
        Publish();
        return result;
    }
    ClockResult NativeModelBackingReferenceController::Release(std::uintptr_t image, std::uint64_t generation, const Token& input,
                                                               NativeModelReferenceRelease::ZeroPolicy inputPolicy, CheckOwner owner,
                                                               NativeModelReferenceRelease::Receipt* output, NativeModelReferenceMemory::Exchange exchange,
                                                               NativeStreamingRemoveCalls::Invoke invoke) noexcept
    {
        const auto                           token = input;
        const auto                           policy = inputPolicy;
        NativeModelReferenceRelease::Receipt receipt;
        const auto                           finish = [&](ClockResult result) noexcept
        {
            if (output)
                *output = receipt;
            return result;
        };
        Operation operation(busy);
        if (!operation.acquired)
            return finish(ClockResult::Busy);
        auto* record = Find(generation, token);
        if (!record)
            return finish(ClockResult::BindingChanged);
        if (record->phase != Phase::Held)
            return finish(ClockResult::OwnerConflict);
        if (policy != NativeModelReferenceRelease::ZeroPolicy::KeepResident && policy != NativeModelReferenceRelease::ZeroPolicy::RemoveAtZero)
            return finish(ClockResult::InvalidArgument);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        auto result = owner();
        if (result != ClockResult::Ok)
            return finish(result);
        const NativeModelReferenceMemory::RequiredIdentity identity = record->acquisition.origin;
        std::uint32_t                                      count = 0;
        result = NativeModelReferenceMemory::ReadCount(image, token.modelIndex, owner, count, &identity);
        record->lastResult = result;
        if (result != ClockResult::Ok)
        {
            if (result == ClockResult::BindingChanged)
            {
                // A changed origin/binding cannot authorize this capability
                // against a later replacement, even without a native entry.
                record->phase = Phase::FaultDebt;
                Publish();
            }
            return finish(result);
        }
        if (!count)
        {
            // An externally lost/zero count is not our release ACK. Keep the
            // Held capability rather than synthesizing cleanup success.
            record->lastResult = ClockResult::BindingChanged;
            record->phase = Phase::FaultDebt;
            Publish();
            return finish(ClockResult::BindingChanged);
        }
        record->releaseSelectedCount = count;
        record->releasePolicy = policy;
        record->releaseFinal = count == 1;
        record->phase = Phase::Releasing;
        Publish();
        if (record->releaseFinal)
            result = NativeModelReferenceRelease::ReleaseFinal(image, token.modelIndex, policy, owner, &receipt, exchange, invoke, &identity);
        else
            result = NativeModelReferenceMemory::Adjust(image, token.modelIndex, -1, owner, &receipt.reference, exchange, &identity);
        // No allocation or owner callback precedes permanent receipt capture.
        // Never select another primitive after refusal/entered uncertainty.
        record->release = receipt;
        record->lastResult = result;
        const bool entered = receipt.reference.entered || receipt.reference.accepted || receipt.removal.entered;
        if (result == ClockResult::Ok)
        {
            record->phase = Phase::Released;
            // The primitive already acknowledged the complete release. A
            // later owner loss still stops Runtime, but cannot resurrect the
            // backing reference or authorize another acquire with this token.
            result = owner();
            record->lastResult = result;
        }
        else
            record->phase = (entered || result == ClockResult::BindingChanged) ? Phase::FaultDebt : Phase::Held;
        Publish();
        return finish(result);
    }

    ClockResult NativeModelBackingReferenceController::Read(std::uint64_t generation, const Token& input, CheckOwner owner, State& output) noexcept
    {
        const auto token = input;
        Operation  operation(busy);
        if (!operation.acquired)
            return ClockResult::Busy;
        auto* record = Find(generation, token);
        if (!record)
            return ClockResult::BindingChanged;
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        const auto snapshot = *record;
        result = owner();
        if (result == ClockResult::Ok)
            output = snapshot;
        return result;
    }
    ClockResult NativeModelBackingReferenceController::RevokeUnacquired(std::uint64_t generation) noexcept
    {
        Operation operation(busy);
        if (!operation.acquired)
            return ClockResult::Busy;
        if (!generation)
            return ClockResult::InvalidArgument;
        for (auto it = records.begin(); it != records.end();)
        {
            if (it->reference.ownerGeneration == generation &&
                (it->phase == Phase::Released || (!Effect(it->acquisition) && it->phase != Phase::Acquiring && it->phase != Phase::Releasing)))
                it = records.erase(it);
            else
                ++it;
        }
        Publish();
        return ClockResult::Ok;
    }
}

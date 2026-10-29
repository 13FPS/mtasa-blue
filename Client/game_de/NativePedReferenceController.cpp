/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native-visible cell storage until verified retirement
 ****************************************************************************/
#include "NativePedReferenceController.h"

namespace mta::game_de
{
    namespace PedReferenceControllerDetail
    {
        struct Lock
        {
            explicit Lock(std::atomic_flag& flag) noexcept : flag(flag), held(!flag.test_and_set()) {}
            ~Lock()
            {
                if (held)
                    flag.clear();
            }
            std::atomic_flag& flag;
            bool              held;
        };
        void ResetReceipt(NativePedReferenceMemory::Receipt* receipt, NativePedReferenceMemory::Operation operation, std::uint64_t generation,
                          std::uint32_t handle = 0) noexcept
        {
            if (receipt)
            {
                *receipt = {};
                receipt->operation = operation;
                receipt->generation = generation;
                receipt->handle = handle;
            }
        }
    }

    std::uint32_t NativePedReferenceController::Publish() noexcept
    {
        std::uint32_t count = 0;
        bool          pending = false;
        for (const auto& slot : slots)
        {
            if (!slot.cell.IsRetained())
                continue;
            ++count;
            pending = pending || stopping || slot.recoveryNeeded;
        }
        // These are conservative producer-visible summaries, never permission
        // to read the non-atomic slot metadata outside the actual callback.
        recovery.store(pending);
        retained.store(count != 0);
        return count;
    }

    ClockResult NativePedReferenceController::Validate(std::uint64_t generation, const Token& token, Slot*& output, bool releasing) noexcept
    {
        if (!generation || !token.version || !token.ownerGeneration ||
            !MtaGame::CPoolSlotPolicy::ContainsIndex(static_cast<std::int32_t>(Capacity), token.slot))
            return ClockResult::InvalidArgument;
        if (generation != ownerGeneration || token.ownerGeneration != generation)
            return ClockResult::OwnerConflict;
        auto& slot = slots[token.slot];
        if (slot.version != token.version || slot.generation != generation)
            return ClockResult::OwnerConflict;
        if (stopping)
            return ClockResult::Stopped;
        if (!slot.issued || slot.releaseRequested || !slot.cell.IsRetained() || (!releasing && !slot.published))
            return ClockResult::NotInitialized;
        output = &slot;
        return ClockResult::Ok;
    }

    ClockResult NativePedReferenceController::Acquire(const Memory& memory, std::uint64_t generation, std::uint32_t handle, Token& output, Receipt* receipt,
                                                      Memory::InvokeRegister invoke) noexcept
    {
        PedReferenceControllerDetail::ResetReceipt(receipt, Memory::Operation::Register, generation, handle);
        PedReferenceControllerDetail::Lock lock(operation);
        if (!lock.held)
            return ClockResult::Busy;
        if (!generation)
            return ClockResult::InvalidArgument;
        if (ownerGeneration != generation)
        {
            if (retained.load())
                return ClockResult::OwnerConflict;
            ownerGeneration = generation;
            stopping = false;
            retireCursor = 0;
        }
        if (stopping)
            return ClockResult::Stopped;

        const auto index =
            MtaGame::CPoolSlotPolicy::FindFreeAfter(static_cast<std::int32_t>(Capacity), cursor,
                                                    [&](std::int32_t candidate)
                                                    {
                                                        const auto& slot = slots[static_cast<std::size_t>(candidate)];
                                                        return !slot.published && !slot.cell.IsRetained() && Token::NextVersion(slot.version) != 0;
                                                    });
        if (index == MtaGame::CPoolSlotPolicy::InvalidSlot)
            return ClockResult::Busy;

        auto& slot = slots[index];
        slot.version = Token::NextVersion(slot.version);
        slot.generation = generation;
        slot.issued = slot.published = slot.releaseRequested = slot.recoveryNeeded = false;
        cursor = static_cast<std::int32_t>(index);
        // Register may allocate before it can enter native code. Publish the
        // possible debt first so producer-side End cannot observe a false
        // clear summary while an unpublished acquisition is in flight.
        retained.store(true);
        recovery.store(true);
        auto result = memory.Register(handle, slot.cell, receipt, invoke);
        if (result == ClockResult::Ok)
        {
            Value                         value;
            NativePedReferenceCell::State state;
            result = memory.Read(slot.cell, value);
            if (result == ClockResult::Ok)
            {
                if (!slot.cell.ReadState(state) || !slot.cell.IsRetained() || !state.registrationVerified || state.uncertain || state.cleanupEntered ||
                    state.generation != generation || state.handle != handle || value.handle != handle || value.nativePed != state.nativePed || !state.node ||
                    !state.cellAddress)
                    result = ClockResult::BindingChanged;
                else
                {
                    slot.issued = slot.published = true;
                    Token token;
                    token.slot = index;
                    token.version = slot.version;
                    token.ownerGeneration = generation;
                    output = token;
                }
            }
        }
        // A failed operation never publishes a token. Its stable native cell
        // remains in this slot until the primitive acknowledges real release.
        slot.recoveryNeeded = result != ClockResult::Ok && slot.cell.IsRetained();
        Publish();
        return result;
    }

    ClockResult NativePedReferenceController::FindPublished(const Memory& memory, std::uint64_t generation, MatchPublished match, void* context,
                                                            Token& tokenOutput, Value& valueOutput) noexcept
    {
        PedReferenceControllerDetail::Lock lock(operation);
        if (!lock.held)
            return ClockResult::Busy;
        if (!generation || !match)
            return ClockResult::InvalidArgument;
        if (generation != ownerGeneration)
            return ClockResult::OwnerConflict;
        if (stopping)
            return ClockResult::Stopped;
        for (std::size_t index = 0; index != slots.size(); ++index)
        {
            auto& slot = slots[index];
            if (!slot.issued || !slot.published || slot.releaseRequested || slot.generation != generation || !slot.cell.IsRetained())
                continue;
            Token token;
            token.slot = static_cast<std::uint32_t>(index);
            token.version = slot.version;
            token.ownerGeneration = generation;
            Value before, after;
            auto  result = memory.Read(slot.cell, before);
            if (result != ClockResult::Ok)
            {
                slot.published = false;
                Publish();
                // Ordinary native destruction invalidates this cell. Other
                // registered peds remain eligible; global owner failures do not.
                if (result == ClockResult::NotInitialized)
                    continue;
                return result;
            }
            bool matches = false;
            if ((result = match(token, before, matches, context)) != ClockResult::Ok)
                return result;
            if (!matches)
                continue;
            if ((result = memory.Read(slot.cell, after)) != ClockResult::Ok)
                return result;
            if (before.handle != after.handle || before.slot != after.slot || before.pool != after.pool || before.objects != after.objects ||
                before.flags != after.flags || before.nativePed != after.nativePed || before.vtable != after.vtable ||
                before.firstMethod != after.firstMethod || before.capacity != after.capacity || before.cursor != after.cursor ||
                before.generation != after.generation || before.ownsAllocations != after.ownsAllocations)
                return ClockResult::BindingChanged;
            tokenOutput = token;
            valueOutput = after;
            return ClockResult::Ok;
        }
        return ClockResult::NotInitialized;
    }
    ClockResult NativePedReferenceController::Resolve(const Memory& memory, std::uint64_t generation, const Token& token, Value& output) noexcept
    {
        PedReferenceControllerDetail::Lock lock(operation);
        if (!lock.held)
            return ClockResult::Busy;
        Slot* slot = nullptr;
        auto  result = Validate(generation, token, slot);
        if (result != ClockResult::Ok)
            return result;
        Value value;
        result = memory.Read(slot->cell, value);
        if (result == ClockResult::Ok)
            output = value;
        else
        {
            // Even transient loss of a fresh native relation stops publication.
            // Keep the issued identity for ordinary Release after native death;
            // an invalidated borrow alone need not stop the whole MTA owner.
            slot->published = false;
            Publish();
        }
        return result;
    }

    ClockResult NativePedReferenceController::Release(const Memory& memory, std::uint64_t generation, const Token& token, Receipt* receipt,
                                                      Memory::InvokeUnregister invoke) noexcept
    {
        PedReferenceControllerDetail::ResetReceipt(receipt, Memory::Operation::Release, generation);
        PedReferenceControllerDetail::Lock lock(operation);
        if (!lock.held)
            return ClockResult::Busy;
        Slot*      slot = nullptr;
        const auto checked = Validate(generation, token, slot, true);
        if (checked != ClockResult::Ok)
            return checked;
        // Revoke before entering cleanup. Failed or partially applied native
        // cleanup cannot leave a caller's old CPU token usable for another read.
        slot->published = false;
        slot->releaseRequested = slot->recoveryNeeded = true;
        recovery.store(true);
        const auto result = memory.Release(slot->cell, receipt, invoke);
        Publish();
        return result;
    }

    ClockResult NativePedReferenceController::Retire(const Memory& memory, std::uint64_t generation, std::uint32_t budget, RetireProgress& output,
                                                     Memory::InvokeUnregister invoke) noexcept
    {
        PedReferenceControllerDetail::Lock lock(operation);
        if (!lock.held)
            return ClockResult::Busy;
        if (!generation || !budget || budget > Capacity)
            return ClockResult::InvalidArgument;
        if (ownerGeneration != generation)
        {
            if (retained.load())
                return ClockResult::OwnerConflict;
            ownerGeneration = generation;
        }
        stopping = true;
        recovery.store(retained.load());
        for (auto& slot : slots)
            slot.published = false;

        RetireProgress progress;
        auto           result = ClockResult::Ok;
        const auto     start = retireCursor;
        // Work is bounded by one fixed slot scan and at most budget genuine
        // primitive release attempts. Rotate the cursor so persistent debt at
        // one slot cannot starve other cells during later recovery callbacks.
        while (progress.scanned < Capacity && progress.attempted < budget)
        {
            const auto index = (start + progress.scanned) % static_cast<std::uint32_t>(Capacity);
            ++progress.scanned;
            auto& slot = slots[index];
            if (!slot.cell.IsRetained())
                continue;
            ++progress.attempted;
            const auto released = slot.generation == generation ? memory.Release(slot.cell, nullptr, invoke) : ClockResult::OwnerConflict;
            if (!slot.cell.IsRetained())
                ++progress.released;
            if (result == ClockResult::Ok && released != ClockResult::Ok)
                result = released;
        }
        retireCursor = (start + progress.scanned) % static_cast<std::uint32_t>(Capacity);
        progress.retained = Publish();
        output = progress;
        return result != ClockResult::Ok ? result : progress.retained ? ClockResult::Busy : ClockResult::Ok;
    }
}

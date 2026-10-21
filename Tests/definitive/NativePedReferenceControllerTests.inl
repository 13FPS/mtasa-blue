/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise real retained-cell control without native GTA execution
 ****************************************************************************/
#include "../../Client/game_de/NativePedReferenceController.h"
#include <memory>

namespace
{
    void RunReferenceControllerTests(ReferenceFixture& fixture, NativePedReferenceMemory::Binding binding)
    {
        using Controller = NativePedReferenceController;
        using Token = NativePedReferenceToken;
        using Memory = NativePedReferenceMemory;
        using Progress = Controller::RetireProgress;
        static_assert(!std::is_copy_constructible_v<Controller> && !std::is_move_constructible_v<Controller>);
        static_assert(Controller::Capacity == NativePedPoolProfile::NativeCapacity);
        static_assert(Token::NextVersion(0) == 1 && Token::NextVersion(UINT64_MAX - 1) == UINT64_MAX && Token::NextVersion(UINT64_MAX) == 0);

        Memory     memory(binding, &PoolTestHost::Owner, &ReferenceTestHost::Recover, 42);
        const auto snapshot = [](const Memory::Receipt& receipt)
        {
            NativePedReferenceCell::State state;
            state.nativePed = receipt.nativePed;
            state.cellAddress = receipt.cell;
            state.node = receipt.node;
            return state;
        };
        const auto sameToken = [](const Token& first, const Token& second)
        { return first.slot == second.slot && first.version == second.version && first.ownerGeneration == second.ownerGeneration; };

        fixture.Reset();
        {
            // Fill the actual bounded controller through the real registration
            // primitive. Duplicate native targets still own distinct private
            // cells; this is deliberately not a fabricated one-wrapper-per-ped
            // SDK registry or a shortcut that mutates controller internals.
            auto controller = std::make_unique<Controller>();
            Check(!controller->IsRetained() && !controller->RecoveryPending(), "a fresh reference controller owns no native-visible storage");
            Token           old;
            Memory::Receipt receipt;
            Check(controller->Acquire(memory, 42, 1, old, &receipt) == ClockResult::Ok && old.slot == 0 && old.version == 1 && old.ownerGeneration == 42 &&
                      receipt.registrationVerified && controller->IsRetained() && !controller->RecoveryPending(),
                  "controller publication follows one actual verified native registration and preserves independent CPU identity");
            const auto                 firstAddress = receipt.cell;
            NativePedPoolReader::Value value;
            Check(controller->Resolve(memory, 42, old, value) == ClockResult::Ok && value.handle == 1 && value.nativePed == fixture.Ped(),
                  "resolving an issued CPU token performs the real fresh native cell and pool relation check");
            Check(controller->Release(memory, 42, old, &receipt) == ClockResult::Ok && !controller->IsRetained() && !controller->RecoveryPending(),
                  "ordinary token release acknowledges actual detached storage without stopping later acquisitions");
            CellFreed(firstAddress);

            std::array<Token, Controller::Capacity> tokens{};
            for (std::size_t i = 1; i < tokens.size(); ++i)
                Check(controller->Acquire(memory, 42, 1, tokens[i]) == ClockResult::Ok && tokens[i].slot == i,
                      "shared MTA free-slot selection visits each available CPU slot without moving native cell metadata");
            Check(controller->Acquire(memory, 42, 1, tokens[0]) == ClockResult::Ok && tokens[0].slot == old.slot && tokens[0].version == old.version + 1,
                  "a reused CPU slot advances its own version independently of the unchanged native pool handle");
            const auto registers = ReferenceTestHost::registers;
            Token      untouched;
            untouched.slot = 17;
            untouched.version = 19;
            untouched.ownerGeneration = 23;
            const auto sentinel = untouched;
            Check(controller->Acquire(memory, 42, 1, untouched, &receipt) == ClockResult::Busy && sameToken(untouched, sentinel) && !receipt.entered &&
                      ReferenceTestHost::registers == registers,
                  "fixed capacity exhaustion preserves caller output and never enters another native registration");
            value.handle = 0xdeadbeef;
            value.nativePed = 123;
            Check(controller->Resolve(memory, 42, old, value) != ClockResult::Ok && value.handle == 0xdeadbeef && value.nativePed == 123 &&
                      controller->Release(memory, 42, old, &receipt) != ClockResult::Ok && !receipt.entered,
                  "an old CPU version cannot resolve or clean up a new registration occupying the same slot");
            for (const auto index : std::array<std::uint32_t, 2>{static_cast<std::uint32_t>(Controller::Capacity), UINT32_MAX})
            {
                auto invalid = tokens[0];
                invalid.slot = index;
                Check(controller->Resolve(memory, 42, invalid, value) == ClockResult::InvalidArgument &&
                          controller->Release(memory, 42, invalid, &receipt) == ClockResult::InvalidArgument && !receipt.entered,
                      "shared signed/unsigned pool bounds reject malformed CPU token indices before any native access");
            }
            Check(controller->Resolve(memory, 43, tokens[0], value) == ClockResult::OwnerConflict &&
                      controller->Release(memory, 43, tokens[0], &receipt) == ClockResult::OwnerConflict && !receipt.entered,
                  "caller generation mismatch cannot borrow or retire another actual owner generation's cells");

            Progress progress;
            progress.scanned = progress.attempted = progress.released = progress.retained = 77;
            Check(controller->Retire(memory, 42, 0, progress) == ClockResult::InvalidArgument && progress.retained == 77 &&
                      controller->Resolve(memory, 42, tokens[0], value) == ClockResult::Ok,
                  "invalid retirement budgets preserve output and do not silently stop normal reference use");
            std::uint32_t remaining = static_cast<std::uint32_t>(Controller::Capacity);
            while (remaining)
            {
                const auto result = controller->Retire(memory, 42, 37, progress);
                const auto expected = remaining < 37 ? remaining : 37u;
                remaining -= expected;
                Check(progress.scanned <= Controller::Capacity && progress.attempted == expected && progress.released == expected &&
                          progress.retained == remaining && result == (remaining ? ClockResult::Busy : ClockResult::Ok) &&
                          controller->IsRetained() == (remaining != 0) && controller->RecoveryPending() == (remaining != 0),
                      "bounded End retirement reports genuine release progress and retains the unload gate until every private word is freed");
                Check(controller->Resolve(memory, 42, tokens[0], value) != ClockResult::Ok,
                      "global retirement revokes all native borrow tokens before the first cleanup action");
            }
            Check(ReferenceTestHost::unregisters == Controller::Capacity + 1,
                  "retirement executes exactly one native unlink per healthy registered cell, including the earlier ordinary release");
            Memory nextMemory(binding, &PoolTestHost::Owner, &ReferenceTestHost::Recover, 43);
            Token  next;
            Check(controller->Acquire(nextMemory, 43, 1, next, &receipt) == ClockResult::Ok && next.ownerGeneration == 43 && next.version == 2,
                  "a later actual owner generation can reuse fully retired storage while preserving CPU slot versions");
            Check(controller->Resolve(nextMemory, 43, tokens[next.slot], value) == ClockResult::OwnerConflict &&
                      controller->Release(nextMemory, 43, next, &receipt) == ClockResult::Ok && !controller->IsRetained(),
                  "an earlier generation token stays invalid even after actual registration resumes with the same native handle");
        }

        for (const bool resetBank : {false, true})
        {
            fixture.Reset();
            auto            controller = std::make_unique<Controller>();
            Token           token;
            Memory::Receipt receipt;
            Check(controller->Acquire(memory, 42, 1, token, &receipt) == ClockResult::Ok, "seed one issued reference before ordinary native invalidation");
            const auto registered = snapshot(receipt);
            if (resetBank)
                fixture.Reset();
            else
                fixture.Resolve(registered);
            NativePedPoolReader::Value value;
            value.handle = 0xdeadbeef;
            value.nativePed = 123;
            Check(controller->Resolve(memory, 42, token, value) != ClockResult::Ok && value.handle == 0xdeadbeef && value.nativePed == 123 &&
                      controller->IsRetained() && !controller->RecoveryPending(),
                  "native death or bank reset invalidates borrowing without turning an ordinary cleanup lifecycle into a whole-owner failure");
            Check(controller->Resolve(memory, 42, token, value) == ClockResult::NotInitialized &&
                      controller->Release(memory, 42, token, &receipt) == ClockResult::Ok && !receipt.entered && receipt.bankAbsent &&
                      receipt.allocationReleased && !controller->IsRetained() && !controller->RecoveryPending() && !ReferenceTestHost::unregisters,
                  "the same issued identity may release its invalidated cell after fresh full-bank absence, without reviving it or calling an old receiver");
            CellFreed(registered.cellAddress);
            Check(controller->Release(memory, 42, token, &receipt) != ClockResult::Ok && !receipt.entered,
                  "completed ordinary cleanup consumes the old CPU token permanently");
        }

        fixture.Reset();
        {
            auto  controller = std::make_unique<Controller>();
            Token token;
            token.slot = 7;
            token.version = 11;
            token.ownerGeneration = 13;
            const auto      sentinel = token;
            Memory::Receipt receipt;
            ReferenceTestHost::result = ClockResult::OriginalException;
            Check(controller->Acquire(memory, 42, 1, token, &receipt) == ClockResult::OriginalException && receipt.entered && !receipt.registrationVerified &&
                      sameToken(token, sentinel) && controller->IsRetained() && controller->RecoveryPending(),
                  "an entered registration fault creates retained unpublished controller debt rather than a token or a dangling freed cell");
            const auto uncertain = snapshot(receipt);
            ReferenceTestHost::result = ClockResult::Ok;
            Progress progress;
            Check(controller->Retire(memory, 42, 1, progress) != ClockResult::Ok && progress.attempted == 1 && !progress.released && progress.retained == 1 &&
                      !ReferenceTestHost::unregisters && ReferenceTestHost::registers == 1,
                  "uncertain publication remains retained and cannot replay native registration or invoke unregister through unverified identity");
            fixture.Resolve(uncertain);
            Check(controller->Retire(memory, 42, 1, progress) == ClockResult::Ok && progress.released == 1 && !progress.retained && !controller->IsRetained() &&
                      !controller->RecoveryPending() && !ReferenceTestHost::unregisters,
                  "later genuine native address absence retires unpublished fault debt without replaying any native action");
            CellFreed(uncertain.cellAddress);
        }

        fixture.Reset();
        {
            auto            controller = std::make_unique<Controller>();
            Token           token;
            Memory::Receipt receipt;
            Check(controller->Acquire(memory, 42, 1, token, &receipt) == ClockResult::Ok, "seed verified token before partially completed native cleanup");
            const auto registered = snapshot(receipt);
            ReferenceTestHost::leaveCellAddress = true;
            ReferenceTestHost::result = ClockResult::OriginalException;
            Check(controller->Release(memory, 42, token, &receipt) == ClockResult::OriginalException && receipt.entered && controller->IsRetained() &&
                      controller->RecoveryPending(),
                  "a cleanup fault retains native-visible storage even when the entity-head unlink has already occurred");
            NativePedPoolReader::Value value;
            const auto                 calls = ReferenceTestHost::unregisters;
            Check(controller->Resolve(memory, 42, token, value) != ClockResult::Ok &&
                      controller->Release(memory, 42, token, &receipt) == ClockResult::NotInitialized && !receipt.entered &&
                      ReferenceTestHost::unregisters == calls,
                  "a requested release revokes its token before native cleanup and a later caller cannot reuse it to repeat the action");
            ReferenceTestHost::result = ClockResult::Ok;
            Progress progress;
            Check(controller->Retire(memory, 42, 1, progress) != ClockResult::Ok && progress.retained == 1 && ReferenceTestHost::unregisters == calls,
                  "bounded recovery keeps an orphaned cell-address node as unload debt without replaying uncertain cleanup");
            fixture.Resolve(registered);
            Check(controller->Retire(memory, 42, 1, progress) == ClockResult::Ok && !controller->IsRetained() && !controller->RecoveryPending() &&
                      ReferenceTestHost::unregisters == calls,
                  "only observed full-bank cell-address absence clears the partial cleanup debt and controller unload gate");
            CellFreed(registered.cellAddress);
        }
        fixture.Reset();
        std::puts("Production native ped reference controller fixture completed");
    }
}

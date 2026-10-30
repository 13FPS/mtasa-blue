/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Original camera Restore with acknowledged engine reference effects
 ****************************************************************************/
#pragma once
#include "NativePlayerInfoMemory.h"
#include <atomic>
#include <memory>
namespace mta::game_de
{
    namespace NativeCameraRestoreCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        using Function = void (*)(std::uintptr_t);
        using Invoke = ClockResult (*)(Function, std::uintptr_t, std::uint32_t&) noexcept;
        using ExchangeBehind = ClockResult (*)(std::uintptr_t, std::uint8_t, std::uint8_t, std::uint8_t&, bool&, std::uint32_t&) noexcept;
        enum class Operation : std::uint8_t
        {
            Restore,
            JumpCut
        };
        struct Receipt
        {
            std::uintptr_t camera = 0, player = 0, entryRva = 0x111f710, root = 0, world = 0;
            std::uintptr_t beforeHead = 0, afterHead = 0, beforeFreeHead = 0, afterFreeHead = 0;
            std::uint32_t  handle = 0, playerMode = 0, changedReferenceNodes = 0, exceptionCode = 0;
            bool           entered = false, returned = false, effectsVerified = false, noEffect = false, debt = false;
            // The backend's named behind-player flag is first; its native
            // follow-ped consumer computes yaw later. No classic cache-field
            // equality is claimed. A known rejected CAS has no entered debt.
            Operation    operation = Operation::Restore;
            std::uint8_t behindBefore = 0, behindObserved = 0;
            bool         behindWriteEntered = false, behindWriteReturned = false, behindAccepted = false, behindVerified = false;
            bool         effectsEntered = false;
            ClockResult  result = ClockResult::NotInitialized;
        };
    }
    class NativeCameraRestore final
    {
    public:
        using CheckOwner = NativeCameraRestoreCalls::CheckOwner;
        using Receipt = NativeCameraRestoreCalls::Receipt;
        using Invoke = NativeCameraRestoreCalls::Invoke;
        NativeCameraRestore() noexcept;
        ~NativeCameraRestore();
        NativeCameraRestore(const NativeCameraRestore&) = delete;
        NativeCameraRestore& operator=(const NativeCameraRestore&) = delete;
        ClockResult          Restore(std::uintptr_t image, const NativePlayerInfoMemory&, CheckOwner, CheckOwner afterApplicationFrameOwner, Receipt* = nullptr,
                                     Invoke = nullptr) noexcept;
        ClockResult          RestoreWithJumpCut(std::uintptr_t image, const NativePlayerInfoMemory&, CheckOwner, CheckOwner afterApplicationFrameOwner,
                                                Receipt* = nullptr, Invoke = nullptr, NativeCameraRestoreCalls::ExchangeBehind = nullptr) noexcept;
        bool                 IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool                 RecoveryPending() const noexcept { return debt.load(); }

    private:
        // Camera reference cells belong to the engine image. An uncertain
        // original call keeps its CPU journal/module, never private-cell
        // cleanup authority, rollback stores or a second Restore attempt.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}

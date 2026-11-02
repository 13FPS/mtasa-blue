/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own the two exact movement bridges and controlled override intent
 ****************************************************************************/
#pragma once
#include "NativePlayerMoveAnimHookOverlay.h"
#include "NativePedPoolReader.h"
#include "NativePedReferenceToken.h"
#include "NativeFunctionRange.h"
#include <array>
#include <atomic>
#include <memory>

namespace mta::game_de
{
    class NativePlayerMoveAnimHookController;
    // The actual Runtime supplies these callbacks for one already-qualified
    // whole Rebuild invocation. Default/stock callers supply no association.
    // Enter suspends general writer TLS; only the named clothes-decision scope
    // is exposed until Leave restores the prior invocation in a finally block.
    struct NativePlayerMoveAnimHookCall
    {
        const NativePlayerMoveAnimHookController* controller = nullptr;
        ClockResult (*enter)(void*) noexcept = nullptr;
        void (*leave)(void*) noexcept = nullptr;
        void* context = nullptr;
    };

    class NativePlayerMoveAnimHookController final
    {
    public:
        using Kind = NativePlayerMoveAnimHookBridge::Kind;
        using Token = NativePedReferenceToken;
        using Ped = NativePedPoolReader::Value;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        struct Identity
        {
            std::uintptr_t image = 0;
            std::uint64_t  generation = 0, admissionSerial = 0;
            DWORD          engineThread = 0;
        };
        struct Owners
        {
            CheckOwner afterApplication = nullptr;
            ClockResult (*resolvePublication)(const Token&, Ped&) noexcept = nullptr;
            ClockResult (*decisionScope)(Kind, std::uintptr_t nativePed) noexcept = nullptr;
            ClockResult (*resolveDecision)(const Token&, Ped&) noexcept = nullptr;
            // Re-admit the chosen group/current native hierarchy rows. Earlier
            // setter validity is not a lease over an engine-owned group array.
            ClockResult (*qualifyDecisionGroup)(std::int32_t) noexcept = nullptr;
        };
        enum class Phase : std::uint32_t
        {
            Empty,
            Preparing,
            Prepared,
            Installing,
            Live,
            Closing,
            Detached,
            Retired,
            Unknown
        };
        struct SiteReceipt
        {
            std::uintptr_t               site = 0, stub = 0;
            std::array<unsigned char, 6> stock{}, owned{};
            std::uint32_t                size = 0;
            DWORD                        originalProtection = 0;
            bool                         protectionEntered = false, protectionChanged = false, writeEntered = false, writeReturned = false;
            bool                         flushAcknowledged = false, protectionRestored = false, verified = false, installed = false;
        };
        struct Receipt
        {
            Identity                   identity;
            Phase                      phase = Phase::Empty;
            ClockResult                result = ClockResult::NotInitialized;
            std::uintptr_t             allocation = 0;
            std::uint32_t              allocationBytes = 0;
            std::array<SiteReceipt, 2> sites{};
            bool                       allocationOwned = false, unwindEntered = false, unwindRegistered = false, codeRx = false, codeFlushed = false;
            bool                       unwindDeleteEntered = false, unwindDeleted = false, freeEntered = false, freed = false, uncertain = false;
            bool                       decisionsOpen = false;
            std::uint32_t              publishedOverrides = 0, inFlight = 0;
        };
        // Only the low-level OS boundaries are replaceable by owned fixtures.
        // No operation may allocate/log/lock user state while peers are parked.
        struct Operations
        {
            BOOL (*query)(LPCVOID, MEMORY_BASIC_INFORMATION*, SIZE_T) noexcept = nullptr;
            LPVOID (*allocate)(LPVOID, SIZE_T, DWORD, DWORD) noexcept = nullptr;
            BOOL (*protect)(LPVOID, SIZE_T, DWORD, PDWORD) noexcept = nullptr;
            BOOL (*write)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*) noexcept = nullptr;
            BOOL (*flush)(HANDLE, LPCVOID, SIZE_T) noexcept = nullptr;
            BOOLEAN (*addFunctions)(PRUNTIME_FUNCTION, DWORD, DWORD64) noexcept = nullptr;
            BOOLEAN (*deleteFunctions)(PRUNTIME_FUNCTION) noexcept = nullptr;
            BOOL (*free)(LPVOID, SIZE_T, DWORD) noexcept = nullptr;
            bool (*functionRange)(const void*, NativeFunctionRange&) noexcept = nullptr;
        };
        static const Operations& DefaultOperations() noexcept;
        explicit NativePlayerMoveAnimHookController(const Operations& = DefaultOperations()) noexcept;
        // Abandoning retained code also retains its CPU function table. A
        // destructor never frees memory still referenced by the OS unwinder;
        // the real enclosing owner must fence module unload with IsRetained.
        ~NativePlayerMoveAnimHookController();
        NativePlayerMoveAnimHookController(const NativePlayerMoveAnimHookController&) = delete;
        NativePlayerMoveAnimHookController& operator=(const NativePlayerMoveAnimHookController&) = delete;
        NativePlayerMoveAnimHookController(NativePlayerMoveAnimHookController&&) = delete;
        NativePlayerMoveAnimHookController& operator=(NativePlayerMoveAnimHookController&&) = delete;

        ClockResult Prepare(const Identity&, const Owners&, Receipt* = nullptr) noexcept;
        // The Runtime's checker must prove its actual current PeerHold is
        // prepared/parked and bound to this generation. A bool is no permit.
        ClockResult Install(CheckOwner parkedOwner, Receipt* = nullptr) noexcept;
        ClockResult Detach(CheckOwner parkedOwner, Receipt* = nullptr) noexcept;
        // After acknowledged detach, remove unwind/free UNPARKED: those OS
        // registries may need locks owned by a formerly parked peer.
        ClockResult Retire(CheckOwner quiescentProducer, Receipt* = nullptr) noexcept;
        void        Close() noexcept;

        ClockResult Publish(const Token&, const Ped&, std::int32_t custom) noexcept;
        ClockResult Withdraw(const Token&) noexcept;
        ClockResult WithdrawAll() noexcept;
        bool        HasOverride(const Token&) const noexcept;
        // CPU intent equality only; the writer still needs its genuine current
        // native owner, reference-cell and field/source bookends.
        bool MatchesOverride(const Token&, std::int32_t custom) const noexcept;
        bool Ready() const noexcept;
        bool IsRetained() const noexcept;
        bool RecoveryPending() const noexcept;
        bool Read(Receipt&) const noexcept;
        // Cache before parking. Besides count==0, ALL these IP ranges must be
        // excluded, including prologue/restore windows outside the counter.
        bool Exclusions(std::array<NativeFunctionRange, 4>&) const noexcept;
        bool MatchesProcess(std::uintptr_t image, const unsigned char*, std::size_t) const noexcept;
        bool MatchesRebuild(std::uintptr_t image, const unsigned char*, std::size_t) const noexcept;

    private:
        struct Data;
        std::unique_ptr<Data> data;
        static std::uint32_t  DecideProcess(std::uintptr_t, std::uint32_t, void*) noexcept;
        static std::uint32_t  DecideClothes(std::uintptr_t, std::uint32_t, void*) noexcept;
        std::uint32_t         Decide(Kind, std::uintptr_t, std::uint32_t) noexcept;
    };
}

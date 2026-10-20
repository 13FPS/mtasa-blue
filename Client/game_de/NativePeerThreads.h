/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain ownership of parked native peers through explicit recovery
 *****************************************************************************/
#pragma once

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <vector>

namespace mta::game_de
{
    static_assert(std::atomic<bool>::is_always_lock_free && std::atomic<std::uint32_t>::is_always_lock_free);
    // This owner borrows the current process, not a game address or engine
    // admission. Its caller must keep the owner and its module alive until all
    // handles retire, including after a failed ResumeThread. NativeClockService
    // is the source of this prepare/park/retain/retry algorithm.
    class NativePeerThreads final
    {
    public:
        static constexpr std::uint32_t MaximumPeers = 1024;
        static constexpr unsigned      MaximumRecoveryAttempts = 8;
        using ContextPolicy = bool (*)(const CONTEXT&, void*) noexcept;

        // Injection is limited to the parked-phase OS boundary. Enumeration and
        // opening handles always use real current-process thread identities.
        // Implementations must not allocate, take heap locks or throw.
        struct Operations
        {
            DWORD (*suspend)(HANDLE) noexcept = nullptr;
            BOOL (*context)(HANDLE, CONTEXT*) noexcept = nullptr;
            DWORD (*resume)(HANDLE) noexcept = nullptr;
            DWORD (*wait)(HANDLE, DWORD) noexcept = nullptr;
            BOOL (*close)(HANDLE) noexcept = nullptr;
            LONG (*registerDetours)(HANDLE) noexcept = nullptr;
        };

        struct State
        {
            std::uint32_t prepared = 0;
            std::uint32_t parked = 0;
            std::uint32_t retained = 0;
            bool          busy = false;
        };

        static const Operations& DefaultOperations() noexcept;
        explicit NativePeerThreads(const Operations& selected = DefaultOperations()) noexcept;
        ~NativePeerThreads();
        NativePeerThreads(const NativePeerThreads&) = delete;
        NativePeerThreads& operator=(const NativePeerThreads&) = delete;

        class Hold final
        {
        public:
            explicit Hold(NativePeerThreads& source) noexcept : owner(source) {}
            ~Hold();
            Hold(const Hold&) = delete;
            Hold& operator=(const Hold&) = delete;

            bool Prepare() noexcept;
            // Failure resumes/closes the partial hold, retaining failed cleanup
            // in the owner before returning. Inspect HasRecovery before doing
            // work which a still-parked peer might own.
            bool Suspend(ContextPolicy policy, void* context) noexcept;
            bool RegisterDetours() const noexcept;
            bool Release() noexcept;

        private:
            struct Peer
            {
                HANDLE handle = nullptr;
                bool   suspended = false;
            };
            NativePeerThreads& owner;
            std::vector<Peer>  peers;
            bool               ownsGate = false;
            bool               prepared = false;
            bool               parked = false;
            friend class NativePeerThreads;
        };

        bool  RetryRecover(unsigned attempts = MaximumRecoveryAttempts) noexcept;
        bool  HasRecovery() const noexcept { return retained.load() != 0; }
        State Read() const noexcept;

    private:
        bool                       ResumePeer(Hold::Peer& peer) const noexcept;
        bool                       ValidOperations() const noexcept;
        const Operations           operations;
        std::vector<Hold::Peer>    recovery;
        std::atomic<bool>          busy{false};
        std::atomic<std::uint32_t> prepared{0}, parked{0}, retained{0};
    };
}

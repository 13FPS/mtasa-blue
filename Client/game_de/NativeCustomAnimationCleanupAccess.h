/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same-owner cleanup commands and CPU-only ticket code lifetime
 ****************************************************************************/
#pragma once
#include "NativeAnimationHierarchyAccess.h"
#include <game/CCustomAnimationCleanup.h>
#include <atomic>
#include <memory>
#include <utility>
#include <thread>
namespace mta::game_de
{
    class NativeCustomAnimSequenceController;
    struct NativeCustomAnimationCleanupToken
    {
        std::uint64_t generation = 0, command = 0;
    };
    struct NativeCustomAnimationCleanupRequest
    {
        std::uint64_t                          generation = 0;
        NativeAnimationHierarchyReferenceToken hierarchy{};
        std::uintptr_t                         originalArray = 0;
        std::size_t                            capacity = 0;
    };
    class NativeCustomAnimationCleanupView;
    class NativeCustomAnimationCleanupLifetime final
    {
    private:
        friend class NativeCustomAnimSequenceController;
        friend class NativeCustomAnimationCleanupView;
        explicit NativeCustomAnimationCleanupLifetime(NativeCustomAnimationCleanupToken token) noexcept : token(token), creator(std::this_thread::get_id()) {}
        const NativeCustomAnimationCleanupToken token;
        std::atomic<std::size_t>                views{1};
        const std::thread::id                   creator;
        std::atomic<bool>                       dirty{false}, offCreatorDrop{false};
        // This is only CPU facade-code lifetime. Native resources and actual
        // detach/free receipts remain in the existing Q/H owner journals.
    };
    class NativeCustomAnimationCleanupView final
    {
    public:
        NativeCustomAnimationCleanupView() noexcept = default;
        NativeCustomAnimationCleanupView(const NativeCustomAnimationCleanupView&) = delete;
        NativeCustomAnimationCleanupView& operator=(const NativeCustomAnimationCleanupView&) = delete;
        NativeCustomAnimationCleanupView(NativeCustomAnimationCleanupView&& other) noexcept : state(std::move(other.state)) {}
        NativeCustomAnimationCleanupView& operator=(NativeCustomAnimationCleanupView&& other) noexcept
        {
            if (this != &other)
            {
                Release();
                state = std::move(other.state);
            }
            return *this;
        }
        ~NativeCustomAnimationCleanupView() { Release(); }
        bool                              IsIssued() const noexcept { return static_cast<bool>(state); }
        NativeCustomAnimationCleanupToken Token() const noexcept { return state ? state->token : NativeCustomAnimationCleanupToken{}; }

    private:
        friend class NativeCustomAnimSequenceController;
        explicit NativeCustomAnimationCleanupView(std::shared_ptr<NativeCustomAnimationCleanupLifetime> value) noexcept : state(std::move(value)) {}
        void Release() noexcept
        {
            if (state)
            {
                // A foreign-thread terminal drop cannot prove code quiescence
                // to the creator. Keep the external module fence quarantined.
                if (std::this_thread::get_id() != state->creator)
                    state->offCreatorDrop.store(true, std::memory_order_release);
                state->views.fetch_sub(1, std::memory_order_acq_rel);
                state->dirty.store(true, std::memory_order_release);
                state.reset();
            }
            // No native callback, record erasure, resource free or module
            // unload is legal here. The external owner rechecks after return.
        }
        std::shared_ptr<NativeCustomAnimationCleanupLifetime> state;
    };
    struct NativeCustomAnimationCleanupBeginReceipt
    {
        NativeCustomAnimationCleanupView view;
        // Once accepted, the view/handle must reach the preallocated facade
        // even if a late owner acknowledgement reports failure. It is not a
        // cleanup-complete result; Resume owns every later acknowledgement.
        bool accepted = false;
    };
    class NativeCustomAnimationCleanupAccess
    {
    public:
        virtual ~NativeCustomAnimationCleanupAccess() = default;
        virtual ClockResult BeginPrivateAnimationCleanup(const NativeCustomAnimationCleanupRequest&, NativeCustomAnimationCleanupBeginReceipt&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ResumePrivateAnimationCleanup(const NativeCustomAnimationCleanupToken&, SCustomAnimationCleanupResult&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Busy/missing proof must retain the actual provider/module. This
        // includes unresolved journals AND completed live CPU ticket views.
        virtual bool HasPendingPrivateAnimationCleanup() const noexcept { return true; }
    };
}

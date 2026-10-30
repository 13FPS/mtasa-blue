/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same-record CPU facade-code lifetime without native cleanup authority
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationAccess.h"
#include <atomic>
#include <limits>
#include <memory>
#include <thread>
#include <utility>
namespace mta::game_de
{
    class NativeAnimationAssociationController;
    class CAnimBlendAssociationDE;
    class NativeAnimationAssociationFacadeView;
    class NativeAnimationAssociationFacadeLifetime final
    {
    private:
        friend class NativeAnimationAssociationController;
        friend class NativeAnimationAssociationFacadeView;
        NativeAnimationAssociationFacadeLifetime(std::uintptr_t image, const NativeAnimationAssociationReferenceToken& token) noexcept
            : image(image), token(token), creator(std::this_thread::get_id())
        {
        }
        const std::uintptr_t                           image;
        const NativeAnimationAssociationReferenceToken token;
        const std::thread::id                          creator;
        std::atomic<std::size_t>                       views{0};
        std::atomic<bool>                              dirty{false}, offCreatorDrop{false};
    };
    class NativeAnimationAssociationFacadeView final
    {
    public:
        NativeAnimationAssociationFacadeView() noexcept = default;
        NativeAnimationAssociationFacadeView(const NativeAnimationAssociationFacadeView&) = delete;
        NativeAnimationAssociationFacadeView& operator=(const NativeAnimationAssociationFacadeView&) = delete;
        NativeAnimationAssociationFacadeView(NativeAnimationAssociationFacadeView&& value) noexcept : state(std::move(value.state)) {}
        NativeAnimationAssociationFacadeView& operator=(NativeAnimationAssociationFacadeView&& value) noexcept
        {
            if (this != &value)
            {
                Release();
                state = std::move(value.state);
            }
            return *this;
        }
        ~NativeAnimationAssociationFacadeView() { Release(); }
        bool                                     IsIssued() const noexcept { return static_cast<bool>(state); }
        NativeAnimationAssociationReferenceToken Token() const noexcept { return state ? state->token : NativeAnimationAssociationReferenceToken{}; }

    private:
        friend class NativeAnimationAssociationController;
        friend class CAnimBlendAssociationDE;
        explicit NativeAnimationAssociationFacadeView(std::shared_ptr<NativeAnimationAssociationFacadeLifetime> value) noexcept : state(std::move(value)) {}
        ClockResult ForkForHierarchy(const NativeAnimationAssociationReferenceToken& requested, NativeAnimationAssociationFacadeView& output) const noexcept
        {
            if (output.IsIssued())
                return ClockResult::InvalidArgument;
            if (!state)
                return ClockResult::NotInitialized;
            if (!requested.ownerGeneration || !requested.serial || requested.ownerGeneration != state->token.ownerGeneration ||
                requested.serial != state->token.serial || std::this_thread::get_id() != state->creator ||
                state->offCreatorDrop.load(std::memory_order_acquire))
                return ClockResult::OwnerConflict;
            auto views = state->views.load(std::memory_order_acquire);
            do
            {
                if (!views || views == (std::numeric_limits<std::size_t>::max)())
                    return ClockResult::OwnerConflict;
            } while (!state->views.compare_exchange_weak(views, views + 1, std::memory_order_acq_rel));
            // shared_ptr copy reuses this actual control block without a new
            // allocation. This extends code lifetime only, never native H/ref.
            output.state = state;
            return ClockResult::Ok;
        }
        void Release() noexcept
        {
            if (!state)
                return;
            // Mark a foreign drop before publishing count0. A creator-side
            // pump must never infer that foreign destructor code has returned.
            if (std::this_thread::get_id() != state->creator)
                state->offCreatorDrop.store(true, std::memory_order_release);
            state->views.fetch_sub(1, std::memory_order_acq_rel);
            state->dirty.store(true, std::memory_order_release);
            state.reset();
            // No provider call, native operation, record pruning or module
            // unload occurs here; the existing external owner keeps the fence.
        }
        std::shared_ptr<NativeAnimationAssociationFacadeLifetime> state;
    };
    struct NativeAnimationAssociationIssuedCopy
    {
        NativeAnimationAssociationReferenceToken token{};
        std::uintptr_t                           interfaceAddress = 0;
        NativeAnimationAssociationFacadeView     code;
    };
}

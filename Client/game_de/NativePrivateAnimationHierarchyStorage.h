/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Module-owned empty private hierarchy storage, never a GTA allocation
 ****************************************************************************/
#pragma once
#include "NativeAnimationHierarchyAccess.h"
#include <array>
#include <atomic>
namespace mta::game_de
{
    struct NativePrivateAnimationHierarchyStorageAccess;
    class NativePrivateAnimationHierarchyStorage final
    {
    public:
        enum class State : std::uint32_t
        {
            Fresh,
            Entered,
            Ready,
            Failed,
            PublicationPinned,
            Retired
        };
        // Actual MTA allocates a plain interface, then explicitly initializes
        // it. Keep the native40-byte hash/alignment bytes uninitialized.
        explicit NativePrivateAnimationHierarchyStorage(std::uint64_t generation) noexcept : generation(generation) {}
        ~NativePrivateAnimationHierarchyStorage() = default;
        NativePrivateAnimationHierarchyStorage(const NativePrivateAnimationHierarchyStorage&) = delete;
        NativePrivateAnimationHierarchyStorage& operator=(const NativePrivateAnimationHierarchyStorage&) = delete;
        NativePrivateAnimationHierarchyStorage(NativePrivateAnimationHierarchyStorage&&) = delete;
        NativePrivateAnimationHierarchyStorage& operator=(NativePrivateAnimationHierarchyStorage&&) = delete;
        State                                   GetState() const noexcept { return state.load(std::memory_order_acquire); }
        std::uint64_t                           OwnerGeneration() const noexcept { return generation; }
        std::uintptr_t                          Address() const noexcept { return reinterpret_cast<std::uintptr_t>(bytes.data()); }
        bool                                    CanDiscard() const noexcept { return GetState() == State::Retired; }

    private:
        friend struct NativePrivateAnimationHierarchyStorageAccess;
        alignas(std::uintptr_t) std::array<unsigned char, 40> bytes;
        std::uint64_t generation;
        // Even a const read must retain an observed foreign child/cache.
        // Clearing those raw fields later never proves native detachment.
        mutable std::atomic<State> state{State::Fresh};
        bool                       hashDefined = false;
        static_assert(sizeof(std::atomic<State>) == 4);
        // Explicit CPU bookkeeping padding keeps the native view exactly40B
        // and avoids implicit alignas tail padding under the normal /WX build.
        std::array<unsigned char, 3> padding{};
    };
    namespace NativePrivateAnimationHierarchyCalls
    {
        using Storage = NativePrivateAnimationHierarchyStorage;
        using CheckOwner = ClockResult (*)() noexcept;
        using Write = ClockResult (*)(std::uintptr_t, std::uint32_t&) noexcept;
        enum class ScalarOperation : std::uint32_t
        {
            RunningCompressed,
            AnimationBlockID
        };
        using ScalarWrite = ClockResult (*)(std::uintptr_t, ScalarOperation, std::uint32_t, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t hierarchy = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, accepted = false, retired = false, verified = false;
        };
        ClockResult Initialize(std::uintptr_t image, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr, Write = nullptr) noexcept;
        // Source-owned private scalar assignments only; all other40 bytes and
        // hash definedness survive, including accepted writes with lost ack.
        ClockResult SetRunningCompressed(std::uintptr_t image, Storage&, std::uint64_t, bool, CheckOwner, Receipt* = nullptr, ScalarWrite = nullptr) noexcept;
        ClockResult SetAnimationBlockID(std::uintptr_t image, Storage&, std::uint64_t, std::int32_t, CheckOwner, Receipt* = nullptr,
                                        ScalarWrite = nullptr) noexcept;
        ClockResult Read(std::uintptr_t image, const Storage&, std::uint64_t, CheckOwner, NativeAnimationHierarchyValues&) noexcept;
        // Only a known neverpublished empty/cache0 receiver may retire. The
        // controller performs plain CPU deletion after a retired receipt,
        // including a lost post-ack; no child/native destructor is invoked.
        ClockResult Retire(std::uintptr_t image, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr) noexcept;
        // Conservative irreversible bookkeeping for future real native entry.
        // No generic Unpin, map-erasure detach or child cleanup is provided.
        ClockResult PinPublication(Storage&, std::uint64_t) noexcept;
    }
}

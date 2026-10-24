/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine defaults for unattached clothes storage owned by the MTA module
 ****************************************************************************/
#pragma once
#include "NativeClothesDescriptorProfile.h"
#include <game/CGameClockService.h>
#include <array>
#include <atomic>
#include <cstdint>
namespace mta::game_de
{
    class NativeClothesDescriptorStorage;
    namespace NativeClothesDescriptorCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        using NativeFunction = void (*)(std::uintptr_t descriptor);
        using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t descriptor, std::uint32_t& exception) noexcept;
        struct Receipt
        {
            // Publish the CPU-owned address only after complete initialization;
            // failures carry no usable descriptor pointer or native lease.
            std::uintptr_t entryRva = 0, descriptor = 0, root = 0, gameMode = 0, level = 0, world = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, returned = false;
        };
        ClockResult Initialize(std::uintptr_t image, NativeClothesDescriptorStorage&, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
    }

    // Classic MTA owns each remote clothes allocation in its own module and
    // releases it after native ped teardown. For this unattached prerequisite,
    // the caller owns this CPU object: no native malloc/free, hidden allocation
    // registry, descriptor adoption or native-data publication is performed.
    class NativeClothesDescriptorStorage final
    {
    public:
        // The genuine initializer writes every descriptor byte. Keeping its
        // CPU buffer uninitialized avoids making zero-fill a fake substitute
        // for the required native name/cache initialization side effects.
        NativeClothesDescriptorStorage() noexcept {}
        ~NativeClothesDescriptorStorage() = default;
        NativeClothesDescriptorStorage(const NativeClothesDescriptorStorage&) = delete;
        NativeClothesDescriptorStorage& operator=(const NativeClothesDescriptorStorage&) = delete;
        NativeClothesDescriptorStorage(NativeClothesDescriptorStorage&&) = delete;
        NativeClothesDescriptorStorage& operator=(NativeClothesDescriptorStorage&&) = delete;
        bool                            IsInitialized() const noexcept { return state.load(std::memory_order_acquire) == State::Initialized; }

    private:
        friend ClockResult NativeClothesDescriptorCalls::Initialize(std::uintptr_t, NativeClothesDescriptorStorage&, NativeClothesDescriptorCalls::CheckOwner,
                                                                    NativeClothesDescriptorCalls::Receipt*, NativeClothesDescriptorCalls::Invoke) noexcept;
        enum class State : unsigned char
        {
            Fresh,
            Entered,
            Initialized,
            Failed
        };
        alignas(std::uintptr_t) std::array<unsigned char, NativeClothesDescriptorProfile::DescriptorBytes> bytes;
        std::atomic<State> state{State::Fresh};
        // Explicit CPU-only tail storage avoids implicit alignas padding;
        // the native descriptor view remains exactly0xe8 bytes above.
        static_assert(sizeof(std::atomic<State>) <= alignof(std::uintptr_t));
        std::array<unsigned char, alignof(std::uintptr_t) - sizeof(std::atomic<State>)> padding{};
    };
}

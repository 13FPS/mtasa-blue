/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Generation-bound native IPL work for MTA's actual world itinerary
 ****************************************************************************/
#pragma once
#include "NativeWorldIplAccess.h"
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativeWorldIplCalls
{
    using CheckOwner = ClockResult (*)() noexcept;
    using IplFilter = MtaGame::CGameWorldPolicy::IplFilter;
    using RemoveFunction = void (*)(std::int32_t);
    using AddFunction = void (*)(void*, void*, const void*);
    using ClearFunction = void (*)(void*);
    using InvokeRemove = ClockResult (*)(RemoveFunction, std::int32_t, std::uint32_t&) noexcept;
    using InvokeAdd = ClockResult (*)(AddFunction, void*, void*, const void*, std::uint32_t&) noexcept;
    using InvokeClear = ClockResult (*)(ClearFunction, void*, std::uint32_t&) noexcept;
    struct Invocations
    {
        InvokeRemove remove = nullptr;
        InvokeAdd    add = nullptr;
        InvokeClear  clear = nullptr;
    };
    enum class Phase : std::uint32_t
    {
        Unbound,
        Enabled,
        Disabled,
        Unknown
    };
    struct Receipt
    {
        // entered counts accepted byte-store boundaries plus native entries;
        // returned counts native returns, and flagStores counts acknowledged
        // byte stores. Uncertain accepted prefixes are never replayed.
        std::uint64_t generation = 0;
        Phase         before = Phase::Unbound, after = Phase::Unbound;
        ClockResult   result = ClockResult::NotInitialized;
        std::uint32_t selected = 0, entered = 0, returned = 0, flagStores = 0, removeAcks = 0, addAcks = 0, clearAcks = 0, exception = 0;
        bool          retained = false, unknown = false;
    };
    class Controller final
    {
    public:
        Controller() noexcept = default;
        Controller(const Controller&) = delete;
        Controller& operator=(const Controller&) = delete;
        // Engine-thread calls freshly bind the same runtime generation. A new
        // generation cannot erase an accepted disabled/unknown native prefix.
        ClockResult Ready(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplicationOwner) noexcept;
        ClockResult Disable(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplicationOwner, Receipt* = nullptr,
                            const Invocations* = nullptr) noexcept;
        ClockResult Enable(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplicationOwner, IplFilter, Receipt* = nullptr,
                           const Invocations* = nullptr) noexcept;
        // The real SDK restores its pools before Enable. End must never try to
        // manufacture that missing itinerary, roll flags back, or replay work.
        ClockResult End(std::uint64_t generation) noexcept;
        bool        Retained() const noexcept { return active.load(std::memory_order_acquire) || retained.load(std::memory_order_acquire); }
        bool        CanDiscard() const noexcept { return !Retained(); }

    private:
        ClockResult       Run(std::uintptr_t, std::uint64_t, CheckOwner, CheckOwner, bool enabled, IplFilter, Receipt*, const Invocations*) noexcept;
        ClockResult       Bind(std::uint64_t) noexcept;
        std::atomic_flag  lock = ATOMIC_FLAG_INIT;
        std::atomic<bool> active{false}, retained{false};
        std::uint64_t     generation = 0;
        Phase             phase = Phase::Unbound;
    };
}

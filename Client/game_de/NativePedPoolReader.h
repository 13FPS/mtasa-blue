/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe native script handles without inventing SDK ped wrappers
 ****************************************************************************/
#pragma once
#include "NativePedPoolProfile.h"
#include <game/CGameClockService.h>

namespace mta::game_de
{
    class NativePedPoolReader final
    {
    public:
        struct Binding
        {
            std::uintptr_t image = 0;
        };
        struct Value
        {
            std::uint32_t  handle = 0, slot = 0;
            std::uintptr_t pool = 0, objects = 0, flags = 0, nativePed = 0, vtable = 0, firstMethod = 0;
            std::int32_t   capacity = 0, cursor = 0;
            unsigned char  generation = 0, ownsAllocations = 0;
        };
        // This check must borrow the current existing native GameThread
        // callback/generation/image, not merely a producer's thread ID. It is
        // required again around every observation and grants no new owner.
        using CheckOwner = ClockResult (*)() noexcept;
        NativePedPoolReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Resolve(std::uint32_t handle, Value&) const noexcept;
        // Report a qualified absent handle separately from callback/profile
        // failure. An absent observation changes only present, never Value;
        // neither result creates a wrapper, registration or native lease.
        // Signed-negative handles follow the pinned early native null branch;
        // out-of-capacity nonnegative indices retain InvalidArgument because
        // that native consumer proves no bounded null branch for them.
        ClockResult Probe(std::uint32_t handle, bool& present, Value&) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;

    private:
        ClockResult Capture(std::uint32_t handle, Value&) const noexcept;
        ClockResult CaptureObserved(std::uint32_t handle, bool& present, Value&) const noexcept;
        Binding     binding;
        CheckOwner  owner = nullptr;
    };
    // Value is an immediate opaque observation, never a CPed/CPlayerPed, a
    // registration or a lifetime lease. Recheck before another immediate
    // consumer; do not dereference it after leaving its borrowed callback.
}

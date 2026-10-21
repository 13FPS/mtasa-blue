/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrowed observations of the actual asynchronous native startup pump
 *****************************************************************************/
#pragma once
#include "NativeApplicationFrameProfile.h"
#include "NativeClassChainAccess.h"
#include "NativeStartupCompletionProfile.h"
#include <game/CGameClockService.h>
#include <cstdint>
#include <limits>

namespace mta::game_de
{
    class NativeStartupCompletionAccess final
    {
    public:
        using ReadMemory = NativeClassChainAccess::ReadMemory;
        using CheckOwner = ClockResult (*)(void* receiver) noexcept;
        struct Value
        {
            std::uint32_t nativeState = 0;
            unsigned char request = 0, pumpActive = 0;

            // This is a native condition, not transaction success: the same
            // zero latch can precede a request. The retained owner must first
            // prove this generation actually entered and published a request.
            bool PumpRetired() const noexcept { return nativeState == 9 && request == 0 && pumpActive == 0; }
        };
        NativeStartupCompletionAccess(std::uintptr_t imageBase, ReadMemory guardedRead, CheckOwner checkOwner) noexcept
            : image(imageBase), read(guardedRead), owner(checkOwner)
        {
        }

        // CheckOwner must qualify the current exact image/profile and admitted
        // normal post-original native Tick receiver/generation/depth, including
        // initialized native GameThread identity and scoped GC-zero protocol.
        // Earlier menu/provider UObjects may legitimately retire during native
        // loading; no graph pointer is retained or demanded from a prior frame.
        ClockResult Read(void* receiver, Value& output) const noexcept
        {
            if (!receiver || !read || !owner)
                return ClockResult::InvalidArgument;
            auto result = owner(receiver);
            if (result != ClockResult::Ok)
                return result;
            Sample first{}, second{};
            if (!Observe(reinterpret_cast<std::uintptr_t>(receiver), first))
                return ClockResult::ReadFailure;
            result = owner(receiver);
            if (result != ClockResult::Ok)
                return result;
            if (!Observe(reinterpret_cast<std::uintptr_t>(receiver), second))
                return ClockResult::ReadFailure;
            if (!Equal(first, second) || !Valid(second))
                return ClockResult::BindingChanged;
            // Revalidate scope after all guarded copies, then publish only
            // scalar facts. Native state9 is not player/network readiness.
            result = owner(receiver);
            if (result != ClockResult::Ok)
                return result;
            output = second.value;
            return ClockResult::Ok;
        }

    private:
        struct Sample
        {
            std::uintptr_t receiver = 0, root = 0, vtable = 0, rootAfter = 0, vtableAfter = 0;
            Value          value{}, after{};
        };
        static bool Add(std::uintptr_t base, std::size_t offset, std::uintptr_t& output) noexcept
        {
            if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
                return false;
            output = base + offset;
            return true;
        }
        template <typename T>
        bool At(std::uintptr_t base, std::size_t offset, T& output) const noexcept
        {
            std::uintptr_t at = 0;
            return Add(base, offset, at) && sizeof(T) <= (std::numeric_limits<std::uintptr_t>::max)() - at && read(at, &output, sizeof(T));
        }
        bool Facts(std::uintptr_t receiver, Value& output) const noexcept
        {
            namespace P = NativeStartupCompletionProfile;
            return At(image, P::StateRva, output.nativeState) && At(image, P::RequestRva, output.request) && At(receiver, P::PumpOffset, output.pumpActive);
        }
        bool Observe(std::uintptr_t receiver, Sample& output) const noexcept
        {
            namespace P = NativeStartupCompletionProfile;
            Sample next{};
            next.receiver = receiver;
            if (!At(image, P::RootRva, next.root) || next.root != receiver || !At(receiver, 0, next.vtable) || !Facts(receiver, next.value) ||
                !Facts(receiver, next.after) || !At(receiver, 0, next.vtableAfter) || !At(image, P::RootRva, next.rootAfter))
                return false;
            output = next;
            return true;
        }
        static bool Equal(const Value& a, const Value& b) noexcept
        {
            return a.nativeState == b.nativeState && a.request == b.request && a.pumpActive == b.pumpActive;
        }
        static bool Equal(const Sample& a, const Sample& b) noexcept
        {
            return a.receiver == b.receiver && a.root == b.root && a.vtable == b.vtable && a.rootAfter == b.rootAfter && a.vtableAfter == b.vtableAfter &&
                   Equal(a.value, b.value) && Equal(a.after, b.after);
        }
        bool Valid(const Sample& sample) const noexcept
        {
            std::uintptr_t expectedVtable = 0;
            return Add(image, NativeStartupCompletionProfile::VtableRva, expectedVtable) && sample.root == sample.receiver && sample.rootAfter == sample.root &&
                   sample.vtable == expectedVtable && sample.vtableAfter == sample.vtable && Equal(sample.value, sample.after) &&
                   sample.value.nativeState >= 6 && sample.value.nativeState <= 9 && sample.value.request <= 1 && sample.value.pumpActive <= 1;
        }
        const std::uintptr_t image;
        const ReadMemory     read;
        const CheckOwner     owner;
    };
}

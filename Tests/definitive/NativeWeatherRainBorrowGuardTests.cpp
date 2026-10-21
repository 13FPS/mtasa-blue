/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real owned-thread TEB exclusion and bounded controlled stack scans
 *****************************************************************************/
#include "../../Client/game_de/NativeWeatherRainBorrowGuard.h"
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
    namespace Borrow = mta::game_de::NativeWeatherRainBorrow;
    int  checks = 0;
    bool Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            std::printf("FAIL %d: %s\n", checks, reason);
        return condition;
    }
    struct Worker final
    {
        HANDLE           ready = nullptr;
        volatile LONG    stop = 0;
        bool             fiber = false;
        volatile LONG64* marker = nullptr;
    };
    DWORD WINAPI Run(void* raw)
    {
        auto& control = *static_cast<Worker*>(raw);
        if (control.fiber && !ConvertThreadToFiber(nullptr))
            return 1;
        volatile LONG64 marker = 0;
        control.marker = &marker;
        SetEvent(control.ready);
        while (!InterlockedCompareExchange(&control.stop, 0, 0))
            YieldProcessor();
        if (control.fiber && !ConvertFiberToThread())
            return 2;
        return marker ? 3 : 0;
    }
    bool OwnedThread(bool fiber)
    {
        Borrow::Guard            guard;
        bool                     ok = Check(guard.Prepare(), "current-thread API/TEB calibration succeeds before any peer is parked");
        const Borrow::Exclusions exclusions{0x710000000000, {}, {}};
        if (!fiber)
            ok &= Check(guard.InspectCurrent(exclusions) == Borrow::Result::Clear,
                        "real public current-stack bounds and captured context admit a clean active stack");
        Worker control{};
        control.fiber = fiber;
        control.ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        const auto handle = control.ready ? CreateThread(nullptr, 0, Run, &control, 0, nullptr) : nullptr;
        ok &= Check(handle && WaitForSingleObject(control.ready, 5000) == WAIT_OBJECT_0 && control.marker,
                    "owned normal/fiber worker publishes its real stack marker before parking");
        Borrow::Guard::Peer peer{};
        if (handle && control.marker)
        {
            const bool prepared = guard.PreparePeer(handle, peer);
            ok &= Check(prepared != fiber, "preparation admits a calibrated ordinary TEB and refuses a fiber TEB");
            if (prepared)
            {
                const bool parked = SuspendThread(handle) != static_cast<DWORD>(-1);
                ok &= Check(parked, "the fixture owns the suspension of its own worker only");
                if (parked)
                {
                    ok &= Check(guard.InspectParked(peer, exclusions) == Borrow::Result::Clear,
                                "actual suspended context uses independent post-suspend NT_TIB stack bounds");
                    for (const auto ret : {0x12c9d7e, 0x12c9db1, 0x12c9d55})
                    {
                        InterlockedExchange64(control.marker, static_cast<LONG64>(exclusions.image + ret));
                        ok &= Check(guard.InspectParked(peer, exclusions) == Borrow::Result::Borrowed,
                                    "actual owned stack marker at each old/new Rain return refuses retirement");
                    }
                    InterlockedExchange64(control.marker, 0);
                    const auto original = peer;
                    peer.teb += sizeof(std::uintptr_t);
                    ok &=
                        Check(guard.InspectParked(peer, exclusions) != Borrow::Result::Clear, "a moved prepared TEB cannot become independent stack authority");
                    peer = original;
                    ok &= Check(ResumeThread(handle) != static_cast<DWORD>(-1), "fixture releases its owned suspension");
                }
            }
        }
        if (handle)
        {
            InterlockedExchange(&control.stop, 1);
            ok &= Check(WaitForSingleObject(handle, 5000) == WAIT_OBJECT_0, "owned worker exits normally after exclusion checks");
        }
        if (handle)
            CloseHandle(handle);
        if (control.ready)
            CloseHandle(control.ready);
        return ok;
    }

    std::array<std::uintptr_t, 1025> words{};
    std::uintptr_t                   failRead = 0;
    bool                             failPages = false;
    bool                             Read(std::uintptr_t address, void* output, std::size_t length) noexcept
    {
        const auto begin = reinterpret_cast<std::uintptr_t>(words.data()), end = begin + sizeof(words);
        if (!output || address < begin || address > end || length > end - address || (failRead && address <= failRead && failRead - address < length))
            return false;
        std::memcpy(output, reinterpret_cast<const void*>(address), length);
        return true;
    }
    bool Pages(std::uintptr_t address, std::size_t length) noexcept
    {
        const auto begin = reinterpret_cast<std::uintptr_t>(words.data()), end = begin + sizeof(words);
        return !failPages && address >= begin && address <= end && length <= end - address;
    }
    bool Controlled()
    {
        const auto         begin = reinterpret_cast<std::uintptr_t>(words.data());
        Borrow::Stack      stack{begin, begin + sizeof(words), begin, 0x12345000, false};
        Borrow::Exclusions exclusions{0x410000000000, {0x100000, 0x100019}, {0x200000, 0x210000}};
        Borrow::Buffer     buffer{};
        auto               inspect = [&]() { return Borrow::Inspect(stack, exclusions, Read, Pages, buffer); };
        bool               ok = Check(inspect() == Borrow::Result::Clear, "a completely readable independently bounded controlled stack is clear");
        for (const auto offset : {0x12c9d7e, 0x12c9db1, 0x12c9d55})
            for (const auto index : {std::size_t{0}, std::size_t{512}, words.size() - 1})
            {
                words[index] = exclusions.image + offset;
                ok &= Check(inspect() == Borrow::Result::Borrowed, "old/new return is detected at first, chunk-boundary and last active word");
                words[index] = 0;
            }
        for (const auto rip : {exclusions.image + 0x12c91d0, exclusions.image + 0x12ca886, exclusions.leaf.begin, exclusions.leaf.end - 1,
                               exclusions.bodyModule.begin, exclusions.bodyModule.end - 1})
        {
            stack.rip = rip;
            ok &= Check(inspect() == Borrow::Result::Borrowed, "whole native serializer, leaf and peer body-module instruction ranges refuse");
        }
        stack.rip = exclusions.bodyModule.begin;
        stack.current = true;
        ok &= Check(inspect() == Borrow::Result::Clear, "the real current guard may execute in the owner module; its stack remains independently scanned");
        stack.current = false;
        stack.rip = 0x12345000;
        failRead = begin + 512 * sizeof(std::uintptr_t);
        ok &= Check(inspect() == Borrow::Result::ReadFailure, "a later chunk read failure cannot become a clear stack");
        failRead = 0;
        failPages = true;
        ok &= Check(inspect() == Borrow::Result::ReadFailure, "an inaccessible/protected committed-stack page refuses before reading");
        failPages = false;
        const auto valid = stack;
        ++stack.rsp;
        ok &= Check(inspect() == Borrow::Result::Unsupported, "unaligned captured RSP refuses");
        stack = valid;
        stack.limit = stack.rsp + 8;
        ok &= Check(inspect() == Borrow::Result::Unsupported, "RSP outside independently qualified bounds refuses");
        stack = valid;
        stack.base = stack.rsp + Borrow::MaximumActiveBytes + 8;
        ok &= Check(inspect() == Borrow::Result::Unsupported, "large/unknown active stack refuses without a broad process scan");
        stack = valid;
        exclusions.leaf.end = exclusions.leaf.begin;
        ok &= Check(inspect() == Borrow::Result::Unsupported, "invalid code exclusion cannot silently remove an acquisition-gap guard");
        return ok;
    }
}

int main()
{
    // Real OS cases run before controlled return-word fixtures, which would
    // intentionally leave conservative false-positive values in our stack.
    bool ok = OwnedThread(false);
    ok &= OwnedThread(true);
    ok &= Controlled();
    std::printf("Rain borrow guard: %d checks, %s\n", checks, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

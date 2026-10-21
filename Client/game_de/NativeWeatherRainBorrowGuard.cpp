/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Use real NT_TIB/current-thread bounds for Rain borrow exclusion
 *****************************************************************************/
#include "NativeWeatherRainBorrowGuard.h"
#include <cstring>
#include <limits>

namespace mta::game_de::NativeWeatherRainBorrow
{
    namespace
    {
        struct BasicThreadInformation final
        {
            LONG           exitStatus;
            ULONG          padding;
            void*          teb;
            std::uintptr_t process, thread, affinity;
            LONG           priority, basePriority;
        };
        static_assert(sizeof(BasicThreadInformation) == 48 && offsetof(BasicThreadInformation, teb) == 8 && offsetof(BasicThreadInformation, process) == 16 &&
                      sizeof(std::uintptr_t) == 8);

        bool Read(std::uintptr_t address, void* output, std::size_t length) noexcept
        {
            SIZE_T actual = 0;
            return address && output && length && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, length, &actual) &&
                   actual == length;
        }
        bool Pages(std::uintptr_t address, std::size_t length) noexcept
        {
            if (!address || !length || length > std::numeric_limits<std::uintptr_t>::max() - address)
                return false;
            const auto     end = address + length;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    info.Type != MEM_PRIVATE || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || (info.Protect & 0xff) != PAGE_READWRITE || !info.RegionSize)
                    return false;
                const auto region = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (region > address || info.RegionSize > std::numeric_limits<std::uintptr_t>::max() - region)
                    return false;
                const auto next = region + info.RegionSize;
                if (next <= address)
                    return false;
                const auto identity = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                if (!identity || (allocation && allocation != identity))
                    return false;
                allocation = identity;
                address = next < end ? next : end;
            }
            return true;
        }
        bool SameTib(const NT_TIB& first, const NT_TIB& second) noexcept
        {
            return first.Self == second.Self && first.StackBase == second.StackBase && first.StackLimit == second.StackLimit &&
                   first.FiberData == second.FiberData && first.SubSystemTib == second.SubSystemTib;
        }
        template <typename Function>
        Function Resolve(HMODULE module, const char* name) noexcept
        {
            const auto address = GetProcAddress(module, name);
            static_assert(sizeof(Function) == sizeof(address));
            Function result = nullptr;
            std::memcpy(&result, &address, sizeof(result));
            return result;
        }
    }

    Result Inspect(const Stack& stack, const Exclusions& exclusions, ReadMemory read, CheckPages pages, Buffer& buffer) noexcept
    {
        if (!read || !pages || !exclusions.image ||
            exclusions.image > std::numeric_limits<std::uintptr_t>::max() - NativeWeatherRainProfile::SerializerEndRva || !stack.limit ||
            stack.limit > stack.rsp || stack.rsp >= stack.base || stack.rsp % sizeof(std::uintptr_t) || stack.base % sizeof(std::uintptr_t) ||
            stack.base - stack.rsp > MaximumActiveBytes ||
            ((exclusions.leaf.begin || exclusions.leaf.end) && (!exclusions.leaf.begin || exclusions.leaf.begin >= exclusions.leaf.end)) ||
            ((exclusions.bodyModule.begin || exclusions.bodyModule.end) &&
             (!exclusions.bodyModule.begin || exclusions.bodyModule.begin >= exclusions.bodyModule.end)))
            return Result::Unsupported;
        const Range serializer{exclusions.image + NativeWeatherRainProfile::SerializerBeginRva, exclusions.image + NativeWeatherRainProfile::SerializerEndRva};
        if (serializer.Contains(stack.rip) || exclusions.leaf.Contains(stack.rip) || (!stack.current && exclusions.bodyModule.Contains(stack.rip)))
            return Result::Borrowed;
        // The two old native return sites cover a callee below either original
        // Rain pointer escape. The new CALL return covers Body's acquisition,
        // finally and epilogue gaps independently of its activity counter.
        std::array<std::uintptr_t, NativeWeatherRainProfile::BorrowReturns.size()> returns{};
        for (std::size_t index = 0; index < returns.size(); ++index)
            returns[index] = exclusions.image + NativeWeatherRainProfile::BorrowReturns[index];
        if (!pages(stack.rsp, static_cast<std::size_t>(stack.base - stack.rsp)))
            return Result::ReadFailure;
        for (auto cursor = stack.rsp; cursor < stack.base;)
        {
            const auto available = static_cast<std::size_t>((stack.base - cursor) / sizeof(std::uintptr_t));
            const auto count = available < buffer.size() ? available : buffer.size();
            const auto bytes = count * sizeof(std::uintptr_t);
            if (!bytes || !pages(cursor, bytes) || !read(cursor, buffer.data(), bytes))
                return Result::ReadFailure;
            for (std::size_t index = 0; index < count; ++index)
                for (const auto target : returns)
                    if (buffer[index] == target)
                        return Result::Borrowed;
            cursor += bytes;
        }
        return Result::Clear;
    }

    bool Guard::QueryTeb(HANDLE handle, DWORD expectedThread, std::uintptr_t& output) const noexcept
    {
        if (!queryThread || !handle || !expectedThread)
            return false;
        BasicThreadInformation info{};
        ULONG                  returned = 0;
        // ThreadBasicInformation is an internal API layout. Accept only the
        // exact Win64 shape calibrated against our public current-thread facts
        // during Prepare; unsupported OS/layout results refuse gracefully.
        if (queryThread(handle, 0, &info, static_cast<ULONG>(sizeof(info)), &returned) < 0 || returned != sizeof(info) || info.process != processId ||
            info.thread != expectedThread || !info.teb || reinterpret_cast<std::uintptr_t>(info.teb) % alignof(NT_TIB))
            return false;
        output = reinterpret_cast<std::uintptr_t>(info.teb);
        return true;
    }
    bool Guard::ReadTib(std::uintptr_t address, NT_TIB& output) const noexcept
    {
        NT_TIB tib{};
        if (!Pages(address, sizeof(tib)) || !Read(address, &tib, sizeof(tib)) || reinterpret_cast<std::uintptr_t>(tib.Self) != address || tib.SubSystemTib ||
            reinterpret_cast<std::uintptr_t>(tib.FiberData) != nonFiberToken || !tib.StackLimit ||
            reinterpret_cast<std::uintptr_t>(tib.StackLimit) >= reinterpret_cast<std::uintptr_t>(tib.StackBase))
            return false;
        output = tib;
        return true;
    }
    bool Guard::Prepare() noexcept
    {
        prepared = false;
        const auto ntdll = GetModuleHandleW(L"ntdll.dll"), kernel = GetModuleHandleW(L"kernel32.dll");
        if (!ntdll || !kernel)
            return false;
        queryThread = Resolve<QueryThread>(ntdll, "NtQueryInformationThread");
        stackLimits = Resolve<StackLimits>(kernel, "GetCurrentThreadStackLimits");
        isFiber = Resolve<IsFiber>(kernel, "IsThreadAFiber");
        if (!queryThread || !stackLimits || !isFiber || isFiber())
            return false;
        processId = GetCurrentProcessId();
        std::uintptr_t teb = 0;
        if (!QueryTeb(GetCurrentThread(), GetCurrentThreadId(), teb) || teb != reinterpret_cast<std::uintptr_t>(NtCurrentTeb()))
            return false;
        NT_TIB tib{};
        if (!Pages(teb, sizeof(tib)) || !Read(teb, &tib, sizeof(tib)))
            return false;
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        nonFiberToken = reinterpret_cast<std::uintptr_t>(tib.FiberData);
        // The public API has established that this is not a fiber. Its NT_TIB
        // union holds a low version token, not a user-addressable fiber object.
        // Refuse unfamiliar tokens; never treat an arbitrary pointer as proof
        // that a parked peer is on its system thread stack.
        if (nonFiberToken >= reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress) || !ReadTib(teb, tib))
            return false;
        ULONG_PTR low = 0, high = 0;
        stackLimits(&low, &high);
        if (!low || low > reinterpret_cast<std::uintptr_t>(tib.StackLimit) || high != reinterpret_cast<std::uintptr_t>(tib.StackBase))
            return false;
        prepared = true;
        return true;
    }
    bool Guard::PreparePeer(HANDLE handle, Peer& output) const noexcept
    {
        if (!prepared || !handle || GetProcessIdOfThread(handle) != processId)
            return false;
        const auto     thread = GetThreadId(handle);
        std::uintptr_t teb = 0;
        NT_TIB         tib{};
        if (!thread || thread == GetCurrentThreadId() || !QueryTeb(handle, thread, teb) || !ReadTib(teb, tib))
            return false;
        output = {handle, thread, teb};
        return true;
    }
    Result Guard::InspectParked(const Peer& peer, const Exclusions& exclusions) noexcept
    {
        if (!prepared || !peer.handle || !peer.threadId || !peer.teb || GetThreadId(peer.handle) != peer.threadId ||
            GetProcessIdOfThread(peer.handle) != processId)
            return Result::Unsupported;
        CONTEXT context{};
        context.ContextFlags = CONTEXT_CONTROL;
        NT_TIB before{}, after{};
        if (!GetThreadContext(peer.handle, &context) || !ReadTib(peer.teb, before))
            return Result::ReadFailure;
        const Stack stack{reinterpret_cast<std::uintptr_t>(before.StackLimit), reinterpret_cast<std::uintptr_t>(before.StackBase),
                          static_cast<std::uintptr_t>(context.Rsp), static_cast<std::uintptr_t>(context.Rip), false};
        const auto  result = Inspect(stack, exclusions, Read, Pages, buffer);
        if (!ReadTib(peer.teb, after))
            return Result::ReadFailure;
        if (!SameTib(before, after))
            return Result::Changed;
        return result;
    }
    Result Guard::InspectCurrent(const Exclusions& exclusions) noexcept
    {
        if (!prepared || !isFiber || isFiber())
            return Result::Unsupported;
        ULONG_PTR low = 0, high = 0, laterLow = 0, laterHigh = 0;
        stackLimits(&low, &high);
        const auto teb = reinterpret_cast<std::uintptr_t>(NtCurrentTeb());
        NT_TIB     before{}, after{};
        CONTEXT    context{};
        if (!ReadTib(teb, before) || low > reinterpret_cast<std::uintptr_t>(before.StackLimit) || high != reinterpret_cast<std::uintptr_t>(before.StackBase))
            return Result::Unsupported;
        RtlCaptureContext(&context);
        const Stack stack{low, high, static_cast<std::uintptr_t>(context.Rsp), static_cast<std::uintptr_t>(context.Rip), true};
        const auto  result = Inspect(stack, exclusions, Read, Pages, buffer);
        stackLimits(&laterLow, &laterHigh);
        if (!ReadTib(teb, after))
            return Result::ReadFailure;
        if (isFiber() || low != laterLow || high != laterHigh || !SameTib(before, after))
            return Result::Changed;
        return result;
    }
}

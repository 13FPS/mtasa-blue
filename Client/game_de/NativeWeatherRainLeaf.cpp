/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own a near RX leaf without publishing or retiring native authority
 *****************************************************************************/
#include "NativeWeatherRainLeaf.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace mta::game_de
{
    namespace
    {
        namespace Bridge = NativeWeatherRainBridge;
        constexpr std::size_t LeafOffset = 64;

        bool Add(std::uintptr_t value, std::size_t size, std::uintptr_t& output) noexcept
        {
            if (value > std::numeric_limits<std::uintptr_t>::max() - size)
                return false;
            output = value + size;
            return true;
        }

        bool Align(std::uintptr_t value, std::size_t alignment, std::uintptr_t& output) noexcept
        {
            std::uintptr_t rounded = 0;
            if (!alignment || (alignment & (alignment - 1)) || !Add(value, alignment - 1, rounded))
                return false;
            output = rounded & ~(static_cast<std::uintptr_t>(alignment) - 1);
            return true;
        }

        bool CopyOut(std::uintptr_t source, void* target, std::size_t size) noexcept
        {
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(source), target, size, &copied) && copied == size;
        }

        bool CopyIn(std::uintptr_t target, const void* source, std::size_t size) noexcept
        {
            SIZE_T copied = 0;
            return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(target), source, size, &copied) && copied == size;
        }

        bool PrivatePage(std::uintptr_t allocation, std::uintptr_t address, std::size_t size, DWORD protection = 0) noexcept
        {
            MEMORY_BASIC_INFORMATION memory{};
            if (VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory) ||
                reinterpret_cast<std::uintptr_t>(memory.AllocationBase) != allocation || memory.State != MEM_COMMIT || memory.Type != MEM_PRIVATE ||
                (protection && memory.Protect != protection))
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
            return address >= begin && address - begin <= memory.RegionSize && size <= memory.RegionSize - (address - begin);
        }

        void* AllocateRange(std::uintptr_t first, std::uintptr_t last, std::size_t bytes, std::size_t granularity) noexcept
        {
            std::uintptr_t cursor = 0;
            if (first > last || !Align(first, granularity, cursor))
                return nullptr;
            // At most the entire signed-rel32 address window at Windows'
            // allocation granularity. A failed exact reservation never grants
            // permission to fall back to an arbitrary distant allocation.
            for (unsigned attempts = 0; cursor <= last && attempts != 65536; ++attempts)
            {
                MEMORY_BASIC_INFORMATION memory{};
                if (VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) != sizeof(memory))
                    return nullptr;
                const auto     begin = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
                std::uintptr_t end = 0;
                if (!Add(begin, memory.RegionSize, end) || end <= cursor)
                    return nullptr;
                if (memory.State == MEM_FREE && cursor >= begin && bytes <= end - cursor)
                {
                    if (void* allocation = VirtualAlloc(reinterpret_cast<void*>(cursor), bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
                        return allocation;
                    if (!Add(cursor, granularity, cursor))
                        return nullptr;
                }
                else if (!Align(end, granularity, cursor))
                    return nullptr;
            }
            return nullptr;
        }
    }

    bool NativeWeatherRainLeaf::AllocationIdentity() const noexcept
    {
        if (!state.allocationBase || !state.pageBytes || state.allocationBytes != state.pageBytes * 2 || state.leafEntry != state.allocationBase + LeafOffset ||
            reinterpret_cast<std::uintptr_t>(state.context) != state.allocationBase + state.pageBytes)
            return false;
        return PrivatePage(state.allocationBase, state.allocationBase, state.pageBytes) &&
               PrivatePage(state.allocationBase, reinterpret_cast<std::uintptr_t>(state.context), state.pageBytes);
    }

    bool NativeWeatherRainLeaf::ContextIdentity() const noexcept
    {
        Bridge::Context observed{};
        return CopyOut(reinterpret_cast<std::uintptr_t>(state.context), &observed, sizeof(observed)) && observed.nativeRain == image + Bridge::NativeRainRva &&
               observed.saveVtable == image + Bridge::SaveVtableRva && observed.loadVtable == image + Bridge::LoadVtableRva &&
               observed.saveSerialize == image + Bridge::SaveSerializeRva && observed.loadSerialize == image + Bridge::LoadSerializeRva;
    }

    bool NativeWeatherRainLeaf::Prepare(std::uintptr_t requestedImage) noexcept
    {
        if (state.allocationBase)
        {
            state.failure = Failure::AlreadyAllocated;
            return false;
        }
        constexpr auto greatestRva = std::max(
            {Bridge::NativeRainRva, Bridge::SaveVtableRva, Bridge::LoadVtableRva, Bridge::SaveSerializeRva, Bridge::LoadSerializeRva, Bridge::EntryRva + 5});
        if (!requestedImage || requestedImage % 8 || requestedImage > std::numeric_limits<std::uintptr_t>::max() - greatestRva)
        {
            state.failure = Failure::InvalidImage;
            return false;
        }
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const std::size_t page = system.dwPageSize, granularity = system.dwAllocationGranularity;
        if (page < LeafOffset + Bridge::Plan{}.bridge.size() || page < sizeof(Bridge::Context) || page > std::numeric_limits<std::size_t>::max() / 2 ||
            (page & (page - 1)) || granularity < page || (granularity & (granularity - 1)))
        {
            state.failure = Failure::Allocation;
            return false;
        }
        const auto               minimum = reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
        const auto               maximum = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
        const auto               source = requestedImage + Bridge::EntryRva + 5;
        constexpr std::uintptr_t backward = std::uintptr_t{1} << 31;
        constexpr std::uintptr_t forward = backward - 1;
        const auto               low = std::max(minimum, source >= backward + LeafOffset ? source - backward - LeafOffset : std::uintptr_t{0});
        std::uintptr_t top = source > std::numeric_limits<std::uintptr_t>::max() - forward ? std::numeric_limits<std::uintptr_t>::max() : source + forward;
        if (maximum < page * 2 - 1 || top < LeafOffset)
        {
            state.failure = Failure::Allocation;
            return false;
        }
        top = std::min(top - LeafOffset, maximum - (page * 2 - 1));
        void* allocation = AllocateRange(std::max(low, source), top, page * 2, granularity);
        if (!allocation && source > low)
            allocation = AllocateRange(low, std::min(top, source - 1), page * 2, granularity);
        if (!allocation)
        {
            state.failure = Failure::Allocation;
            return false;
        }

        // Capture the actual reservation base immediately. Every later failure
        // keeps explicit debt, including failure before a single leaf byte was
        // published; a destructor cannot infer entry or stack retirement.
        state.allocationBase = reinterpret_cast<std::uintptr_t>(allocation);
        state.allocationBytes = page * 2;
        state.pageBytes = page;
        state.leafEntry = state.allocationBase + LeafOffset;
        state.context = reinterpret_cast<Bridge::Context*>(state.allocationBase + page);
        image = requestedImage;
        if (!AllocationIdentity() || !PrivatePage(state.allocationBase, state.allocationBase, page * 2, PAGE_READWRITE))
        {
            state.failure = Failure::Identity;
            return false;
        }
        // VirtualAlloc supplies storage, not a C++ Context lifetime. Construct
        // the trivial descriptor before Build borrows its typed address.
        ::new (static_cast<void*>(state.context)) Bridge::Context{};
        Bridge::Context context{};
        context.nativeRain = image + Bridge::NativeRainRva;
        context.saveVtable = image + Bridge::SaveVtableRva;
        context.loadVtable = image + Bridge::LoadVtableRva;
        context.saveSerialize = image + Bridge::SaveSerializeRva;
        context.loadSerialize = image + Bridge::LoadSerializeRva;
        if (!CopyIn(reinterpret_cast<std::uintptr_t>(state.context), &context, sizeof(context)) || !ContextIdentity())
        {
            state.failure = Failure::Copy;
            return false;
        }
        if (!Bridge::Build(image, state.leafEntry, state.context, state.plan))
        {
            state.failure = Failure::Build;
            return false;
        }
        if (!CopyIn(state.leafEntry, state.plan.bridge.data(), state.plan.bridge.size()))
        {
            state.failure = Failure::Copy;
            return false;
        }
        DWORD previous = 0;
        if (!VirtualProtect(allocation, page, PAGE_EXECUTE_READ, &previous) || previous != PAGE_READWRITE)
        {
            state.failure = Failure::Protection;
            return false;
        }
        if (!FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(state.leafEntry), state.plan.bridge.size()))
        {
            state.failure = Failure::Flush;
            return false;
        }
        complete = true;
        return Verify();
    }

    bool NativeWeatherRainLeaf::Verify() noexcept
    {
        state.prepared = false;
        if (!complete || !AllocationIdentity() || !PrivatePage(state.allocationBase, state.allocationBase, state.pageBytes, PAGE_EXECUTE_READ) ||
            !PrivatePage(state.allocationBase, reinterpret_cast<std::uintptr_t>(state.context), state.pageBytes, PAGE_READWRITE))
        {
            state.failure = Failure::Identity;
            return false;
        }
        Bridge::Plan                                            expected{};
        std::array<unsigned char, Bridge::Plan{}.bridge.size()> observed{};
        if (!ContextIdentity() || !Bridge::Build(image, state.leafEntry, state.context, expected) || expected.entry != state.plan.entry ||
            expected.bridge != state.plan.bridge || !CopyOut(state.leafEntry, observed.data(), observed.size()) || observed != state.plan.bridge)
        {
            state.failure = Failure::Readback;
            return false;
        }
        state.prepared = true;
        state.failure = Failure::None;
        return true;
    }

    bool NativeWeatherRainLeaf::Read(State& output) const noexcept
    {
        if (!state.allocationBase)
            return false;
        output = state;
        return true;
    }

    bool NativeWeatherRainLeaf::Release(ReleaseCheck check, void* opaque) noexcept
    {
        if (!state.allocationBase)
            return true;
        if (!check)
        {
            state.failure = Failure::Quiescence;
            return false;
        }
        if (!AllocationIdentity())
        {
            state.failure = Failure::Identity;
            return false;
        }
        LONG64 active = 0;
        if (!CopyOut(reinterpret_cast<std::uintptr_t>(state.context) + offsetof(Bridge::Context, activeCalls), &active, sizeof(active)))
        {
            state.failure = Failure::Readback;
            return false;
        }
        if (active)
        {
            state.failure = Failure::ActiveCalls;
            return false;
        }
        if (!check(opaque, state))
        {
            state.failure = Failure::Quiescence;
            return false;
        }
        // The callback confirms the owner's already-held quiescence. It must
        // remain held across this exact-base MEM_RELEASE; neither a successful
        // query nor zero activeCalls makes that native lifetime proof for us.
        if (!VirtualFree(reinterpret_cast<void*>(state.allocationBase), 0, MEM_RELEASE))
        {
            state.failure = Failure::Release;
            return false;
        }
        state = {};
        image = 0;
        complete = false;
        return true;
    }
}

/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Atomic native code windows, checked fields and owned near RW storage
 *****************************************************************************/
#include "NativeWeatherOverrideWin32.h"
#include "NativeImageAccess.h"
#include <intrin.h>
#include <limits>
#include <windows.h>

namespace mta::game_de
{
    namespace
    {
        bool ReadCode(std::uintptr_t address, void* output, std::size_t size) noexcept
        {
            if (!address || !output || !size || address > (std::numeric_limits<std::uintptr_t>::max)() - size)
                return false;
            MEMORY_BASIC_INFORMATION page{};
            if (!VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) || page.State != MEM_COMMIT || page.Type != MEM_IMAGE ||
                !NativeImageAccess::ImagePages(reinterpret_cast<std::uintptr_t>(page.AllocationBase), address, size, false, true))
                return false;
            SIZE_T read = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &read) && read == size;
        }
        bool ScalarAddress(const NativeWeatherScalarProfile::Binding& binding, NativeWeatherScalarProfile::Field field, std::uintptr_t& output) noexcept
        {
            const auto index = static_cast<std::size_t>(field);
            if (!binding.image || binding.image > (std::numeric_limits<std::uintptr_t>::max)() - NativeWeatherScalarProfile::ImageSize ||
                index >= NativeWeatherScalarProfile::Fields.size())
                return false;
            const auto address = binding.image + NativeWeatherScalarProfile::Fields[index];
            if (address % sizeof(LONG) || !NativeImageAccess::ImagePages(binding.image, address, sizeof(LONG), true, false))
                return false;
            output = address;
            return true;
        }
        NativeWeatherOverridePlatform::Exchange ExchangeWindow(std::uintptr_t address, std::size_t size, const NativeWeatherOverridePlatform::Bytes& expected,
                                                               const NativeWeatherOverridePlatform::Bytes& desired) noexcept
        {
            // The intrinsic destination is naturally aligned in both paths.
            // Faults and failed compares leave the entire old word intact;
            // no memcpy/WPM code-write fallback is permitted here.
            LONG64 expectedLow = 0, desiredLow = 0, desiredHigh = 0;
            std::memcpy(&expectedLow, expected.data(), 8);
            std::memcpy(&desiredLow, desired.data(), 8);
            std::memcpy(&desiredHigh, desired.data() + 8, 8);
            alignas(16) LONG64 comparand[2]{};
            std::memcpy(comparand, expected.data(), 16);
            __try
            {
                bool exchanged = false;
                if (size == 8)
                    exchanged = _InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address), desiredLow, expectedLow) == expectedLow;
                else
                    exchanged = _InterlockedCompareExchange128(reinterpret_cast<volatile LONG64*>(address), desiredHigh, desiredLow, comparand) != 0;
                return exchanged ? NativeWeatherOverridePlatform::Exchange::Exchanged : NativeWeatherOverridePlatform::Exchange::Conflict;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return NativeWeatherOverridePlatform::Exchange::Unavailable;
            }
        }
        NativeWeatherOverridePlatform::Exchange ExchangeField(std::uintptr_t address, std::uint32_t expected, std::uint32_t desired) noexcept
        {
            __try
            {
                return static_cast<std::uint32_t>(_InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(desired),
                                                                              static_cast<LONG>(expected))) == expected
                           ? NativeWeatherOverridePlatform::Exchange::Exchanged
                           : NativeWeatherOverridePlatform::Exchange::Conflict;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return NativeWeatherOverridePlatform::Exchange::Unavailable;
            }
        }
    }
    NativeWeatherOverridePlatform::CodeRead NativeWeatherOverrideWin32::Reader() const noexcept
    {
        return &ReadCode;
    }
    bool NativeWeatherOverrideWin32::Revalidate(const Binding& binding, const NativeWeatherOverrideCodeView& view) noexcept
    {
        return NativeWeatherScalarProfile::Revalidate(binding, view);
    }
    bool NativeWeatherOverrideWin32::CompareExchange128() const noexcept
    {
        return sizeof(void*) == 8 && IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128) != FALSE;
    }
    bool NativeWeatherOverrideWin32::ScratchValid(std::uintptr_t address) const noexcept
    {
        MEMORY_BASIC_INFORMATION page{};
        return address && address % 4 == 0 && VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) &&
               page.AllocationBase == reinterpret_cast<void*>(address) && page.State == MEM_COMMIT && page.Type == MEM_PRIVATE &&
               page.Protect == PAGE_READWRITE && page.RegionSize >= sizeof(float);
    }
    std::uintptr_t NativeWeatherOverrideWin32::AllocateNear(const NativeWeatherOverrideProfile::Range& requested) noexcept
    {
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const auto granularity = static_cast<std::uintptr_t>(system.dwAllocationGranularity);
        const auto minimum = reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
        const auto maximum = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
        auto       start = requested.minimum > minimum ? requested.minimum : minimum;
        const auto end = requested.maximum < maximum ? requested.maximum : maximum;
        if (!granularity || start > end || start > (std::numeric_limits<std::uintptr_t>::max)() - granularity)
            return 0;
        start = (start + granularity - 1) / granularity * granularity;
        // VirtualQuery skips occupied regions. Allocation happens before peer
        // suspension, so allocator locks held by a parked thread cannot stall
        // this native callback while patch transactions are in progress.
        for (std::size_t attempt = 0; start <= end && attempt < 65536; ++attempt)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (!VirtualQuery(reinterpret_cast<const void*>(start), &page, sizeof(page)))
                return 0;
            const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base)
                return 0;
            const auto next = base + page.RegionSize;
            if (page.State == MEM_FREE && next >= start && next - start >= system.dwPageSize)
            {
                const auto allocation =
                    reinterpret_cast<std::uintptr_t>(VirtualAlloc(reinterpret_cast<void*>(start), system.dwPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                if (allocation == start && ScratchValid(allocation))
                    return allocation;
                if (allocation)
                    VirtualFree(reinterpret_cast<void*>(allocation), 0, MEM_RELEASE);
                if (end - start < granularity)
                    return 0;
                start += granularity;
            }
            else
            {
                if (next <= start || next > (std::numeric_limits<std::uintptr_t>::max)() - granularity)
                    return 0;
                start = (next + granularity - 1) / granularity * granularity;
            }
        }
        return 0;
    }
    bool NativeWeatherOverrideWin32::ReleaseScratch(std::uintptr_t address) noexcept
    {
        return ScratchValid(address) && VirtualFree(reinterpret_cast<void*>(address), 0, MEM_RELEASE) != FALSE;
    }
    bool NativeWeatherOverrideWin32::OpenCode(const Binding& binding, std::uintptr_t address, std::size_t size, std::uint32_t& previous) noexcept
    {
        if ((size != 8 && size != 16) || address % size || !NativeImageAccess::ImagePages(binding.image, address, size, false, true))
            return false;
        DWORD protection = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &protection))
            return false;
        previous = protection;
        return true;
    }
    bool NativeWeatherOverrideWin32::CloseCode(const Binding& binding, std::uintptr_t address, std::size_t size, std::uint32_t previous) noexcept
    {
        if ((size != 8 && size != 16) || address % size || !NativeImageAccess::ImagePages(binding.image, address, size, false, true))
            return false;
        DWORD ignored = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, previous, &ignored))
            return false;
        MEMORY_BASIC_INFORMATION page{};
        return VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) && page.Protect == previous;
    }
    NativeWeatherOverridePlatform::Exchange NativeWeatherOverrideWin32::ExchangeCode(const Binding& binding, std::uintptr_t address, std::size_t size,
                                                                                     const Bytes& expected, const Bytes& desired) noexcept
    {
        // ImagePages' writable mode deliberately admits only nonexecutable
        // data. Code transactions instead require executable image pages plus
        // the exact temporary writable protection owned by OpenCode.
        MEMORY_BASIC_INFORMATION page{};
        if ((size != 8 && size != 16) || address % size || (size == 16 && !CompareExchange128()) ||
            !NativeImageAccess::ImagePages(binding.image, address, size, false, true) ||
            VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) != sizeof(page) ||
            (page.Protect != PAGE_EXECUTE_READWRITE && page.Protect != PAGE_EXECUTE_WRITECOPY) ||
            address - reinterpret_cast<std::uintptr_t>(page.BaseAddress) > page.RegionSize ||
            size > page.RegionSize - (address - reinterpret_cast<std::uintptr_t>(page.BaseAddress)))
            return Exchange::Unavailable;
        return ExchangeWindow(address, size, expected, desired);
    }
    bool NativeWeatherOverrideWin32::Flush(std::uintptr_t address, std::size_t size) noexcept
    {
        return FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(address), size) != FALSE;
    }
    bool NativeWeatherOverrideWin32::ReadScalar(const Binding& binding, Field field, std::uint32_t& output) noexcept
    {
        std::uintptr_t address = 0;
        std::uint32_t  value = 0;
        if (!ScalarAddress(binding, field, address) || !NativeImageAccess::ReadImage(binding.image, address, &value, sizeof(value), true, false))
            return false;
        output = value;
        return true;
    }
    NativeWeatherOverridePlatform::Exchange NativeWeatherOverrideWin32::ExchangeScalar(const Binding& binding, Field field, std::uint32_t expected,
                                                                                       std::uint32_t desired) noexcept
    {
        std::uintptr_t address = 0;
        return ScalarAddress(binding, field, address) ? ExchangeField(address, expected, desired) : Exchange::Unavailable;
    }
}

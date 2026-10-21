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
        bool SameRainAllocation(const NativeWeatherRainLeaf::State& first, const NativeWeatherRainLeaf::State& second) noexcept
        {
            return first.allocationBase == second.allocationBase && first.allocationBytes == second.allocationBytes && first.pageBytes == second.pageBytes &&
                   first.leafEntry == second.leafEntry && first.context == second.context && first.plan.entry == second.plan.entry &&
                   first.plan.bridge == second.plan.bridge;
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
    ClockResult NativeWeatherOverrideWin32::QualifyRain(const Binding& binding, const NativeWeatherOverrideCodeView& view) noexcept
    {
        if (!binding.image || (rainImage && rainImage != binding.image))
            return ClockResult::BindingChanged;
        if (rainCopyTarget)
            return RevalidateRain(binding, view, {}, false) ? ClockResult::Ok : ClockResult::BindingChanged;
        // Loader/export lookup happens before any peer parks. Under the hold
        // only the captured expected pointer and bounded image bytes are read.
        const auto runtime = GetModuleHandleW(L"VCRUNTIME140.dll");
        if (!runtime)
            return ClockResult::UnsupportedBuild;
        const auto copy = GetProcAddress(runtime, "memcpy");
        if (!copy)
            return ClockResult::UnsupportedBuild;
        rainImage = binding.image;
        rainCopyTarget = reinterpret_cast<std::uintptr_t>(copy);
        return RevalidateRain(binding, view, {}, false) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
    ClockResult NativeWeatherOverrideWin32::PrepareRain(const Binding& binding, const NativeWeatherOverrideCodeView& view,
                                                        NativeWeatherRainLeaf::State& output) noexcept
    {
        NativeWeatherRainLeaf::State existing{};
        if (rainLeaf.Read(existing))
        {
            output = existing;
            return ClockResult::Busy;
        }
        const auto qualified = QualifyRain(binding, view);
        if (qualified != ClockResult::Ok)
            return qualified;
        const bool prepared = rainLeaf.Prepare(binding.image);
        if (rainLeaf.Read(existing))
            output = existing;
        if (!prepared || !existing.prepared)
            return ClockResult::WriteFailure;
        return ClockResult::Ok;
    }
    bool NativeWeatherOverrideWin32::ReadRain(NativeWeatherRainLeaf::State& output, std::uint64_t& active, std::uint64_t& foreign) noexcept
    {
        NativeWeatherRainLeaf::State state{};
        if (!rainLeaf.Read(state) || !state.context)
            return false;
        MEMORY_BASIC_INFORMATION info{};
        const auto               address = reinterpret_cast<std::uintptr_t>(state.context);
        if (!VirtualQuery(state.context, &info, sizeof(info)) || info.State != MEM_COMMIT || info.Type != MEM_PRIVATE ||
            reinterpret_cast<std::uintptr_t>(info.AllocationBase) != state.allocationBase || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            (info.Protect & 0xff) != PAGE_READWRITE)
            return false;
        NativeWeatherRainBridge::Context context{};
        SIZE_T                           read = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), state.context, &context, sizeof(context), &read) || read != sizeof(context) ||
            address % alignof(NativeWeatherRainBridge::Context))
            return false;
        output = state;
        active = static_cast<std::uint64_t>(context.activeCalls);
        foreign = static_cast<std::uint64_t>(context.unqualifiedArchive);
        return true;
    }
    bool NativeWeatherOverrideWin32::RevalidateRain(const Binding& binding, const NativeWeatherOverrideCodeView& view,
                                                    const NativeWeatherRainLeaf::State& state, bool retained) noexcept
    {
        namespace Rain = NativeWeatherRainProfile;
        if (!rainCopyTarget || rainImage != binding.image)
            return false;
        if (state.allocationBase)
        {
            NativeWeatherRainLeaf::State actual{};
            if (!rainLeaf.Read(actual) || !SameRainAllocation(state, actual) || (state.prepared && !rainLeaf.Verify()))
                return false;
        }
        if (retained && (!state.prepared || !state.leafEntry || !state.context))
            return false;
        std::array<unsigned char, 8> entry{};
        if (!NativeImageAccess::ReadImage(binding.image, binding.image + Rain::EntryRva, entry.data(), entry.size(), false, true) ||
            (entry != NativeWeatherRainBridge::StockEntry && (!retained || entry != state.plan.entry)))
            return false;
        std::array<unsigned char, Rain::MaximumPinBytes> bytes{};
        for (const auto& pin : Rain::Code)
        {
            if (pin.size > bytes.size() ||
                !NativeImageAccess::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, pin.rva != Rain::SerializerUnwindRva) ||
                !view.Originalize(binding, pin.rva, bytes.data(), pin.size))
                return false;
            if (retained)
            {
                const auto begin = pin.rva > Rain::EntryRva ? pin.rva : Rain::EntryRva;
                const auto pinEnd = pin.rva + pin.size, entryEnd = Rain::EntryRva + entry.size();
                const auto end = pinEnd < entryEnd ? pinEnd : entryEnd;
                for (auto rva = begin; rva < end; ++rva)
                {
                    if (bytes[rva - pin.rva] != entry[rva - Rain::EntryRva])
                        return false;
                    bytes[rva - pin.rva] = NativeWeatherRainBridge::StockEntry[rva - Rain::EntryRva];
                }
            }
            if (std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        for (const auto& slot : Rain::Slots)
        {
            std::uintptr_t actual = 0;
            if (!NativeImageAccess::ReadImage(binding.image, binding.image + slot.rva, &actual, sizeof(actual)) || actual != binding.image + slot.target)
                return false;
        }
        std::uintptr_t copy = 0;
        return NativeImageAccess::ReadImage(binding.image, binding.image + Rain::CopyIatRva, &copy, sizeof(copy)) && copy == rainCopyTarget;
    }
    bool NativeWeatherOverrideWin32::ReleaseRain(const NativeWeatherRainLeaf::State& expected, NativeWeatherRainLeaf::ReleaseCheck check,
                                                 void* context) noexcept
    {
        NativeWeatherRainLeaf::State actual{};
        if (!rainLeaf.Read(actual) || !SameRainAllocation(expected, actual) || !rainLeaf.Release(check, context))
            return false;
        rainImage = rainCopyTarget = 0;
        return true;
    }
    bool NativeWeatherOverrideWin32::CompareExchange128() const noexcept
    {
        return sizeof(void*) == 8 && IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128) != FALSE;
    }
    bool NativeWeatherOverrideWin32::ScratchValid(std::uintptr_t address) const noexcept
    {
        if (!address)
            return false;
        for (const auto& slot : slots)
            if (slot.HasAllocation() && slot.Accumulator() == address)
                return slot.Valid();
        return false;
    }
    ClockResult NativeWeatherOverrideWin32::AllocateNear(const Binding& binding, Field field, const NativeWeatherOverrideProfile::Range& requested,
                                                         std::uintptr_t& output) noexcept
    {
        const auto index = static_cast<std::size_t>(field);
        if (index >= slots.size())
            return ClockResult::InvalidArgument;
        if (slots[index].HasAllocation())
            return ClockResult::Busy;
        NativeWeatherOverrideProfile::Range qualified;
        auto                                result = NativeWeatherOverrideProfile::Plan::AccumulatorRange(binding, field, qualified);
        if (result != ClockResult::Ok)
            return result;
        if (requested.minimum != qualified.minimum || requested.maximum != qualified.maximum)
            return ClockResult::InvalidArgument;
        NativeWeatherOverridePlacementProfile::Placement placement;
        result = NativeWeatherOverridePlacementProfile::Placement::Prepare(binding, field, placement);
        if (result != ClockResult::Ok)
            return result;
        result = NativeWeatherOverrideSlot::Allocate(placement, slots[index]);
        if (slots[index].HasAllocation())
            output = slots[index].Accumulator();
        return result;
    }
    bool NativeWeatherOverrideWin32::ReleaseScratch(std::uintptr_t address) noexcept
    {
        if (!address)
            return false;
        for (auto& slot : slots)
            if (slot.HasAllocation() && slot.Accumulator() == address)
                return slot.Release();
        return false;
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

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check native pool generation and root/header bookends before output
 ****************************************************************************/
#include "NativePedPoolReader.h"
#include "NativeImageAccess.h"
#include <game/CPoolSlotPolicy.h>
#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace P = NativePedPoolProfile;
        namespace Image = NativeImageAccess;
        bool Add(std::uintptr_t base, std::size_t offset, std::uintptr_t& output) noexcept
        {
            if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
                return false;
            output = base + offset;
            return true;
        }
        bool NativePages(std::uintptr_t at, std::size_t size) noexcept
        {
            if (!at || !size || size > (std::numeric_limits<std::uintptr_t>::max)() - at)
                return false;
            const auto     end = at + size;
            std::uintptr_t allocation = 0;
            while (at < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(at), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ||
                    (info.Protect & 0xff) != PAGE_READWRITE || !info.RegionSize)
                    return false;
                const auto start = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                const auto identity = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                if (!identity || (allocation && allocation != identity) || start > at ||
                    info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - start || start + info.RegionSize <= at)
                    return false;
                allocation = identity;
                at = (start + info.RegionSize < end) ? start + info.RegionSize : end;
            }
            return true;
        }
        template <typename T>
        bool NativeRead(std::uintptr_t at, T& output) noexcept
        {
            T      next{};
            SIZE_T copied = 0;
            if (!NativePages(at, sizeof(T)) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        bool Same(const NativePedPoolReader::Value& first, const NativePedPoolReader::Value& second) noexcept
        {
            return first.handle == second.handle && first.slot == second.slot && first.pool == second.pool && first.objects == second.objects &&
                   first.flags == second.flags && first.nativePed == second.nativePed && first.vtable == second.vtable &&
                   first.firstMethod == second.firstMethod && first.capacity == second.capacity && first.cursor == second.cursor &&
                   first.generation == second.generation && first.ownsAllocations == second.ownsAllocations;
        }
    }
    bool NativePedPoolReader::Qualify(Binding& output) noexcept
    {
        if (sizeof(void*) != 8 || !Image::CheckExecutableBuild())
            return false;
        Binding next{reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))};
        if (!Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedPoolReader::Revalidate(const Binding& binding) noexcept
    {
        const auto image = binding.image;
        if (!image || image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) ||
            image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageSize)
            return false;
        IMAGE_DOS_HEADER dos{};
        if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > P::ImageSize - sizeof(IMAGE_NT_HEADERS64))
            return false;
        IMAGE_NT_HEADERS64 nt{};
        const auto         ntRva = static_cast<std::uintptr_t>(dos.e_lfanew);
        if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
            nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt.OptionalHeader.SizeOfImage != P::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
            !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto                           sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (sectionsRva > P::ImageSize || bytes > P::ImageSize - sectionsRva || !Image::ReadImage(image, image + sectionsRva, sections.data(), bytes))
            return false;
        const auto contains = [&](std::uintptr_t at, std::size_t size, DWORD required, DWORD forbidden)
        {
            for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                if (Image::SectionContains(sections[index], at, size, required, forbidden))
                    return true;
            return false;
        };
        if (!contains(P::RootRva, 8, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) || !Image::ImagePages(image, image + P::RootRva, 8, true))
            return false;
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativePedPoolReader::Capture(std::uint32_t handle, Value& output) const noexcept
    {
        Value next;
        next.handle = handle;
        next.slot = handle >> 8;
        if (!Image::ReadImage(binding.image, binding.image + P::RootRva, &next.pool, sizeof(next.pool), true))
            return ClockResult::ReadFailure;
        if (!next.pool)
            return ClockResult::NotInitialized;
        if (next.pool % alignof(std::uintptr_t) || !NativePages(next.pool, P::OwnsOffset + 1) || !NativeRead(next.pool + P::ObjectsOffset, next.objects) ||
            !NativeRead(next.pool + P::FlagsOffset, next.flags) || !NativeRead(next.pool + P::CapacityOffset, next.capacity) ||
            !NativeRead(next.pool + P::CursorOffset, next.cursor) || !NativeRead(next.pool + P::OwnsOffset, next.ownsAllocations))
            return ClockResult::ReadFailure;
        // The proved shipped pool allocates1000 complete derived-ped slots.
        // Shutdown frees these banks before clearing the global, so a nonnull
        // root alone cannot authorize even a handle1 observation.
        if (!next.objects || !next.flags || next.capacity != static_cast<std::int32_t>(P::NativeCapacity) || next.ownsAllocations != 1 || next.cursor < -1 ||
            next.cursor >= next.capacity || next.objects % alignof(std::uintptr_t))
            return ClockResult::BindingChanged;
        if (!MtaGame::CPoolSlotPolicy::ContainsIndex(next.capacity, next.slot))
            return ClockResult::InvalidArgument;
        if (!NativePages(next.objects, P::NativeCapacity * P::ObjectStride) || !NativePages(next.flags, P::NativeCapacity))
            return ClockResult::ReadFailure;
        std::uintptr_t flag = 0;
        if (!Add(next.flags, next.slot, flag) || !NativeRead(flag, next.generation))
            return ClockResult::ReadFailure;
        // The native comparison's low byte includes the empty bit. Valid
        // handles never contain that bit; conservatively reject a crafted
        // free-slot byte even if it happens to compare equal.
        if ((next.generation & P::EmptyBit) || next.generation != static_cast<unsigned char>(handle))
            return ClockResult::NotInitialized;
        if (!Add(next.objects, static_cast<std::size_t>(next.slot) * P::ObjectStride, next.nativePed) || !NativeRead(next.nativePed, next.vtable) ||
            next.vtable % alignof(std::uintptr_t) || !Image::ImagePages(binding.image, next.vtable, sizeof(std::uintptr_t)))
            return ClockResult::ReadFailure;
        MEMORY_BASIC_INFORMATION tablePage{};
        if (VirtualQuery(reinterpret_cast<void*>(next.vtable), &tablePage, sizeof(tablePage)) != sizeof(tablePage) || tablePage.Protect != PAGE_READONLY ||
            !Image::ReadImage(binding.image, next.vtable, &next.firstMethod, sizeof(next.firstMethod)) ||
            !Image::ImagePages(binding.image, next.firstMethod, 1, false, true))
            return ClockResult::BindingChanged;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult NativePedPoolReader::Resolve(std::uint32_t handle, Value& output) const noexcept
    {
        if (!owner)
            return ClockResult::NotInitialized;
        auto checked = owner();
        if (checked != ClockResult::Ok)
            return checked;
        // Native script handles are signed even though this API receives
        // their bits as uint32_t. Reject negative handles before slot math.
        if (handle > static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)()))
            return ClockResult::InvalidArgument;
        if (!Revalidate(binding))
            return ClockResult::UnsupportedBuild;
        Value first, second;
        checked = Capture(handle, first);
        if (checked != ClockResult::Ok)
            return checked;
        checked = owner();
        if (checked != ClockResult::Ok)
            return checked;
        checked = Capture(handle, second);
        if (checked != ClockResult::Ok)
            return checked;
        if (!Same(first, second) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        checked = owner();
        if (checked != ClockResult::Ok)
            return checked;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativePedPoolReader::Recheck(const Value& previous) const noexcept
    {
        Value      next;
        const auto result = Resolve(previous.handle, next);
        return result != ClockResult::Ok ? result : Same(previous, next) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}

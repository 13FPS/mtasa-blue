/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow exact task-bank identities without inventing task ownership
 ****************************************************************************/
#include "NativeTaskPoolReader.h"
#include "NativeTaskPoolProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace mta::game_de
{
    namespace TaskPoolDetail
    {
        namespace P = NativeTaskPoolProfile;
        namespace Image = NativeImageAccess;
        using Reader = NativeTaskPoolReader;
        bool Pages(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + bytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (page.Protect & 0xff) != PAGE_READWRITE || !page.RegionSize)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                const auto identity = reinterpret_cast<std::uintptr_t>(page.AllocationBase);
                if (!identity || (allocation && identity != allocation) || base > address ||
                    page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + page.RegionSize <= address)
                    return false;
                allocation = identity;
                address = base + page.RegionSize < end ? base + page.RegionSize : end;
            }
            return true;
        }
        template <class T>
        bool Read(std::uintptr_t address, T& output) noexcept
        {
            T      next{};
            SIZE_T copied = 0;
            if (!Pages(address, sizeof(next)) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        bool Same(const Reader::Value& a, const Reader::Value& b) noexcept
        {
            return a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativeTask == b.nativeTask && a.vtable == b.vtable &&
                   a.deletingDestructor == b.deletingDestructor && a.getTypeMethod == b.getTypeMethod && a.isSimpleMethod == b.isSimpleMethod &&
                   a.slot == b.slot && a.nativeType == b.nativeType && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations && a.simple == b.simple;
        }
        bool ImageSections(std::uintptr_t image, std::array<IMAGE_SECTION_HEADER, 96>& sections, std::size_t& count) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 ||
                !Image::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || !nt.FileHeader.NumberOfSections ||
                nt.FileHeader.NumberOfSections > sections.size())
                return false;
            count = nt.FileHeader.NumberOfSections;
            const auto rva = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            return Image::ReadImage(image, image + rva, sections.data(), count * sizeof(IMAGE_SECTION_HEADER));
        }
        bool InSection(const std::array<IMAGE_SECTION_HEADER, 96>& sections, std::size_t count, std::uintptr_t rva, std::size_t bytes, DWORD required,
                       DWORD forbidden) noexcept
        {
            for (std::size_t i = 0; i < count; ++i)
                if (Image::SectionContains(sections[i], rva, bytes, required, forbidden))
                    return true;
            return false;
        }
        bool ReadOnlyImage(std::uintptr_t image, std::uintptr_t address, std::size_t bytes) noexcept
        {
            MEMORY_BASIC_INFORMATION page{};
            return Image::ImagePages(image, address, bytes) && VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) == sizeof(page) &&
                   page.Protect == PAGE_READONLY;
        }
    }
    bool NativeTaskPoolReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativeTaskPoolReader::Revalidate(const Binding& binding) noexcept
    {
        namespace D = TaskPoolDetail;
        namespace P = NativeTaskPoolProfile;
        if (!NativePedPoolReader::Revalidate(binding))
            return false;
        const auto                           image = binding.image;
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        std::size_t                          count = 0;
        if (!D::ImageSections(image, sections, count) ||
            !D::InSection(sections, count, P::RootRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
            !D::Image::ImagePages(image, image + P::RootRva, sizeof(std::uintptr_t), true))
            return false;
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() ||
                !D::InSection(sections, count, pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !D::Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        for (const auto& slot : P::Slots)
        {
            if (!D::InSection(sections, count, slot.vtable, P::TypeSlot + sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ,
                              IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                !D::ReadOnlyImage(image, image + slot.vtable, P::TypeSlot + sizeof(std::uintptr_t)))
                return false;
            for (const auto& method : std::array<std::pair<std::size_t, std::uintptr_t>, 3>{
                     {{0, slot.deletingDestructor}, {P::SimpleSlot, slot.isSimple}, {P::TypeSlot, slot.getType}}})
            {
                std::uintptr_t actual = 0;
                if (!D::Image::ReadImage(image, image + slot.vtable + method.first, &actual, sizeof(actual)) || actual != image + method.second)
                    return false;
            }
        }
        return true;
    }
    ClockResult NativeTaskPoolReader::Owner() const noexcept
    {
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if ((result = NativeStartGameCalls::CheckThread(binding.image)) != ClockResult::Ok)
            return result;
        std::uint32_t state = 0;
        if (!NativeGameStateMemory::Read(binding.image, state))
            return ClockResult::BindingChanged;
        if (state != 9)
            return ClockResult::NotInitialized;
        return owner();
    }
    ClockResult NativeTaskPoolReader::Capture(std::uintptr_t nativeTask, Value& output) const noexcept
    {
        namespace D = TaskPoolDetail;
        namespace P = NativeTaskPoolProfile;
        Value next;
        next.nativeTask = nativeTask;
        if (!D::Image::ReadImage(binding.image, binding.image + P::RootRva, &next.pool, sizeof(next.pool), true))
            return ClockResult::ReadFailure;
        if (!next.pool)
            return ClockResult::NotInitialized;
        if (next.pool % alignof(std::uintptr_t) || !D::Pages(next.pool, P::HeaderBytes) || !D::Read(next.pool + P::ObjectsOffset, next.objects) ||
            !D::Read(next.pool + P::FlagsOffset, next.flags) || !D::Read(next.pool + P::CapacityOffset, next.capacity) ||
            !D::Read(next.pool + P::CursorOffset, next.cursor) || !D::Read(next.pool + P::OwnsOffset, next.ownsAllocations))
            return ClockResult::ReadFailure;
        // Shutdown frees banks/header before clearing the global. Validate
        // both complete allocations and this exact occupied row every time.
        if (!next.objects || next.objects % alignof(std::uintptr_t) || !next.flags || next.capacity != static_cast<std::int32_t>(P::NativeCapacity) ||
            next.ownsAllocations != 1 || next.cursor < -1 || next.cursor >= next.capacity)
            return ClockResult::BindingChanged;
        const auto bytes = P::NativeCapacity * P::ObjectStride;
        if (!D::Pages(next.objects, bytes) || !D::Pages(next.flags, P::NativeCapacity))
            return ClockResult::ReadFailure;
        if (nativeTask < next.objects || nativeTask - next.objects >= bytes || (nativeTask - next.objects) % P::ObjectStride)
            return ClockResult::InvalidArgument;
        next.slot = static_cast<std::uint32_t>((nativeTask - next.objects) / P::ObjectStride);
        if (!D::Read(next.flags + next.slot, next.generation))
            return ClockResult::ReadFailure;
        if (next.generation & P::EmptyBit)
            return ClockResult::NotInitialized;
        if (!D::Read(nativeTask, next.vtable))
            return ClockResult::ReadFailure;
        bool found = false;
        for (const auto& slot : P::Slots)
            if (next.vtable == binding.image + slot.vtable)
            {
                next.deletingDestructor = binding.image + slot.deletingDestructor;
                next.getTypeMethod = binding.image + slot.getType;
                next.isSimpleMethod = binding.image + slot.isSimple;
                // These exact qualified native methods return constants. Reading
                // that proved identity avoids an unnecessary virtual native call;
                // no value is inferred for any other task vtable or type.
                next.nativeType = slot.nativeType;
                next.simple = slot.simple;
                found = true;
                break;
            }
        if (!found)
            return ClockResult::UnsupportedBuild;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult NativeTaskPoolReader::Read(std::uintptr_t nativeTask, Value& output) const noexcept
    {
        auto result = Owner();
        if (result != ClockResult::Ok)
            return result;
        if (!nativeTask)
            return ClockResult::NotInitialized;
        if (!Revalidate(binding))
            return ClockResult::UnsupportedBuild;
        Value first, second;
        if ((result = Capture(nativeTask, first)) != ClockResult::Ok || (result = Owner()) != ClockResult::Ok ||
            (result = Capture(nativeTask, second)) != ClockResult::Ok)
            return result;
        if (!TaskPoolDetail::Same(first, second) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = Owner()) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativeTaskPoolReader::Recheck(const Value& previous) const noexcept
    {
        Value      current;
        const auto result = Read(previous.nativeTask, current);
        return result != ClockResult::Ok ? result : TaskPoolDetail::Same(previous, current) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}

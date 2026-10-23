/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the exact MTA primary water task without exporting its lifetime
 ****************************************************************************/
#include "NativePedWaterTaskReader.h"
#include "NativePedWaterTaskProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativePedIntelligenceReader.h"
#include "NativeTaskPoolReader.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <tuple>
namespace mta::game_de
{
    namespace PedWaterTaskDetail
    {
        namespace P = NativePedWaterTaskProfile;
        namespace Image = NativeImageAccess;
        struct Observation
        {
            NativePedMetadata                  metadata;
            NativePedIntelligenceReader::Value intelligence;
            NativeTaskPoolReader::Value        task;
            bool                               hasTask = false;
        };
        bool Same(const Observation& a, const Observation& b) noexcept
        {
            const auto& i = a.intelligence;
            const auto& j = b.intelligence;
            const auto& t = a.task;
            const auto& u = b.task;
            return a.hasTask == b.hasTask && a.metadata.modelIndex == b.metadata.modelIndex && a.metadata.entityType == b.metadata.entityType &&
                   std::tie(i.pool, i.objects, i.flags, i.nativeIntelligence, i.taskManager, i.primaryTasks, i.secondaryTasks, i.slot, i.capacity, i.cursor,
                            i.generation, i.ownsAllocations, i.rawType) == std::tie(j.pool, j.objects, j.flags, j.nativeIntelligence, j.taskManager,
                                                                                    j.primaryTasks, j.secondaryTasks, j.slot, j.capacity, j.cursor,
                                                                                    j.generation, j.ownsAllocations, j.rawType) &&
                   std::tie(t.pool, t.objects, t.flags, t.nativeTask, t.vtable, t.deletingDestructor, t.getTypeMethod, t.isSimpleMethod, t.slot, t.nativeType,
                            t.capacity, t.cursor, t.generation, t.ownsAllocations,
                            t.simple) == std::tie(u.pool, u.objects, u.flags, u.nativeTask, u.vtable, u.deletingDestructor, u.getTypeMethod, u.isSimpleMethod,
                                                  u.slot, u.nativeType, u.capacity, u.cursor, u.generation, u.ownsAllocations, u.simple);
        }
        ClockResult Capture(NativePedWaterTaskReader::Binding binding, NativePedWaterTaskReader::CheckOwner owner, const NativePedWaterTaskReader::Ped& ped,
                            Observation& output) noexcept
        {
            NativePedMetadataMemory     metadata(binding, owner);
            NativePedIntelligenceReader intelligence(binding, owner);
            NativeTaskPoolReader        tasks(binding, owner);
            Observation                 next;
            auto                        result = metadata.Read(ped, next.metadata);
            if (result != ClockResult::Ok || (result = intelligence.Read(ped, next.intelligence)) != ClockResult::Ok)
                return result;
            const auto selected = next.intelligence.primaryTasks[P::PrimaryIndex];
            next.hasTask = selected != 0;
            if (selected && (result = tasks.Read(selected, next.task)) != ClockResult::Ok)
                return result;
            // Native deletion precedes cell/flag clearing and generations can
            // wrap. Re-observe the owning manager after the immediate task
            // read; neither an unchanged cell nor this snapshot is a lease.
            if ((result = intelligence.Recheck(next.intelligence)) != ClockResult::Ok)
                return result;
            NativePedMetadata after;
            if ((result = metadata.Read(ped, after)) != ClockResult::Ok)
                return result;
            if (after.modelIndex != next.metadata.modelIndex || after.entityType != next.metadata.entityType)
                return ClockResult::OwnerConflict;
            // A destructor can replace the task vtable while its manager cell
            // and generation still name the old row. Check that immediate
            // task identity again after reading its owning ped/intelligence.
            if (next.hasTask && (result = tasks.Recheck(next.task)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
    }
    bool NativePedWaterTaskReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedWaterTaskReader::Revalidate(const Binding& binding) noexcept
    {
        namespace D = PedWaterTaskDetail;
        namespace P = NativePedWaterTaskProfile;
        if (!NativePedMetadataMemory::Revalidate(binding) || !NativePedIntelligenceReader::Revalidate(binding) || !NativeTaskPoolReader::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!D::Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !D::Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
            !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto rva = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto size = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (rva > NativePedPoolProfile::ImageSize || size > NativePedPoolProfile::ImageSize - rva ||
            !D::Image::ReadImage(binding.image, binding.image + rva, sections.data(), size))
            return false;
        // Keep the event-to-type relationship exact as well as its constant
        // leaf: another event table must not inherit the water proof merely
        // because that leaf remains present somewhere in the executable.
        bool eventTableContained = false;
        for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
            eventTableContained |= D::Image::SectionContains(sections[index], P::EventVtableRva, P::EventTypeSlot + sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ,
                                                             IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE);
        MEMORY_BASIC_INFORMATION tablePage{};
        const auto               eventTable = binding.image + P::EventVtableRva;
        std::uintptr_t           eventType = 0;
        if (!eventTableContained || VirtualQuery(reinterpret_cast<void*>(eventTable), &tablePage, sizeof(tablePage)) != sizeof(tablePage) ||
            tablePage.Protect != PAGE_READONLY || !D::Image::ReadImage(binding.image, eventTable + P::EventTypeSlot, &eventType, sizeof(eventType)) ||
            eventType != binding.image + P::EventTypeRva)
            return false;
        for (const auto& pin : P::Code)
        {
            bool contained = false;
            for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                contained |= D::Image::SectionContains(sections[index], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE);
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!contained || !pin.size || pin.size > actual.size() ||
                !D::Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativePedWaterTaskReader::Read(const Ped& ped, bool& output) const noexcept
    {
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::UnsupportedBuild;
        PedWaterTaskDetail::Observation first, second;
        if ((result = PedWaterTaskDetail::Capture(binding, owner, ped, first)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok ||
            (result = PedWaterTaskDetail::Capture(binding, owner, ped, second)) != ClockResult::Ok)
            return result;
        if (!PedWaterTaskDetail::Same(first, second) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = owner()) != ClockResult::Ok)
            return result;
        // This is the actual SDK predicate, not script04AD, active task254,
        // physical water flags, a child-task scan or a current-player fallback.
        // Unsupported task vtables above never become a fabricated false.
        output = second.hasTask && second.task.nativeType == NativePedWaterTaskProfile::InWaterType;
        return ClockResult::Ok;
    }
}

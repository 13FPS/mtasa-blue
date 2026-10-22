/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe real intelligence/manager cells without task lifetime claims
 ****************************************************************************/
#include "NativePedIntelligenceReader.h"
#include "NativePedIntelligenceProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace PedIntelligenceDetail
    {
        namespace P = NativePedIntelligenceProfile;
        namespace Image = NativeImageAccess;
        using Memory = NativePedIntelligenceReader;
        using PedValue = NativePedPoolReader::Value;
        bool SamePed(const PedValue& a, const PedValue& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        bool Pages(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + bytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (info.Protect & 0xff) != PAGE_READWRITE)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                const auto current = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                if (!current || (allocation && allocation != current) || base > address ||
                    info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + info.RegionSize <= address)
                    return false;
                allocation = current;
                address = base + info.RegionSize < end ? base + info.RegionSize : end;
            }
            return true;
        }
        template <class T>
        bool Object(std::uintptr_t address, T& output) noexcept
        {
            T      next{};
            SIZE_T copied = 0;
            if (!Pages(address, sizeof(next)) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        ClockResult Owner(Memory::Binding binding, Memory::CheckOwner owner) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
            if (result != ClockResult::Ok)
                return result;
            result = NativeStartGameCalls::CheckThread(binding.image);
            if (result != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            if (state != 9)
                return ClockResult::NotInitialized;
            return owner();
        }
        ClockResult Identity(Memory::Binding binding, Memory::CheckOwner owner, const PedValue& expected) noexcept
        {
            auto result = Owner(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            // Both native classes own their intelligence through the common
            // ped constructor/destructor. A loaded local-player pointer is
            // neither a substitute for this exact ped nor a lifetime grant.
            if (expected.vtable != binding.image + P::PedVtableRva && expected.vtable != binding.image + P::PlayerVtableRva)
                return ClockResult::UnsupportedBuild;
            NativePedPoolReader pool(binding, owner);
            PedValue            current;
            if ((result = pool.Resolve(expected.handle, current)) != ClockResult::Ok)
                return result;
            if (!SamePed(expected, current))
                return ClockResult::BindingChanged;
            if ((result = pool.Recheck(current)) != ClockResult::Ok)
                return result;
            return Owner(binding, owner);
        }
        struct Header
        {
            std::uintptr_t pool = 0, objects = 0, flags = 0;
            std::int32_t   capacity = 0, cursor = -1;
            unsigned char  owns = 0;
        };
        bool SameHeader(const Header& a, const Header& b) noexcept
        {
            return a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.capacity == b.capacity && a.cursor == b.cursor && a.owns == b.owns;
        }
        ClockResult ReadHeader(Memory::Binding binding, Header& output) noexcept
        {
            Header next;
            if (!Image::ReadImage(binding.image, binding.image + P::RootRva, &next.pool, sizeof(next.pool), true))
                return ClockResult::ReadFailure;
            if (!next.pool)
                return ClockResult::NotInitialized;
            if (next.pool % alignof(std::uintptr_t) || !Object(next.pool + P::ObjectsOffset, next.objects) || !Object(next.pool + P::FlagsOffset, next.flags) ||
                !Object(next.pool + P::CapacityOffset, next.capacity) || !Object(next.pool + P::CursorOffset, next.cursor) ||
                !Object(next.pool + P::OwnsOffset, next.owns))
                return ClockResult::ReadFailure;
            if (next.capacity != P::NativeCapacity || next.cursor < -1 || next.cursor >= next.capacity || next.owns != 1 ||
                next.objects % alignof(std::uintptr_t))
                return ClockResult::BindingChanged;
            if (!Pages(next.objects, static_cast<std::size_t>(P::NativeCapacity) * P::ObjectStride) ||
                !Pages(next.flags, static_cast<std::size_t>(P::NativeCapacity)))
                return ClockResult::ReadFailure;
            output = next;
            return ClockResult::Ok;
        }
        bool SameValue(const Memory::Value& a, const Memory::Value& b) noexcept
        {
            return SamePed(a.ped, b.ped) && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativeIntelligence == b.nativeIntelligence &&
                   a.taskManager == b.taskManager && a.primaryTasks == b.primaryTasks && a.secondaryTasks == b.secondaryTasks && a.slot == b.slot &&
                   a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation && a.ownsAllocations == b.ownsAllocations &&
                   a.rawType == b.rawType;
        }
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const PedValue& expected, Memory::Value& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            Header first, second;
            if ((result = ReadHeader(binding, first)) != ClockResult::Ok)
                return result;
            Memory::Value next;
            next.ped = expected;
            if (!Object(expected.nativePed + P::TypeOffset, next.rawType) || !Object(expected.nativePed + P::IntelligenceOffset, next.nativeIntelligence))
                return ClockResult::ReadFailure;
            if ((next.rawType & 7) != 3)
                return ClockResult::BindingChanged;
            if (!next.nativeIntelligence)
                return ClockResult::NotInitialized;
            if (next.nativeIntelligence < first.objects || (next.nativeIntelligence - first.objects) % P::ObjectStride)
                return ClockResult::BindingChanged;
            const auto slot = (next.nativeIntelligence - first.objects) / P::ObjectStride;
            if (slot >= static_cast<std::size_t>(P::NativeCapacity))
                return ClockResult::BindingChanged;
            next.slot = static_cast<std::uint32_t>(slot);
            unsigned char flag = 0;
            if (!Object(first.flags + next.slot, flag))
                return ClockResult::ReadFailure;
            if (flag & P::EmptyBit)
                return ClockResult::BindingChanged;
            // The prefix is the real owner word followed by the embedded
            // manager's 5+6 task cells and its owner word. Copying addresses
            // does not inspect task objects or confer their native lifetime.
            std::array<std::uintptr_t, 13> prefix{};
            static_assert(sizeof(prefix) == P::PrefixBytes);
            if (!Object(next.nativeIntelligence, prefix))
                return ClockResult::ReadFailure;
            if (prefix[0] != expected.nativePed || prefix[12] != expected.nativePed)
                return ClockResult::BindingChanged;
            for (std::size_t i = 0; i != next.primaryTasks.size(); ++i)
                next.primaryTasks[i] = prefix[i + 1];
            for (std::size_t i = 0; i != next.secondaryTasks.size(); ++i)
                next.secondaryTasks[i] = prefix[i + 6];
            std::uintptr_t pointerAfter = 0;
            unsigned char  typeAfter = 0, flagAfter = 0;
            if (!Object(expected.nativePed + P::IntelligenceOffset, pointerAfter) || !Object(expected.nativePed + P::TypeOffset, typeAfter) ||
                !Object(first.flags + next.slot, flagAfter))
                return ClockResult::ReadFailure;
            if ((result = ReadHeader(binding, second)) != ClockResult::Ok)
                return result;
            if (!SameHeader(first, second) || pointerAfter != next.nativeIntelligence || typeAfter != next.rawType || flag != flagAfter)
                return ClockResult::BindingChanged;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            next.pool = first.pool;
            next.objects = first.objects;
            next.flags = first.flags;
            next.taskManager = next.nativeIntelligence + P::ManagerOffset;
            next.capacity = first.capacity;
            next.cursor = first.cursor;
            next.ownsAllocations = first.owns;
            next.generation = static_cast<unsigned char>(flag & P::GenerationMask);
            output = next;
            return ClockResult::Ok;
        }

    }

    bool NativePedIntelligenceReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedIntelligenceReader::Revalidate(const Binding& binding) noexcept
    {
        namespace Image = NativeImageAccess;
        if (!NativePedPoolReader::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !Image::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        bool rootContained = false;
        for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
            if (Image::SectionContains(sections[i], NativePedIntelligenceProfile::RootRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE,
                                       IMAGE_SCN_MEM_EXECUTE))
                rootContained = true;
        if (!rootContained || !Image::ImagePages(binding.image, binding.image + NativePedIntelligenceProfile::RootRva, sizeof(std::uintptr_t), true))
            return false;
        for (const auto& pin : NativePedIntelligenceProfile::Code)
        {
            bool contained = false;
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contained = true;
            std::array<unsigned char, NativePedIntelligenceProfile::MaximumPinBytes> actual{};
            if (!contained || !pin.size || pin.size > actual.size() ||
                !Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativePedIntelligenceReader::Read(const PedValue& expected, Value& output) const noexcept
    {
        namespace D = PedIntelligenceDetail;
        Value first, second;
        auto  result = D::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if (!D::SameValue(first, second))
            return ClockResult::OwnerConflict;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativePedIntelligenceReader::Recheck(const Value& expected) const noexcept
    {
        Value current;
        auto  result = Read(expected.ped, current);
        if (result != ClockResult::Ok)
            return result;
        return PedIntelligenceDetail::SameValue(expected, current) ? ClockResult::Ok : ClockResult::OwnerConflict;
    }
}

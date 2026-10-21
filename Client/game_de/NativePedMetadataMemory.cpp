/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe real ped model/type bits without inventing model validity
 ****************************************************************************/
#include "NativePedMetadataMemory.h"
#include "NativePedMetadataProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace PedMetadataDetail
    {
        namespace P = NativePedMetadataProfile;
        namespace Image = NativeImageAccess;
        using Memory = NativePedMetadataMemory;
        using Value = NativePedPoolReader::Value;
        bool SamePed(const Value& a, const Value& b) noexcept
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
        ClockResult Identity(Memory::Binding binding, Memory::CheckOwner owner, const Value& expected) noexcept
        {
            auto result = Owner(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            // The qualified common entity/ped constructor chain proves the
            // same metadata on both exact known classes. Neither route needs
            // a CURRENT-player substitution or a model-bank dereference.
            if (expected.vtable != binding.image + P::PedVtableRva && expected.vtable != binding.image + P::PlayerVtableRva)
                return ClockResult::UnsupportedBuild;
            NativePedPoolReader pool(binding, owner);
            Value               current;
            if ((result = pool.Resolve(expected.handle, current)) != ClockResult::Ok)
                return result;
            if (!SamePed(expected, current))
                return ClockResult::BindingChanged;
            if ((result = pool.Recheck(current)) != ClockResult::Ok)
                return result;
            return Owner(binding, owner);
        }
        struct Snapshot
        {
            std::uint16_t modelIndex = 0;
            std::uint8_t  rawType = 0;
        };
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Value& expected, Snapshot& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            Snapshot     next;
            std::uint8_t typeAfter = 0;
            if (!Object(expected.nativePed + P::TypeOffset, next.rawType) || !Object(expected.nativePed + P::ModelOffset, next.modelIndex) ||
                !Object(expected.nativePed + P::TypeOffset, typeAfter))
                return ClockResult::ReadFailure;
            if ((next.rawType & P::TypeMask) != P::PedType || next.rawType != typeAfter)
                return ClockResult::BindingChanged;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
    }

    bool NativePedMetadataMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedMetadataMemory::Revalidate(const Binding& binding) noexcept
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
        for (const auto& pin : NativePedMetadataProfile::Code)
        {
            bool contained = false;
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contained = true;
            std::array<unsigned char, NativePedMetadataProfile::MaximumPinBytes> actual{};
            if (!contained || !pin.size || pin.size > actual.size() ||
                !Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativePedMetadataMemory::Read(const Value& expected, NativePedMetadata& output) const noexcept
    {
        namespace D = PedMetadataDetail;
        D::Snapshot first, second;
        auto        result = D::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if (first.modelIndex != second.modelIndex || first.rawType != second.rawType)
            return ClockResult::OwnerConflict;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // Classic GetModelIndex returns the actual WORD, including ffff. It
        // does not make that value a safe model-bank index. Type is the actual
        // observed low3-bit field; upper status bits never become SDK type.
        NativePedMetadata next;
        next.modelIndex = second.modelIndex;
        next.entityType = static_cast<std::uint8_t>(second.rawType & NativePedMetadataProfile::TypeMask);
        output = next;
        return ClockResult::Ok;
    }
}

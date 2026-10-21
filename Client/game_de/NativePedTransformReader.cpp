/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bookend native pool identity, matrix alternatives and real vectors
 ****************************************************************************/
#include "NativePedTransformReader.h"
#include "NativePedTransformProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace PedTransformDetail
    {
        namespace P = NativePedTransformProfile;
        namespace Image = NativeImageAccess;
        bool NativePages(std::uintptr_t at, std::size_t bytes) noexcept
        {
            if (!at || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - at)
                return false;
            const auto     end = at + bytes;
            std::uintptr_t allocation = 0;
            while (at < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(at), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (info.Protect & 0xff) != PAGE_READWRITE)
                    return false;
                const auto start = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                const auto currentAllocation = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                if (!currentAllocation || (allocation && allocation != currentAllocation) || start > at ||
                    info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - start || start + info.RegionSize <= at)
                    return false;
                allocation = currentAllocation;
                at = start + info.RegionSize < end ? start + info.RegionSize : end;
            }
            return true;
        }
        template <class T>
        bool Read(std::uintptr_t address, T& output) noexcept
        {
            T      next{};
            SIZE_T copied = 0;
            if (!NativePages(address, sizeof(next)) ||
                !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &next, sizeof(next), &copied) || copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        bool SamePool(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        bool Same(const NativePedTransformReader::Value& a, const NativePedTransformReader::Value& b) noexcept
        {
            if (!SamePool(a.ped, b.ped) || a.matrix != b.matrix || a.positionAddress != b.positionAddress ||
                std::memcmp(&a.position, &b.position, sizeof(CVector)) || std::memcmp(&a.velocity, &b.velocity, sizeof(CVector)))
                return false;
            for (std::size_t i = 0; i != a.matrixRows.size(); ++i)
                if (std::memcmp(&a.matrixRows[i], &b.matrixRows[i], sizeof(CVector)))
                    return false;
            return true;
        }
    }
    bool NativePedTransformReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedTransformReader::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedTransformProfile;
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
        const auto sectionRva = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (nt.FileHeader.NumberOfSections > sections.size() || sectionRva > NativePedPoolProfile::ImageSize ||
            bytes > NativePedPoolProfile::ImageSize - sectionRva || !Image::ReadImage(binding.image, binding.image + sectionRva, sections.data(), bytes))
            return false;
        const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
        {
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], rva, size, required, forbidden))
                    return true;
            return false;
        };
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        for (const auto table : {P::PedVtableRva, P::PlayerVtableRva})
        {
            std::uintptr_t entry = 0;
            if (!contains(table + P::TeleportSlotOffset, 8, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ReadImage(binding.image, binding.image + table + P::TeleportSlotOffset, &entry, sizeof(entry)) ||
                entry != binding.image + P::NativeTeleportRva)
                return false;
        }
        const auto scale = [&](std::uintptr_t rva, std::uint32_t expected)
        {
            std::uint32_t actual = 0;
            return contains(rva, 4, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) &&
                   Image::ReadImage(binding.image, binding.image + rva, &actual, sizeof(actual)) && actual == expected;
        };
        return scale(P::ScriptVelocityInScaleRva, P::ScriptVelocityInScaleBits) && scale(P::ScriptVelocityOutScaleRva, P::ScriptVelocityOutScaleBits);
    }
    ClockResult NativePedTransformReader::Capture(std::uint32_t handle, Value& output) const noexcept
    {
        namespace D = PedTransformDetail;
        namespace P = NativePedTransformProfile;
        NativePedPoolReader pool(binding, owner);
        Value               next;
        auto                result = pool.Resolve(handle, next.ped);
        if (result != ClockResult::Ok)
            return result;
        // Only independently identified native ped implementations are admitted.
        // Pool occupancy by itself does not turn a partly constructed slot into
        // a complete entity or establish an arbitrary derived-vtable contract.
        if (next.ped.vtable != binding.image + P::PedVtableRva && next.ped.vtable != binding.image + P::PlayerVtableRva)
            return ClockResult::UnsupportedBuild;
        if (!D::Read(next.ped.nativePed + P::MatrixPointerOffset, next.matrix))
            return ClockResult::ReadFailure;
        if (next.matrix)
        {
            std::uintptr_t matrixOwner = 0;
            if (next.matrix % alignof(std::uintptr_t) || !D::NativePages(next.matrix, P::MatrixOwnerOffset + sizeof(matrixOwner)) ||
                !D::Read(next.matrix + P::MatrixOwnerOffset, matrixOwner))
                return ClockResult::ReadFailure;
            if (matrixOwner != next.ped.nativePed)
                return ClockResult::BindingChanged;
            for (std::size_t i = 0; i != next.matrixRows.size(); ++i)
                if (!D::Read(next.matrix + i * 0x10, next.matrixRows[i]))
                    return ClockResult::ReadFailure;
            next.positionAddress = next.matrix + P::MatrixPositionOffset;
            next.position = next.matrixRows[3];
        }
        else
        {
            next.positionAddress = next.ped.nativePed + P::InlinePositionOffset;
            if (!D::Read(next.positionAddress, next.position))
                return ClockResult::ReadFailure;
        }
        if (!D::Read(next.ped.nativePed + P::VelocityOffset, next.velocity))
            return ClockResult::ReadFailure;
        std::uintptr_t currentMatrix = 0, matrixOwner = 0;
        if (!D::Read(next.ped.nativePed + P::MatrixPointerOffset, currentMatrix) || (next.matrix && !D::Read(next.matrix + P::MatrixOwnerOffset, matrixOwner)))
            return ClockResult::ReadFailure;
        if (currentMatrix != next.matrix || (next.matrix && matrixOwner != next.ped.nativePed))
            return ClockResult::BindingChanged;
        if ((result = pool.Recheck(next.ped)) != ClockResult::Ok)
            return result;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult NativePedTransformReader::Read(std::uint32_t handle, Value& output) const noexcept
    {
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::UnsupportedBuild;
        Value first, second;
        if ((result = Capture(handle, first)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok ||
            (result = Capture(handle, second)) != ClockResult::Ok)
            return result;
        // Raw vector bits intentionally survive. Numeric repair is a separate
        // genuine SDK wrapper policy; a snapshot must not fabricate a last-good
        // native value or multiply by the script API's velocity scale.
        if (!PedTransformDetail::Same(first, second) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = owner()) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativePedTransformReader::Recheck(const Value& previous) const noexcept
    {
        Value      next;
        const auto result = Read(previous.ped.handle, next);
        return result != ClockResult::Ok ? result : PedTransformDetail::Same(previous, next) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
    ClockResult NativePedTransformReader::BorrowPosition(std::uint32_t handle, CVector*& output) const noexcept
    {
        Value first, confirmed;
        auto  result = Read(handle, first);
        if (result != ClockResult::Ok || (result = Capture(handle, confirmed)) != ClockResult::Ok)
            return result;
        if (!PedTransformDetail::Same(first, confirmed) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = owner()) != ClockResult::Ok)
            return result;
        output = reinterpret_cast<CVector*>(confirmed.positionAddress);
        return ClockResult::Ok;
    }
}

/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual gravity getter bits while refusing changed native meaning
 *****************************************************************************/
#include "NativeGravityReadAccess.h"
#include "NativeGravityReadProfile.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <windows.h>

namespace mta::game_de
{
    bool NativeGravityReadAccess::Qualify() const noexcept
    {
        namespace Profile = NativeGravityReadProfile;
        if (!image || !read || image > (std::numeric_limits<std::uintptr_t>::max)() - Profile::ImageSize)
            return false;
        IMAGE_DOS_HEADER dos{};
        if (!read(image, image, &dos, sizeof(dos), false) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > Profile::ImageSize - sizeof(IMAGE_NT_HEADERS64))
            return false;
        const auto         ntRva = static_cast<std::uintptr_t>(dos.e_lfanew);
        IMAGE_NT_HEADERS64 nt{};
        if (!read(image, image + ntRva, &nt, sizeof(nt), false) || nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != Profile::ImageSize ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto sectionRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto sectionBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        if (sectionRva > Profile::ImageSize || sectionBytes > Profile::ImageSize - sectionRva)
            return false;
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (!read(image, image + sectionRva, sections.data(), sectionBytes, false))
            return false;
        const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
        {
            for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
            {
                const auto& section = sections[index];
                if ((section.Characteristics & required) == required && !(section.Characteristics & forbidden) && rva >= section.VirtualAddress &&
                    rva - section.VirtualAddress <= section.Misc.VirtualSize && size <= section.Misc.VirtualSize - (rva - section.VirtualAddress))
                    return true;
            }
            return false;
        };
        // These two operands are distinct read-only pools. Their file defaults
        // document native signs, not a mutable world object or a coupled setter.
        for (const auto rva : {Profile::DownwardAccelerationRva, Profile::OrientedAccelerationRva})
            if (!contains(rva, sizeof(float), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                return false;
        for (const auto& pin : Profile::Code)
        {
            std::array<unsigned char, 96> actual{};
            if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !read(image, image + pin.rva, actual.data(), pin.size, true) || std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                return false;
        }
        return true;
    }

    bool NativeGravityReadAccess::Read(float& output) const noexcept
    {
        if (!Qualify())
            return false;
        std::uint32_t first = 0, second = 0;
        const auto    address = image + NativeGravityReadProfile::DownwardAccelerationRva;
        if (!read(image, address, &first, sizeof(first), false) || !read(image, address, &second, sizeof(second), false) || first != second)
            return false;
        static_assert(sizeof(float) == sizeof(second));
        float value = 0;
        std::memcpy(&value, &second, sizeof(value));
        if (!std::isfinite(value) || !Qualify())
            return false;
        // Native arithmetic applies this operand without clamping or scaling.
        // Preserve all finite bits, including signed zero and changed finite
        // values. Matching samples are consistency, not a native physics lock.
        std::memcpy(&output, &second, sizeof(output));
        return true;
    }
}

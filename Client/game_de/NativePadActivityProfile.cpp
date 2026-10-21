/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit genuine pad timestamp storage without native calls or writes
 *****************************************************************************/
#include "NativePadActivityProfile.h"
#include "NativeImageAccess.h"
#include <cstring>
#include <limits>
#include <windows.h>

namespace mta::game_de::NativePadActivityProfile
{
    namespace
    {
        namespace Image = NativeImageAccess;
        bool Verify(std::uintptr_t image) noexcept
        {
            if (sizeof(void*) != 8 || !image || image > (std::numeric_limits<std::uintptr_t>::max)() - ImageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > ImageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            const auto         ntRva = static_cast<std::uintptr_t>(dos.e_lfanew);
            IMAGE_NT_HEADERS64 nt{};
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionsSize = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > ImageSize || sectionsSize > ImageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionsRva, sections.data(), sectionsSize))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(PadArrayRva, PadCount * PadStride, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(NativeTimerRva, FieldSize, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + PadArrayRva, PadCount * PadStride, true) ||
                !Image::ImagePages(image, image + NativeTimerRva, FieldSize, true))
                return false;
            for (std::size_t pad = 0; pad < PadCount; ++pad)
            {
                const auto    base = image + PadArrayRva + pad * PadStride;
                std::uint32_t identity = 0;
                // The initializer assigns immutable physical IDs. A malformed
                // or not-yet-constructed second pad must not create a binding.
                if (!Image::ReadImage(image, base + PadIdentityOffset, &identity, sizeof(identity), true) || identity != pad ||
                    !Image::ImagePages(image, base + FieldOffset, FieldSize, true))
                    return false;
            }
            for (const auto& pin : Pins)
            {
                std::array<unsigned char, 16> actual{};
                if (pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
    }
    bool Qualify(Binding& output) noexcept
    {
        if (sizeof(void*) != 8 || !NativeImageAccess::CheckExecutableBuild())
            return false;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!Verify(image))
            return false;
        output = {image, image + PadArrayRva};
        return true;
    }
    bool Revalidate(const Binding& binding) noexcept
    {
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        return image && binding.image == image && image <= (std::numeric_limits<std::uintptr_t>::max)() - ImageSize && binding.pads == image + PadArrayRva &&
               Verify(image);
    }
}

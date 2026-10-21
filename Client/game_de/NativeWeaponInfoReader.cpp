/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe actual DE weapon rows without executing native functions
 *****************************************************************************/
#include "NativeWeaponInfoReader.h"
#include "NativeWeaponInfoProfile.h"
#include "NativeImageAccess.h"

#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace Profile = NativeWeaponInfoProfile;
        namespace Image = NativeImageAccess;
        static_assert(sizeof(NativeWeaponInfoValues) == Profile::RowBytes);

        bool Binding(std::uintptr_t image) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - Profile::ImageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > Profile::ImageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::uintptr_t>(dos.e_lfanew);
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != Profile::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionRva > Profile::ImageSize || sectionBytes > Profile::ImageSize - sectionRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionRva, sections.data(), sectionBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            // Independently decoded row count and stride describe this bank,
            // not an arbitrary writable image region with classic-like bytes.
            if (!contains(Profile::BankRva, Profile::RowCount * Profile::RowBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + Profile::BankRva, Profile::RowCount * Profile::RowBytes, true))
                return false;
            const auto pinsMatch = [&](const auto& pins, bool code)
            {
                for (const auto& pin : pins)
                {
                    std::array<unsigned char, 160> actual{};
                    if (!pin.size || pin.size > actual.size() ||
                        !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | (code ? IMAGE_SCN_MEM_EXECUTE : 0),
                                  IMAGE_SCN_MEM_WRITE | (code ? 0 : IMAGE_SCN_MEM_EXECUTE)) ||
                        !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, code) ||
                        std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                        return false;
                }
                return true;
            };
            return pinsMatch(Profile::Code, true) && pinsMatch(Profile::Literals, false);
        }
    }

    Status NativeWeaponInfoReader::Bind() noexcept
    {
        const auto thread = GetCurrentThreadId();
        if (ownerThread.load() && ownerThread.load() != thread)
            return Status::WrongThread;
        Reset();
        if (sizeof(void*) != 8 || !Image::CheckExecutableBuild())
            return Status::Unsupported;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!Binding(image))
            return Status::Unsupported;
        imageAddress.store(image);
        ownerThread.store(thread);
        return Status::Ok;
    }

    Status NativeWeaponInfoReader::Read(std::size_t sdkSlot, NativeWeaponInfoValues& output) const noexcept
    {
        if (sdkSlot >= Profile::RowCount)
            return Status::InvalidArgument;
        const auto image = imageAddress.load();
        const auto owner = ownerThread.load();
        if (!image || !owner)
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) || !Binding(image))
            return Status::Unsupported;
        const auto             address = image + Profile::BankRva + sdkSlot * Profile::RowBytes;
        NativeWeaponInfoValues first, second;
        if (!Image::ReadImage(image, address, &first, sizeof(first), true) || !Image::ReadImage(image, address, &second, sizeof(second), true) ||
            std::memcmp(&first, &second, sizeof(first)) || !Binding(image))
            return Status::Unsupported;
        // Matching bookends are observations, not a native loader lock. Publish
        // only successful complete rows; failed OS copies leave output intact.
        output = second;
        return Status::Ok;
    }

    void NativeWeaponInfoReader::Reset() noexcept
    {
        ownerThread.store(0);
        imageAddress.store(0);
    }

    bool NativeWeaponInfoReader::IsBound() const noexcept
    {
        return imageAddress.load() != 0 && ownerThread.load() != 0;
    }
}

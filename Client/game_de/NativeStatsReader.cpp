/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse proven stat bank/classifier semantics behind the actual SDK
 *****************************************************************************/
#include "NativeStatsReader.h"
#include "NativeStatsProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace mta::game_de
{
    namespace
    {
        namespace Profile = NativeStatsProfile;
        namespace Image = NativeImageAccess;
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
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > Profile::ImageSize || bytes > Profile::ImageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionsRva, sections.data(), bytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                    if (Image::SectionContains(sections[i], rva, size, required, forbidden))
                        return true;
                return false;
            };
            for (const auto bank : {std::pair{Profile::FloatBankRva, static_cast<std::size_t>(Profile::FloatCount)},
                                    std::pair{Profile::IntegerBankRva, static_cast<std::size_t>(Profile::IntegerStorageCount)}})
            {
                const auto size = bank.second * sizeof(std::uint32_t);
                if (!contains(bank.first, size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                    !Image::ImagePages(image, image + bank.first, size, true))
                    return false;
            }
            for (const auto& pin : Profile::Code)
            {
                std::array<unsigned char, 160> actual{};
                if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            return true;
        }
    }

    Status NativeStatsReader::Bind() noexcept
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
    Status NativeStatsReader::CurrentImage(std::uintptr_t& output) const noexcept
    {
        const auto image = imageAddress.load();
        const auto thread = ownerThread.load();
        if (!image || !thread)
            return Status::NotInitialized;
        if (thread != GetCurrentThreadId())
            return Status::WrongThread;
        if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) || !Binding(image))
            return Status::Unsupported;
        output = image;
        return Status::Ok;
    }
    Status NativeStatsReader::Read(unsigned short index, float& output) const noexcept
    {
        const bool floating = index < Profile::FloatCount;
        if (!floating && (index < Profile::IntegerFirst || index >= Profile::IntegerSdkEnd))
            return Status::InvalidArgument;
        std::uintptr_t image = 0;
        const auto     status = CurrentImage(image);
        if (status != Status::Ok)
            return status;
        const auto    rva = floating ? Profile::FloatBankRva + index * sizeof(std::uint32_t)
                                     : Profile::IntegerBankRva + (index - Profile::IntegerFirst) * sizeof(std::uint32_t);
        std::uint32_t first = 0, second = 0;
        if (!Image::ReadImage(image, image + rva, &first, sizeof(first), true) || !Image::ReadImage(image, image + rva, &second, sizeof(second), true) ||
            first != second || image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) || !Binding(image))
            return Status::Unsupported;
        float value = 0;
        if (floating)
            std::memcpy(&value, &second, sizeof(value));
        else
        {
            std::int32_t integer = 0;
            std::memcpy(&integer, &second, sizeof(integer));
            value = static_cast<float>(integer);
        }
        // Publish only complete matching observations. The native getter's
        // signed integer conversion is preserved without aliasing float gaps.
        output = value;
        return Status::Ok;
    }
    Status NativeStatsReader::ReadSkillIndex(eWeaponType weapon, unsigned short& output) const noexcept
    {
        std::uintptr_t image = 0;
        const auto     status = CurrentImage(image);
        if (status != Status::Ok)
            return status;
        const auto     type = static_cast<std::int64_t>(weapon);
        unsigned short index = (std::numeric_limits<unsigned short>::max)();
        // This is the actual native classifier's bounded CPU policy. Its
        // existing outer guard excludes the otherwise unreachable rifle case;
        // the presence of a named sniper stat does not authorize inventing it.
        if (type >= 22 && type <= 32)
            index = type == 32 ? 75 : static_cast<unsigned short>(69 + type - 22);
        if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) || !Binding(image))
            return Status::Unsupported;
        output = index;
        return Status::Ok;
    }
    void NativeStatsReader::Reset() noexcept
    {
        ownerThread.store(0);
        imageAddress.store(0);
    }
    bool NativeStatsReader::IsBound() const noexcept
    {
        return ownerThread.load() && imageAddress.load();
    }
}

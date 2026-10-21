/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native data filename verification without classic offsets
 *****************************************************************************/
#include "NativeDataFileNamesReader.h"
#include "NativeDataFileNamesProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace Profile = NativeDataFileNamesProfile;
        namespace Image = NativeImageAccess;

        bool Sample(std::uintptr_t image, bool& verified) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - Profile::ImageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > Profile::ImageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            const auto         ntRva = static_cast<std::uintptr_t>(dos.e_lfanew);
            IMAGE_NT_HEADERS64 nt{};
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
            const auto codeMatches = [&]()
            {
                for (const auto& pin : Profile::Code)
                {
                    std::array<unsigned char, 160> actual{};
                    if (!pin.size || pin.size > actual.size() ||
                        !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                        !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                        std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                        return false;
                }
                return true;
            };
            const auto data = [&](std::uintptr_t rva, std::size_t size, void* output)
            {
                return contains(rva, size, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE) &&
                       Image::ReadImage(image, image + rva, output, size);
            };
            if (!codeMatches())
                return false;
            for (const auto& literal : Profile::Literals)
            {
                std::array<char, 4> actual{};
                if (!literal.size || literal.size > actual.size() || !data(literal.rva, literal.size, actual.data()) ||
                    std::memcmp(actual.data(), literal.expected, literal.size))
                    return false;
            }
            bool next = true;
            for (const auto& name : Profile::Names)
            {
                std::array<char, 32> actual{};
                if (!name.size || name.size > actual.size() || !data(name.rva, name.size, actual.data()))
                    return false;
                // Classic VerifySADataFileNames returns false for an altered
                // loader filename. Keep that observation distinct from an
                // unreadable/changed binding, without probing arbitrary paths.
                next = (std::memcmp(actual.data(), name.expected, name.size) == 0) && next;
            }
            // A code change across the data reads must not validate names
            // whose native loader operand relationship has already changed.
            if (!codeMatches())
                return false;
            verified = next;
            return true;
        }
    }

    Status NativeDataFileNamesReader::Bind() noexcept
    {
        const auto thread = GetCurrentThreadId();
        if (ownerThread.load() && ownerThread.load() != thread)
            return Status::WrongThread;
        Reset();
        if (sizeof(void*) != 8 || !Image::CheckExecutableBuild())
            return Status::Unsupported;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        bool       observed = false;
        if (!Sample(image, observed))
            return Status::Unsupported;
        imageAddress.store(image);
        ownerThread.store(thread);
        return Status::Ok;
    }

    Status NativeDataFileNamesReader::Read(bool& verified) const noexcept
    {
        const auto image = imageAddress.load();
        const auto owner = ownerThread.load();
        if (!image || !owner)
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
            return Status::Unsupported;
        bool first = false, second = false;
        if (!Sample(image, first) || !Sample(image, second) || first != second)
            return Status::Unsupported;
        // Matching bookends are read consistency, not an engine/loader lock.
        // No filesystem or archive-content claim is made by a true result.
        verified = second;
        return Status::Ok;
    }

    void NativeDataFileNamesReader::Reset() noexcept
    {
        ownerThread.store(0);
        imageAddress.store(0);
    }

    bool NativeDataFileNamesReader::IsBound() const noexcept
    {
        return imageAddress.load() != 0 && ownerThread.load() != 0;
    }
}

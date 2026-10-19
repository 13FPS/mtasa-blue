/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read verified DE partition operands without classic row-layout casts
 *****************************************************************************/
#include "NativeStreamingFileIdsReader.h"
#include "NativeStreamingFileIdsProfile.h"
#include "NativeImageAccess.h"

#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace Profile = NativeStreamingFileIdsProfile;
        namespace Image = NativeImageAccess;

        template <std::size_t Size>
        bool Pin(std::uintptr_t image, std::uintptr_t rva, const std::array<unsigned char, Size>& expected, bool code,
                 std::array<unsigned char, Size>* output = nullptr) noexcept
        {
            std::array<unsigned char, Size> actual{};
            if (!Image::ReadImage(image, image + rva, actual.data(), actual.size(), false, code) || actual != expected)
                return false;
            if (output)
                *output = actual;
            return true;
        }

        template <typename T, std::size_t Size>
        T Operand(const std::array<unsigned char, Size>& code, std::size_t offset) noexcept
        {
            T result{};
            std::memcpy(&result, code.data() + offset, sizeof(result));
            return result;
        }

        bool Sample(std::uintptr_t image, NativeStreamingFileIds& output) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - Profile::imageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > Profile::imageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != Profile::imageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > Profile::imageSize || sectionsBytes > Profile::imageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t bytes, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, bytes, required, forbidden))
                        return true;
                return false;
            };
            const auto code = [&](std::uintptr_t rva, const auto& bytes, auto* destination)
            {
                return contains(rva, bytes.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) &&
                       Pin(image, rva, bytes, true, destination);
            };
            const auto label = [&](std::uintptr_t rva, const auto& bytes)
            { return contains(rva, bytes.size(), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) && Pin(image, rva, bytes, false); };
            std::array<unsigned char, Profile::initialize.size()> initializer{};
            std::array<unsigned char, Profile::selector.size()>   selector{};
            if (!code(Profile::initializeRva, Profile::initialize, &initializer) || !code(Profile::selectorRva, Profile::selector, &selector) ||
                !code(Profile::classifierRva, Profile::classifier, static_cast<std::array<unsigned char, Profile::classifier.size()>*>(nullptr)) ||
                !code(Profile::requestStrideRva, Profile::requestStride, static_cast<std::array<unsigned char, Profile::requestStride.size()>*>(nullptr)) ||
                !code(Profile::requestDffRva, Profile::requestDff, static_cast<std::array<unsigned char, Profile::requestDff.size()>*>(nullptr)) ||
                !code(Profile::requestWindowsRva, Profile::requestWindows, static_cast<std::array<unsigned char, Profile::requestWindows.size()>*>(nullptr)) ||
                !label(Profile::categoryNamesRva, Profile::categoryNames) || !label(Profile::iplNameRva, Profile::iplName) ||
                !label(Profile::filenameFormatsRva, Profile::filenameFormats) || !label(Profile::scriptFormatRva, Profile::scriptFormat) ||
                !label(Profile::animationRecordingFormatsRva, Profile::animationRecordingFormats))
                return false;

            // Return the live, checked operands rather than cached default limits.
            // DFF's zero base is encoded as the selector's unbiased model index;
            // every following category is an explicit native comparison operand.
            NativeStreamingFileIds next;
            next.dff = 0;
            next.txd = Operand<std::uint32_t>(selector, 0x1286b56 - Profile::selectorRva);
            next.col = Operand<std::uint32_t>(selector, 0x1286b80 - Profile::selectorRva);
            next.ipl = Operand<std::uint32_t>(selector, 0x1286bd0 - Profile::selectorRva);
            next.dat = Operand<std::uint32_t>(selector, 0x1286c1f - Profile::selectorRva);
            next.ifp = Operand<std::uint32_t>(selector, 0x1286c6e - Profile::selectorRva);
            next.rrr = Operand<std::uint32_t>(selector, 0x1286c9c - Profile::selectorRva);
            next.scm = Operand<std::uint32_t>(selector, 0x1286cc4 - Profile::selectorRva);
            const auto extent = Operand<std::uint32_t>(initializer, 0x113bde3 - Profile::initializeRva);
            const auto stride = initializer[0x113bde0 - Profile::initializeRva];
            if (!stride || extent % stride)
                return false;
            next.count = extent / stride;
            const auto arrayRva = static_cast<std::int64_t>(0x113bdc3) + Operand<std::int32_t>(initializer, 0x113bdbf - Profile::initializeRva);
            if (arrayRva != static_cast<std::int64_t>(Profile::nativeArrayRva) || next.count < 4 ||
                !contains(Profile::nativeArrayRva, extent, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + Profile::nativeArrayRva, extent, true))
                return false;
            constexpr std::array<std::uintptr_t, 4> sentinelInstructions = {0x113bde9, 0x113be05, 0x113be24, 0x113be43};
            for (std::size_t index = 0; index < sentinelInstructions.size(); ++index)
            {
                const auto instruction = sentinelInstructions[index];
                const auto sentinel = static_cast<std::int64_t>(instruction + 7) + Operand<std::int32_t>(initializer, instruction + 3 - Profile::initializeRva);
                if (sentinel != arrayRva + static_cast<std::int64_t>((next.count - 4 + index) * stride))
                    return false;
            }
            // The native initializer includes all four list sentinels. Their
            // contents are not read and no native row is cast to CStreamingInfo.
            output = next;
            return true;
        }
    }

    Status NativeStreamingFileIdsReader::Bind() noexcept
    {
        const auto thread = GetCurrentThreadId();
        if (ownerThread.load() && ownerThread.load() != thread)
            return Status::WrongThread;
        Reset();
        if (sizeof(void*) != 8 || !Image::CheckExecutableBuild())
            return Status::Unsupported;
        const auto             image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        NativeStreamingFileIds values;
        if (!Sample(image, values))
            return Status::Unsupported;
        imageAddress.store(image);
        ownerThread.store(thread);
        return Status::Ok;
    }

    Status NativeStreamingFileIdsReader::Read(NativeStreamingFileIds& output) const noexcept
    {
        const auto owner = ownerThread.load();
        const auto image = imageAddress.load();
        if (!owner || !image)
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        NativeStreamingFileIds first, second;
        if (!Sample(image, first) || !Sample(image, second) || std::memcmp(&first, &second, sizeof(first)) != 0)
            return Status::Unsupported;
        // Matching sampled reads do not claim an engine lock. Bind establishes
        // caller-thread read ownership only and grants no write/callback authority.
        output = second;
        return Status::Ok;
    }

    void NativeStreamingFileIdsReader::Reset() noexcept
    {
        ownerThread.store(0);
        imageAddress.store(0);
    }

    bool NativeStreamingFileIdsReader::IsBound() const noexcept
    {
        return ownerThread.load() != 0 && imageAddress.load() != 0;
    }
}

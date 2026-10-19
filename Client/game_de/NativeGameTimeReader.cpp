/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Sample genuine native DE time without hooks, writes or emulation
 *****************************************************************************/
#include "NativeGameTimeReader.h"
#include "NativeClockMemory.h"
#include "CClockDE.h"

#include <array>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace TimeProfile = NativeClockProfile;
        namespace TimeImage = NativeImageAccess;

        template <std::size_t Size>
        bool TimeCode(std::uintptr_t image, std::uintptr_t rva, const std::array<unsigned char, Size>& expected) noexcept
        {
            std::array<unsigned char, Size> actual{};
            return TimeImage::ReadImage(image, image + rva, actual.data(), actual.size(), false, true) && actual == expected;
        }

        bool TimeBinding(std::uintptr_t image) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - TimeProfile::imageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!TimeImage::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > TimeProfile::imageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!TimeImage::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != TimeProfile::imageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > TimeProfile::imageSize || sectionsBytes > TimeProfile::imageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!TimeImage::ReadImage(image, image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t bytes, bool code)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (TimeImage::SectionContains(sections[index], rva, bytes, IMAGE_SCN_MEM_READ | (code ? IMAGE_SCN_MEM_EXECUTE : IMAGE_SCN_MEM_WRITE),
                                                   code ? 0 : IMAGE_SCN_MEM_EXECUTE))
                        return true;
                return false;
            };
            // Reuse the exact clock calculation and script getter evidence.
            // A detoured/frozen clock span is refused instead of guessing its
            // current owner. Native input/render leases alter neither span.
            return contains(TimeProfile::clockCodeRva, TimeProfile::clockCode.size(), true) &&
                   contains(TimeProfile::getterCaseRva, TimeProfile::getterCase.size(), true) && contains(TimeProfile::hoursRva, 1, false) &&
                   contains(TimeProfile::minutesRva, 1, false) && contains(TimeProfile::secondsRva, 2, false) &&
                   contains(TimeProfile::nativeTimerRva, 4, false) && contains(TimeProfile::lastChangeRva, 4, false) &&
                   contains(TimeProfile::minuteDurationRva, 4, false) && TimeCode(image, TimeProfile::clockCodeRva, TimeProfile::clockCode) &&
                   TimeCode(image, TimeProfile::getterCaseRva, TimeProfile::getterCase);
        }
    }

    Status NativeGameTimeReader::Bind() noexcept
    {
        const auto thread = GetCurrentThreadId();
        if (ownerThread.load() && ownerThread.load() != thread)
            return Status::WrongThread;
        Reset();
        if (sizeof(void*) != 8 || !TimeImage::CheckExecutableBuild())
            return Status::Unsupported;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!TimeBinding(image))
            return Status::Unsupported;
        imageAddress.store(image);
        ownerThread.store(thread);
        return Status::Ok;
    }

    Status NativeGameTimeReader::Read(GameTimeStateV1& output) const noexcept
    {
        const auto owner = ownerThread.load();
        if (!owner || !imageAddress.load())
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        const auto  image = imageAddress.load();
        ClockValues first, second;
        if (!TimeBinding(image) || !NativeClockMemory::Read(image, first) || !NativeClockMemory::Read(image, second) ||
            !NativeClockMemory::Equal(first, second) || !TimeBinding(image))
            return Status::Unsupported;
        GameTimeStateV1 next;
        next.producerThread = owner;
        next.hour = second.hour;
        next.minute = second.minute;
        next.seconds = second.seconds;
        next.nativeTimer = second.nativeTimer;
        next.lastMinuteChange = second.lastMinuteChange;
        next.millisecondsPerGameMinute = second.millisecondsPerGameMinute;
        output = next;
        return Status::Ok;
    }

    void NativeGameTimeReader::Reset() noexcept
    {
        ownerThread.store(0);
        imageAddress.store(0);
    }

    bool NativeGameTimeReader::IsBound() const noexcept
    {
        return ownerThread.load() != 0 && imageAddress.load() != 0;
    }
}

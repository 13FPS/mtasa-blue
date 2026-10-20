/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualify pad updater identity without invoking or modifying it
 *****************************************************************************/
#include "NativePadUpdateProfile.h"
#include "NativeImageAccess.h"

#include <cstring>
#include <limits>
#include <windows.h>

namespace mta::game_de::NativePadUpdateProfile
{
    namespace
    {
        namespace Image = NativeImageAccess;

        bool Verify(std::uintptr_t image, bool requireUnhookedEntry) noexcept
        {
#if defined(_WIN64)
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - ImageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > ImageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::uintptr_t>(dos.e_lfanew);
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96 ||
                nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXCEPTION)
                return false;
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionsSize = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > ImageSize || sectionsSize > ImageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionsRva, sections.data(), sectionsSize))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t bytes, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, bytes, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(UpdaterRva, EndRva - UpdaterRva, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !contains(PadArrayRva, PadCount * PadStride, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + UpdaterRva, EndRva - UpdaterRva, false, true) ||
                !Image::ImagePages(image, image + PadArrayRva, PadCount * PadStride, true))
                return false;
            std::array<unsigned char, EntryBytes.size()> entry{};
            if (requireUnhookedEntry && (!Image::ReadImage(image, image + UpdaterRva, entry.data(), entry.size(), false, true) || entry != EntryBytes))
                return false;
            for (const auto& pin : Pins)
            {
                if (!contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    return false;
                std::array<unsigned char, 32> actual{};
                if (pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true))
                    return false;
                for (std::size_t index = 0; index < pin.size; ++index)
                {
                    const auto rva = pin.rva + index;
                    // The owner checks its complete captured entry snapshot.
                    // Even in hooked mode no byte outside that prefix can vary.
                    if (!requireUnhookedEntry && rva >= UpdaterRva && rva - UpdaterRva < EntryBytes.size())
                        continue;
                    if (actual[index] != pin.bytes[index])
                        return false;
                }
            }
            const auto& exception = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
            if (!exception.VirtualAddress || exception.Size > ImageSize || exception.VirtualAddress > ImageSize - exception.Size)
                return false;
            for (const auto& expected : RuntimeEntries)
            {
                if (expected.rva < exception.VirtualAddress || expected.rva - exception.VirtualAddress > exception.Size ||
                    sizeof(RUNTIME_FUNCTION) > exception.Size - (expected.rva - exception.VirtualAddress) ||
                    !contains(expected.rva, sizeof(RUNTIME_FUNCTION), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                    !contains(expected.unwind, 4, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                    return false;
                DWORD64          base = 0;
                const auto       found = RtlLookupFunctionEntry(image + expected.begin, &base, nullptr);
                RUNTIME_FUNCTION actual{};
                // Compare the OS result with the precise immutable .pdata row,
                // not an arbitrary dynamic unwind table covering similar code.
                if (base != image || reinterpret_cast<std::uintptr_t>(found) != image + expected.rva ||
                    !Image::ReadImage(image, image + expected.rva, &actual, sizeof(actual)) || actual.BeginAddress != expected.begin ||
                    actual.EndAddress != expected.end || actual.UnwindData != expected.unwind)
                    return false;
            }
            return true;
#else
            (void)image;
            (void)requireUnhookedEntry;
            return false;
#endif
        }
    }

    bool Qualify(Binding& output) noexcept
    {
        // The same existing executable digest guards every native service.
        // Never publish a partial binding when metadata or a code pin refuses.
        if (sizeof(void*) != 8 || !NativeImageAccess::CheckExecutableBuild())
            return false;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!Verify(image, true))
            return false;
        output = {image, image + UpdaterRva, image + PadArrayRva};
        return true;
    }

    bool Revalidate(const Binding& binding, bool requireUnhookedEntry) noexcept
    {
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        return sizeof(void*) == 8 && binding.image == image && image && image <= (std::numeric_limits<std::uintptr_t>::max)() - ImageSize &&
               binding.updater == image + UpdaterRva && binding.pads == image + PadArrayRva && Verify(image, requireUnhookedEntry);
    }
}

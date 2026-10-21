/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualify native world-property cheat bytes without executing cheats
 *****************************************************************************/
#include "NativeCheatProfile.h"
#include "NativeImageAccess.h"
#include <cstring>
#include <limits>
#include <windows.h>

namespace mta::game_de::NativeCheatProfile
{
    const std::array<Pin, 7>           Code = {{
        {0x10708d6, 29, {0x33, 0xdb, 0x48, 0x8d, 0x0d, 0xd1, 0xc7, 0x14, 0x04, 0x8b, 0xc3, 0x48, 0x83, 0xf8, 0x75,
                                   0x73, 0x6a, 0x88, 0x1c, 0x08, 0x48, 0xff, 0xc0, 0x48, 0x83, 0xf8, 0x75, 0x7c, 0xf2}},
        {0x10817b4, 50, {0x48, 0x8d, 0x1d, 0x45, 0xe8, 0xf7, 0xfe, 0x4a, 0x8b, 0x84, 0xe3, 0x70, 0x64, 0x30, 0x04, 0x4c, 0x8b,
                                   0xac, 0x24, 0xa8, 0x01, 0x00, 0x00, 0x48, 0x8b, 0xbc, 0x24, 0xa0, 0x01, 0x00, 0x00, 0x48, 0x8b, 0xb4,
                                   0x24, 0x98, 0x01, 0x00, 0x00, 0x48, 0x85, 0xc0, 0x0f, 0x84, 0xa6, 0x00, 0x00, 0x00, 0xff, 0xd0}},
        {0x108188a, 20, {0x41, 0x80, 0xbc, 0x1c, 0xb0, 0xd0, 0x1b, 0x05, 0x00, 0x0f, 0x94, 0xc0, 0x41, 0x88, 0x84, 0x1c, 0xb0, 0xd0, 0x1b, 0x05}},
        {0x13838a9, 9, {0x80, 0x3d, 0x2d, 0x98, 0xe3, 0x03, 0x00, 0x74, 0x51}},
        {0x137ef10, 34, {0x48, 0x83, 0xec, 0x38, 0x0f, 0xb6, 0x41, 0x6a, 0xc0, 0xe8, 0x03, 0x3c, 0x08, 0x77, 0x72, 0xba, 0x09,
                                   0x01, 0x00, 0x00, 0x0f, 0xa3, 0xc2, 0x73, 0x68, 0x80, 0x3d, 0xbc, 0xe1, 0xe3, 0x03, 0x00, 0x74, 0x5f}},
        {0x13a6cbb, 17, {0x80, 0x3d, 0x2b, 0x64, 0xe1, 0x03, 0x00, 0x74, 0x08, 0xf3, 0x0f, 0x59, 0x1d, 0x38, 0x28, 0x82, 0x03}},
        {0x1353636, 22, {0x80, 0x3d, 0xbb, 0x9a, 0xe6, 0x03, 0x00, 0x74, 0x0d, 0x83, 0xf8, 0x01, 0x77, 0x08, 0xf3, 0x0f, 0x59, 0x15, 0xec, 0x5f, 0x87, 0x03}},
    }};
    const std::array<Registration, 10> Registrations = {{
        {0x10752b8, 0x4306d50, "R4C32S34"},
        {0x1075321, 0x4306d78, "DRIVEONWATER"},
        {0x1076f68, 0x4306f80, "CHITTYCHITTYBANGBANG"},
        {0x1076fd1, 0x4307278, "UD131RL1L"},
        {0x107703a, 0x4307270, "RIPAZHA"},
        {0x1077171, 0x43068a8, "CJPHONEHOME"},
        {0x10771da, 0x4307290, "TSCCSCC12234"},
        {0x1077243, 0x4307288, "JHJOECW"},
        {0x107862c, 0x4307180, "UUTTUULRS44"},
        {0x1078695, 0x4307170, "KANGAROO"},
    }};
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
            const auto sectionRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionRva > ImageSize || sectionBytes > ImageSize - sectionRva)
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
            if (!contains(FlagsRva, NativeFlagCount, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + FlagsRva, NativeFlagCount, true))
                return false;
            for (const auto& pin : Code)
            {
                std::array<unsigned char, 160> actual{};
                if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            for (const auto& registration : Registrations)
            {
                std::array<unsigned char, 7> instruction{};
                std::array<char, 32>         name{};
                const auto                   size = std::strlen(registration.name) + 1;
                if (size > name.size() ||
                    !contains(registration.instructionRva, instruction.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !contains(registration.nameRva, size, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + registration.instructionRva, instruction.data(), instruction.size(), false, true) ||
                    instruction[0] != 0x4c || instruction[1] != 0x8d || instruction[2] != 0x05 ||
                    !Image::ReadImage(image, image + registration.nameRva, name.data(), size) || std::memcmp(name.data(), registration.name, size))
                    return false;
                std::int32_t relative = 0;
                std::memcpy(&relative, instruction.data() + 3, sizeof(relative));
                if (static_cast<std::int64_t>(registration.instructionRva) + 7 + relative != static_cast<std::int64_t>(registration.nameRva))
                    return false;
            }
            for (const auto& member : Members)
            {
                if (member.nativeIndex >= NativeFlagCount)
                    return false;
                const auto    slotRva = DispatchTableRva + member.nativeIndex * sizeof(std::uint64_t);
                std::uint64_t function = 1;
                unsigned char value = 0;
                // The native dispatcher directly toggles these null-function
                // entries. Calling its broader action path would run unrelated
                // achievement/UI/gameplay side effects and is never required.
                if (!contains(slotRva, sizeof(function), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + slotRva, &function, sizeof(function)) || function != 0 ||
                    !Image::ReadImage(image, image + FlagsRva + member.nativeIndex, &value, sizeof(value), true) || value > 1)
                    return false;
            }
            return true;
        }
    }
    bool Find(const char* name, Flag& output) noexcept
    {
        if (!name)
            return false;
        for (std::size_t index = 0; index < Members.size(); ++index)
            if (!std::strcmp(name, Members[index].sdkName))
            {
                output = static_cast<Flag>(index);
                return true;
            }
        return false;
    }
    bool Qualify(Binding& output) noexcept
    {
        if (sizeof(void*) != 8 || !NativeImageAccess::CheckExecutableBuild())
            return false;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!Verify(image))
            return false;
        output = {image, image + FlagsRva};
        return true;
    }
    bool Revalidate(const Binding& binding) noexcept
    {
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        return image && binding.image == image && image <= (std::numeric_limits<std::uintptr_t>::max)() - ImageSize && binding.flags == image + FlagsRva &&
               Verify(image);
    }
}

/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit, read-only binding to the tested DE executable's pad array
 *****************************************************************************/

#include "NativePadReader.h"
#include "NativeImageAccess.h"

#include <array>
#include <limits>
#include <windows.h>

namespace mta::game_de
{
    namespace
    {
        using NativeImageAccess::CheckExecutableBuild;
        using NativeImageAccess::ImagePages;
        using NativeImageAccess::ReadImage;
        using NativeImageAccess::SectionContains;

        constexpr std::size_t                   imageSize = 97141760;
        constexpr std::size_t                   instructionRva = 17844674;
        constexpr std::size_t                   padsRva = 87984144;
        constexpr std::size_t                   padStride = NativePadStride;
        constexpr std::size_t                   padsSize = 2 * padStride;
        constexpr std::array<unsigned char, 12> expectedInstruction = {0x4c, 0x8d, 0x35, 0x47, 0x3e, 0x2e, 0x04, 0xf3, 0x44, 0x0f, 0x10, 0x3d};
        constexpr std::size_t                   deviceStateRva = 0x5709a40;
        constexpr std::size_t                   deviceStateBytes = NativeDeviceStateCount * sizeof(std::int32_t);
        constexpr std::size_t                   deviceBaseInstructionRva = 0xc099d9;
        constexpr std::array<unsigned char, 7>  deviceBaseInstruction = {0x4c, 0x8d, 0x3d, 0x20, 0x66, 0x3f, 0xff};
        constexpr std::size_t                   deviceLoadInstructionRva = 0xc09a18;
        constexpr std::array<unsigned char, 8>  deviceLoadInstruction = {0x45, 0x8b, 0x8c, 0x87, 0x40, 0x9a, 0x70, 0x05};

        bool CheckImage(std::uintptr_t image, bool requireDeviceState = false) noexcept
        {
            if (image > (std::numeric_limits<std::uintptr_t>::max)() - imageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > imageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != imageSize ||
                nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > imageSize || sectionsBytes > imageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!ReadImage(image, image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            bool instructionSection = false;
            bool padsSection = false;
            bool deviceSection = false;
            bool deviceBaseCodeSection = false;
            bool deviceLoadCodeSection = false;
            for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
            {
                instructionSection |=
                    SectionContains(sections[index], instructionRva, expectedInstruction.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, 0);
                padsSection |= SectionContains(sections[index], padsRva, padsSize, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE);
                deviceSection |=
                    SectionContains(sections[index], deviceStateRva, deviceStateBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE);
                deviceBaseCodeSection |=
                    SectionContains(sections[index], deviceBaseInstructionRva, deviceBaseInstruction.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, 0);
                deviceLoadCodeSection |=
                    SectionContains(sections[index], deviceLoadInstructionRva, deviceLoadInstruction.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, 0);
            }
            return instructionSection && padsSection && (!requireDeviceState || (deviceSection && deviceBaseCodeSection && deviceLoadCodeSection));
        }

        template <std::size_t Size>
        bool CheckCode(std::uintptr_t image, std::size_t rva, const std::array<unsigned char, Size>& expected) noexcept
        {
            std::array<unsigned char, Size> actual;
            return ReadImage(image, image + rva, actual.data(), actual.size(), false, true) && actual == expected;
        }

        bool CheckLayoutInstructions(std::uintptr_t image) noexcept
        {
            // Guard the actual native caller and updater, not just the public
            // signature locating the array. These relative instructions retain
            // their bytes under ASLR and establish both object/state offsets.
            constexpr std::array<unsigned char, 7>  firstPad = {0x48, 0x8d, 0x0d, 0xd7, 0x37, 0x29, 0x04};
            constexpr std::array<unsigned char, 26> dispatch = {0xe8, 0x17, 0x03, 0x00, 0x00, 0xba, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8d, 0x0d,
                                                                0x43, 0x39, 0x29, 0x04, 0x48, 0x83, 0xc4, 0x28, 0xe9, 0x02, 0x03, 0x00, 0x00};
            constexpr std::array<unsigned char, 80> savePrevious = {
                0x0f, 0x10, 0x41, 0x38, 0x49, 0x89, 0x5b, 0x10, 0x4c, 0x8b, 0xf1, 0x0f, 0x10, 0x49, 0x48, 0x49, 0x89, 0x73, 0x18, 0x48,
                0x8d, 0xb1, 0xb4, 0x00, 0x00, 0x00, 0x0f, 0x11, 0x41, 0x68, 0x49, 0x89, 0x7b, 0x20, 0x48, 0x8d, 0xb9, 0xe4, 0x00, 0x00,
                0x00, 0x0f, 0x10, 0x41, 0x58, 0x4d, 0x89, 0x7b, 0xd8, 0x4c, 0x8b, 0xcf, 0x44, 0x0f, 0xb7, 0xfa, 0x41, 0x0f, 0x29, 0x73,
                0xc8, 0x0f, 0x11, 0x49, 0x78, 0x4c, 0x8b, 0xc6, 0x48, 0x8d, 0x54, 0x24, 0x70, 0x0f, 0x11, 0x81, 0x88, 0x00, 0x00, 0x00};
            constexpr std::array<unsigned char, 29> commitCurrent = {0x49, 0x8b, 0xce, 0x0f, 0x10, 0x00, 0x41, 0x0f, 0x11, 0x46, 0x38, 0x0f, 0x10, 0x48, 0x10,
                                                                     0x41, 0x0f, 0x11, 0x4e, 0x48, 0x0f, 0x10, 0x40, 0x20, 0x41, 0x0f, 0x11, 0x46, 0x58};
            // These getter paths independently corroborate the four signed
            // analogue lanes. Other paths can use an action provider instead;
            // matching this layout does not establish all gameplay semantics.
            constexpr std::array<unsigned char, 4> firstAxis = {0x0f, 0xbf, 0x47, 0x38};
            constexpr std::array<unsigned char, 4> secondAxis = {0x0f, 0xbf, 0x47, 0x3a};
            constexpr std::array<unsigned char, 4> thirdAxis = {0x0f, 0xb7, 0x47, 0x3c};
            constexpr std::array<unsigned char, 4> fourthAxis = {0x0f, 0xb7, 0x4f, 0x3e};
            return CheckCode(image, 0x1155032, firstPad) && CheckCode(image, 0x1155044, dispatch) && CheckCode(image, 0x1155387, savePrevious) &&
                   CheckCode(image, 0x115540e, commitCurrent) && CheckCode(image, 0x1157723, firstAxis) && CheckCode(image, 0x1157732, secondAxis) &&
                   CheckCode(image, 0x1157977, thirdAxis) && CheckCode(image, 0x115798d, fourthAxis);
        }

        bool CheckInstruction(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, expectedInstruction.size()> instruction;
            if (!ReadImage(image, image + instructionRva, instruction.data(), instruction.size(), false, true) || instruction != expectedInstruction)
                return false;
            std::int32_t displacement = 0;
            std::memcpy(&displacement, instruction.data() + 3, sizeof(displacement));
            const auto decodedRva = static_cast<std::int64_t>(instructionRva) + 7 + displacement;
            return decodedRva == static_cast<std::int64_t>(padsRva) && CheckLayoutInstructions(image);
        }

        bool CheckDeviceInstructions(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, deviceBaseInstruction.size()> base;
            std::array<unsigned char, deviceLoadInstruction.size()> load;
            if (!ReadImage(image, image + deviceBaseInstructionRva, base.data(), base.size(), false, true) || base != deviceBaseInstruction ||
                !ReadImage(image, image + deviceLoadInstructionRva, load.data(), load.size(), false, true) || load != deviceLoadInstruction)
                return false;
            std::int32_t  baseDisplacement = 0;
            std::uint32_t tableDisplacement = 0;
            std::memcpy(&baseDisplacement, base.data() + 3, sizeof(baseDisplacement));
            std::memcpy(&tableDisplacement, load.data() + 4, sizeof(tableDisplacement));
            // The native helper establishes R15=image through RIP-relative LEA,
            // then reads int32 sources with [R15+RAX*4+tableRva]. Neither native
            // function is called, and no mapping/held semantics are inferred.
            return static_cast<std::int64_t>(deviceBaseInstructionRva) + 7 + baseDisplacement == 0 && tableDisplacement == deviceStateRva;
        }
    }

    bool NativePadReader::Bind() noexcept
    {
        const auto thread = GetCurrentThreadId();
        if (ownerThread.load() && ownerThread.load() != thread)
            return false;
        Reset();
        if (sizeof(void*) != 8 || !CheckExecutableBuild())
            return false;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        // Public array-signature provenance: DK22Pac/plugin-sdk, Zlib,
        // commit15f15b60bbf74c106e1b496ff92c98764abf4605, CPad.cpp/h under
        // plugin_sa_unreal. Native caller/updater/getter code establishes this
        // build's corrected enclosing layout; neither that evidence nor these
        // reads verify every named field, keyboard aggregation or engine writes.
        if (!image || !CheckImage(image) || !CheckInstruction(image) || !ImagePages(image, image + padsRva, padsSize, true))
            return false;
        imageAddress.store(image);
        padAddress.store(image + padsRva);
        ownerThread.store(thread);
        return true;
    }

    bool NativePadReader::Read(std::uint32_t padIndex, CControllerState& current, CControllerState& previous) const noexcept
    {
        if (padIndex >= 2 || !ownerThread.load() || ownerThread.load() != GetCurrentThreadId())
            return false;
        const auto image = imageAddress.load();
        const auto pads = padAddress.load();
        if (!image || pads != image + padsRva || !CheckInstruction(image))
            return false;
        std::array<unsigned char, padsSize> snapshot;
        return ReadImage(image, pads, snapshot.data(), snapshot.size(), true) &&
               CopyNativePadStates(snapshot.data(), snapshot.size(), padIndex, current, previous);
    }

    bool NativePadReader::ReadDeviceState(NativeDeviceState& state) const noexcept
    {
        if (!ownerThread.load() || ownerThread.load() != GetCurrentThreadId())
            return false;
        const auto image = imageAddress.load();
        if (!image || padAddress.load() != image + padsRva || !CheckImage(image, true) || !CheckInstruction(image) || !CheckDeviceInstructions(image))
            return false;
        // This table is in the main image's writable, non-executable .data
        // virtual range, including zero-filled runtime storage. Publish only
        // after a complete OS-checked copy of all 127 raw int32 sources.
        NativeDeviceState next;
        if (!ReadImage(image, image + deviceStateRva, next.sources.data(), deviceStateBytes, true))
            return false;
        state = next;
        return true;
    }

    void NativePadReader::Reset() noexcept
    {
        ownerThread.store(0);
        padAddress.store(0);
        imageAddress.store(0);
    }

    bool NativePadReader::IsBound() const noexcept
    {
        return ownerThread.load() != 0 && imageAddress.load() != 0 && padAddress.load() != 0;
    }
}

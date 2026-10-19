/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read genuine native startup states without lifecycle emulation
 *****************************************************************************/
#include "NativeGameStateReader.h"
#include "NativeGameStateProfile.h"
#include "NativeImageAccess.h"

#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace Profile = NativeGameStateProfile;
        namespace Image = NativeImageAccess;

        template <typename T>
        T Operand(const unsigned char* bytes) noexcept
        {
            T value{};
            std::memcpy(&value, bytes, sizeof(value));
            return value;
        }

        bool Binding(std::uintptr_t image) noexcept
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
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(Profile::stateRva, sizeof(std::uint32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + Profile::stateRva, sizeof(std::uint32_t), true))
                return false;

            // These bounded pins qualify both the numeric dispatch and the
            // independently identified semantic paths. A modified image cannot
            // silently reuse our mapping merely because its state stays in range.
            for (const auto& pin : Profile::pins)
            {
                std::array<unsigned char, 94> actual{};
                if (pin.size > actual.size() ||
                    !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | (pin.code ? IMAGE_SCN_MEM_EXECUTE : 0),
                              IMAGE_SCN_MEM_WRITE | (pin.code ? 0 : IMAGE_SCN_MEM_EXECUTE)) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) || std::memcmp(actual.data(), pin.bytes, pin.size) != 0)
                    return false;
                if (pin.rva == Profile::state_dispatchRva)
                {
                    const auto state = static_cast<std::int64_t>(pin.rva + 6) + Operand<std::int32_t>(actual.data() + 2);
                    const auto base = static_cast<std::int64_t>(pin.rva + 52) + Operand<std::int32_t>(actual.data() + 48);
                    if (state != static_cast<std::int64_t>(Profile::stateRva) || base != 0 || actual[28] != 9 ||
                        Operand<std::uint32_t>(actual.data() + 55) != Profile::state_tableRva)
                        return false;
                }
                if (pin.rva == Profile::state_tableRva)
                {
                    constexpr std::array<std::uint32_t, 4> targets = {0x120a01f, 0x120a12f, 0x120a234, 0x120a32c};
                    for (std::size_t index = 0; index < targets.size(); ++index)
                        if (Operand<std::uint32_t>(actual.data() + (index + 6) * sizeof(std::uint32_t)) != targets[index])
                            return false;
                }
            }
            return true;
        }

        bool Translate(std::uint32_t native, SystemState::Enum& output) noexcept
        {
            // Early native states 0..5 have no established SDK semantics. Do
            // not infer them from matching integer values or from user pause.
            if (native > 9)
                return false;
            switch (native)
            {
                case 6:
                    output = SystemState::GS_INIT_FRONTEND;
                    return true;
                case 7:
                    output = SystemState::GS_FRONTEND;
                    return true;
                case 8:
                    output = SystemState::GS_INIT_PLAYING_GAME;
                    return true;
                case 9:
                    output = SystemState::GS_PLAYING_GAME;
                    return true;
                default:
                    return false;
            }
        }
    }

    Status NativeGameStateReader::Bind() noexcept
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
        // Admission is independent of the current state. This caller owns only
        // subsequent reads, never a native callback or lifecycle-write lease.
        imageAddress.store(image);
        ownerThread.store(thread);
        return Status::Ok;
    }

    Status NativeGameStateReader::Read(SystemState::Enum& output) const noexcept
    {
        const auto owner = ownerThread.load();
        const auto image = imageAddress.load();
        if (!owner || !image)
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        std::uint32_t     first = 0, second = 0;
        SystemState::Enum next;
        if (!Binding(image) || !Image::ReadImage(image, image + Profile::stateRva, &first, sizeof(first), true) ||
            !Image::ReadImage(image, image + Profile::stateRva, &second, sizeof(second), true) || first != second || !Translate(second, next) ||
            !Binding(image))
            return Status::Unsupported;
        // Matching samples qualify the returned value without claiming an
        // engine lock. Failed reads and unproved states leave output untouched.
        output = next;
        return Status::Ok;
    }

    void NativeGameStateReader::Reset() noexcept
    {
        ownerThread.store(0);
        imageAddress.store(0);
    }

    bool NativeGameStateReader::IsBound() const noexcept
    {
        return ownerThread.load() != 0 && imageAddress.load() != 0;
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the current main target independently of camera-row products
 ****************************************************************************/
#include "NativeCameraMainTargetMemory.h"
#include "NativeCameraMainTargetProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace CameraMainTargetDetail
    {
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraMainTargetProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace Image = NativeImageAccess;
        static_assert(sizeof(std::uintptr_t) == Q::TargetBytes && Q::TargetOffset + Q::TargetBytes == Q::CameraBytes);
        bool ImageLayout(std::uintptr_t image) noexcept
        {
            if (sizeof(void*) != 8 || !image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > P::ImageBytes - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != P::ImageBytes || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto                           sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto                           sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (sectionsRva > P::ImageBytes || sectionsBytes > P::ImageBytes - sectionsRva ||
                !Image::ReadImage(image, image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t bytes, DWORD required, DWORD forbidden)
            {
                for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                    if (Image::SectionContains(sections[i], rva, bytes, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(P::CameraRva, Q::CameraBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(P::CrtPointerRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                !contains(P::CameraVtableRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (const auto& pin : P::Pins)
                if (!contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    return false;
            for (const auto& pin : Q::Pins)
                if (!contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    return false;
            for (const auto& group : P::Groups)
                if (!contains(group.slotRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                    !contains(group.tableSlotRva, sizeof(std::uint32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    return false;
            return true;
        }

        bool Profile(std::uintptr_t image) noexcept
        {
            if (!ImageLayout(image))
                return false;
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            for (const auto& group : P::Groups)
            {
                std::uintptr_t entry = 0;
                std::uint32_t  branch = 0;
                if (!Image::ReadImage(image, image + group.slotRva, &entry, sizeof(entry)) || entry != image + group.entryRva ||
                    !Image::ReadImage(image, image + group.tableSlotRva, &branch, sizeof(branch)) || branch != group.branchRva)
                    return false;
            }
            for (const auto& pin : Q::Pins)
            {
                std::array<unsigned char, Q::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            std::uintptr_t initializer = 0;
            // Normalize the relocated CRT pointer. Preferred-base disk bytes
            // would reject the genuine same image when Windows rebases it.
            return Image::ReadImage(image, image + P::CrtPointerRva, &initializer, sizeof(initializer)) && initializer == image + P::CrtWrapperRva &&
                   D::Profile(image);
        }

        bool Storage(std::uintptr_t image) noexcept
        {
            std::uintptr_t vtable = 0;
            // The native constructor owns these static rows. Verify current
            // image storage and camera type without freezing mutable contents
            // or demanding the unrelated matrix/ped/target dependencies.
            return (image + P::CameraRva + Q::TargetOffset) % alignof(std::uintptr_t) == 0 &&
                   Image::ImagePages(image, image + P::CameraRva, Q::CameraBytes, true) &&
                   Image::ReadImage(image, image + P::CameraRva, &vtable, sizeof(vtable), true) && vtable == image + P::CameraVtableRva;
        }

        struct Snapshot
        {
            D::Value       world;
            std::uintptr_t target = 0;
        };
        bool Same(const Snapshot& a, const Snapshot& b) noexcept
        {
            return a.target == b.target && D::Same(a.world, b.world, true);
        }
        ClockResult Sample(std::uintptr_t image, NativeCameraMainTargetMemory::CheckOwner owner, NativeCameraMainTargetMemory::CheckOwner afterApplicationOwner,
                           Snapshot& output) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
                return ClockResult::UnsupportedBuild;
            auto result = afterApplicationOwner();
            if (result != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            if (!Storage(image))
                return ClockResult::BindingChanged;
            Snapshot next;
            if ((result = D::Capture(image, next.world)) != ClockResult::Ok)
                return result;
            if (!Image::ReadImage(image, image + P::CameraRva + Q::TargetOffset, &next.target, sizeof(next.target), true))
                return ClockResult::ReadFailure;
            if ((result = afterApplicationOwner()) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, next.world) || !Profile(image) || !Storage(image))
                return ClockResult::BindingChanged;
            // Even unknown or noncanonical bits remain uninterpreted. Do not
            // dereference them or add them to the current UObject graph.
            output = next;
            return ClockResult::Ok;
        }
    }
}
namespace mta::game_de::NativeCameraMainTargetMemory
{
    ClockResult Read(std::uintptr_t image, CheckOwner owner, CheckOwner afterApplicationOwner, std::uintptr_t& output) noexcept
    {
        namespace C = CameraMainTargetDetail;
        namespace D = NativeStreamingCompletionContext;
        if (!owner || !afterApplicationOwner)
            return ClockResult::NotInitialized;
        if (!D::TryEnter())
            return ClockResult::Busy;
        D::Scope    scope;
        C::Snapshot first, current, confirmed;
        auto        result = C::Sample(image, owner, afterApplicationOwner, first);
        if (result != ClockResult::Ok || (result = C::Sample(image, owner, afterApplicationOwner, current)) != ClockResult::Ok ||
            (result = C::Sample(image, owner, afterApplicationOwner, confirmed)) != ClockResult::Ok)
            return result;
        if (!C::Same(first, current) || !C::Same(current, confirmed))
            return ClockResult::BindingChanged;
        output = confirmed.target;
        return ClockResult::Ok;
    }
}

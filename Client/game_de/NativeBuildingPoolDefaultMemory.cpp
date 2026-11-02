/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the source-qualified BUILDING default on its current owner
 ****************************************************************************/
#include "NativeBuildingPoolDefaultMemory.h"
#include "NativeBuildingPoolDefaultProfile.h"
#include "NativeImageAccess.h"
#include "NativeStreamingCompletionContext.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace BuildingPoolDefaultDetail
    {
        namespace P = NativeBuildingPoolDefaultProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace Image = NativeImageAccess;
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
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            // Qualify the real BUILDING identity source while deliberately not
            // reading its mutable pool header, capacity or object allocations.
            if (!contains(P::PoolRootRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + P::PoolRootRva, sizeof(std::uintptr_t), true) ||
                !contains(P::VtableRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (const auto& pin : P::Pins)
                if (!contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
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
            std::uintptr_t destructor = 0;
            // The relocated native vtable binds the typed constructor to the
            // deleting path for this exact pool; no captured pointer is called.
            return Image::ReadImage(image, image + P::VtableRva, &destructor, sizeof(destructor)) && destructor == image + P::DestructorRva &&
                   D::Profile(image);
        }
        struct Snapshot
        {
            D::Value      world;
            std::uint32_t capacity = 0;
        };
        bool Same(const Snapshot& first, const Snapshot& second) noexcept
        {
            return first.capacity == second.capacity && D::Same(first.world, second.world, true);
        }
        ClockResult Sample(std::uintptr_t image, NativeBuildingPoolDefaultMemory::CheckOwner owner,
                           NativeBuildingPoolDefaultMemory::CheckOwner afterApplicationOwner, Snapshot& output) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
                return ClockResult::UnsupportedBuild;
            auto result = afterApplicationOwner();
            if (result != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            Snapshot next;
            if ((result = D::Capture(image, next.world)) != ClockResult::Ok)
                return result;
            // Read the actual immutable immediate after its complete typed
            // source path matches. A live resized size can never replace it.
            if (!Image::ReadImage(image, image + P::CapacityImmediateRva, &next.capacity, sizeof(next.capacity), false, true))
                return ClockResult::ReadFailure;
            if (next.capacity != P::OriginalCapacity)
                return ClockResult::BindingChanged;
            if ((result = afterApplicationOwner()) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, next.world) || !Profile(image))
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
    }
}
namespace mta::game_de::NativeBuildingPoolDefaultMemory
{
    ClockResult Read(std::uintptr_t image, CheckOwner owner, CheckOwner afterApplicationOwner, std::uint32_t& output) noexcept
    {
        namespace C = BuildingPoolDefaultDetail;
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
        output = confirmed.capacity;
        return ClockResult::Ok;
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One current-bank hierarchy origin qualifier for native observations
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockMemory.h"
#include "NativeAnimationBlockHierarchyProfile.h"
#include "NativeAnimationManagerMemory.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de::NativeAnimationHierarchyOrigin
{
    namespace Hierarchy = NativeAnimationBlockHierarchyProfile;
    namespace Block = NativeAnimationBlockProfile;
    namespace Image = NativeImageAccess;
    using Binding = NativeAnimationBlockMemory::Binding;
    using CheckOwner = NativeAnimationBlockMemory::CheckOwner;
    struct Snapshot
    {
        NativeAnimationManagerCounts extent{};
        std::uint32_t                block = 0, count = 0;
        std::int32_t                 start = 0;
    };
    inline ClockResult Owner(Binding binding, CheckOwner owner) noexcept
    {
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(binding.image)) != ClockResult::Ok)
            return result;
        std::uint32_t state = 0;
        if (!NativeGameStateMemory::Read(binding.image, state))
            return ClockResult::BindingChanged;
        return state == 9 ? owner() : ClockResult::NotInitialized;
    }
    inline bool Sections(Binding binding, std::uintptr_t hierarchyRva, std::uintptr_t blockRva, const NativeAnimationBlockProfile::Pin* extra = nullptr,
                         std::size_t extraCount = 0) noexcept
    {
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
            bytes > NativePedPoolProfile::ImageSize - at || !Image::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
        {
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], rva, size, required, forbidden))
                    return true;
            return false;
        };
        constexpr auto rw = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
        if (!contains(hierarchyRva, Hierarchy::RowBytes, rw, IMAGE_SCN_MEM_EXECUTE) ||
            !Image::ImagePages(binding.image, binding.image + hierarchyRva, Hierarchy::RowBytes, true))
            return false;
        if (blockRva && (!contains(blockRva, Block::RowBytes, rw, IMAGE_SCN_MEM_EXECUTE) ||
                         !Image::ImagePages(binding.image, binding.image + blockRva, Block::RowBytes, true)))
            return false;
        for (const auto& pin : Hierarchy::Code)
            if (!contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                return false;
        for (std::size_t index = 0; index != extraCount; ++index)
            if (!contains(extra[index].rva, extra[index].size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                return false;
        return true;
    }
    inline bool Profile(Binding binding, std::uintptr_t hierarchyRva, std::uintptr_t blockRva) noexcept
    {
        if (!NativeAnimationManagerMemory::Revalidate(binding) || !Sections(binding, hierarchyRva, blockRva))
            return false;
        for (const auto& pin : Hierarchy::Code)
        {
            std::array<unsigned char, Hierarchy::MaximumPinBytes> bytes{};
            if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    template <class Payload>
    struct Observation
    {
        Snapshot origin{};
        Payload  payload{};
    };
    // The payload remains between the same origin bookends. The old
    // gateway callback still observes only its original DWORD hash cell.
    template <class Payload>
    ClockResult Capture(Binding binding, CheckOwner owner, std::uintptr_t ordinal, std::uintptr_t hierarchyRva,
                        ClockResult (*observe)(Binding, std::uintptr_t, Payload&) noexcept, Observation<Payload>& output) noexcept
    {
        NativeAnimationManagerMemory manager(binding, owner);
        Snapshot                     next;
        auto                         result = manager.ReadCounts(next.extent);
        if (result != ClockResult::Ok)
            return result;
        if (next.extent.blocks < 0 || static_cast<std::uint32_t>(next.extent.blocks) > Block::ObservationLimit || next.extent.animations <= 0)
            return ClockResult::BindingChanged;
        const auto highwater = static_cast<std::uint32_t>(next.extent.animations);
        if (ordinal >= highwater)
            return ClockResult::InvalidArgument;
        if (!Profile(binding, hierarchyRva, 0))
            return ClockResult::BindingChanged;
        // A raw numeric locator becomes a typed current hierarchy only
        // through the native loader's owning block and actual range.
        if (!Image::ReadImage(binding.image, binding.image + hierarchyRva + Hierarchy::OwningBlockOffset, &next.block, sizeof(next.block), true))
            return ClockResult::ReadFailure;
        if (next.block >= static_cast<std::uint32_t>(next.extent.blocks))
            return ClockResult::BindingChanged;
        const auto blockRva = Block::BankRva + static_cast<std::uintptr_t>(next.block) * Block::RowBytes;
        if (!Sections(binding, hierarchyRva, blockRva))
            return ClockResult::BindingChanged;
        if (!Image::ReadImage(binding.image, binding.image + blockRva + Block::IdOffset, &next.start, sizeof(next.start), true) ||
            !Image::ReadImage(binding.image, binding.image + blockRva + Block::AnimationCountOffset, &next.count, sizeof(next.count), true))
            return ClockResult::ReadFailure;
        if (next.start < 0 || !next.count || next.count > static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)()))
            return ClockResult::BindingChanged;
        const auto start = static_cast<std::uint32_t>(next.start);
        if (start >= highwater || next.count > highwater - start || ordinal < start || ordinal - start >= next.count)
            return ClockResult::BindingChanged;
        Payload payload;
        if ((result = observe(binding, binding.image + hierarchyRva, payload)) != ClockResult::Ok)
            return result;
        std::uint32_t blockAfter = 0, countAfter = 0;
        std::int32_t  startAfter = 0;
        if (!Image::ReadImage(binding.image, binding.image + hierarchyRva + Hierarchy::OwningBlockOffset, &blockAfter, sizeof(blockAfter), true) ||
            !Image::ReadImage(binding.image, binding.image + blockRva + Block::IdOffset, &startAfter, sizeof(startAfter), true) ||
            !Image::ReadImage(binding.image, binding.image + blockRva + Block::AnimationCountOffset, &countAfter, sizeof(countAfter), true))
            return ClockResult::ReadFailure;
        if (blockAfter != next.block || startAfter != next.start || countAfter != next.count)
            return ClockResult::BindingChanged;
        NativeAnimationManagerCounts after;
        if ((result = manager.ReadCounts(after)) != ClockResult::Ok)
            return result;
        if (next.extent.blocks != after.blocks || next.extent.animations != after.animations || !Profile(binding, hierarchyRva, blockRva))
            return ClockResult::BindingChanged;
        if ((result = Owner(binding, owner)) != ClockResult::Ok)
            return result;
        output.origin = next;
        output.payload = payload;
        return ClockResult::Ok;
    }
    inline bool Same(const Snapshot& a, const Snapshot& b) noexcept
    {
        return a.extent.blocks == b.extent.blocks && a.extent.animations == b.extent.animations && a.block == b.block && a.start == b.start &&
               a.count == b.count;
    }

    struct Location
    {
        std::uintptr_t ordinal = 0, rva = 0;
    };
    inline ClockResult Locate(Binding binding, std::uintptr_t locator, Location& output) noexcept
    {
        namespace Hierarchy = NativeAnimationBlockHierarchyProfile;
        static_assert(Hierarchy::BankRva + Hierarchy::RowBytes <= NativePedPoolProfile::ImageSize);
        // All locator arithmetic is copied and checked before callbacks. The
        // physical image bound is not a guessed native table capacity.
        const auto requested = locator;
        if (binding.image > (std::numeric_limits<std::uintptr_t>::max)() - Hierarchy::BankRva)
            return ClockResult::InvalidArgument;
        const auto base = binding.image + Hierarchy::BankRva;
        if (requested < base)
            return ClockResult::InvalidArgument;
        const auto displacement = requested - base;
        if (displacement % Hierarchy::RowBytes)
            return ClockResult::InvalidArgument;
        const auto ordinal = displacement / Hierarchy::RowBytes;
        if (ordinal > (NativePedPoolProfile::ImageSize - Hierarchy::BankRva - Hierarchy::RowBytes) / Hierarchy::RowBytes ||
            binding.image > (std::numeric_limits<std::uintptr_t>::max)() - Hierarchy::BankRva - ordinal * Hierarchy::RowBytes - Hierarchy::RowBytes)
            return ClockResult::InvalidArgument;
        const auto rva = Hierarchy::BankRva + ordinal * Hierarchy::RowBytes;
        output = {ordinal, rva};
        return ClockResult::Ok;
    }
}

/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reacquire actual row ownership before publishing an opaque address
 ****************************************************************************/
#include "NativeAnimationBlockFactoryMemory.h"
#include "NativeAnimationBlockHierarchyProfile.h"
#include "NativeAnimationManagerMemory.h"
#include "NativeImageAccess.h"
#include "NativeAnimationHierarchyOrigin.h"
#include <array>
#include <cstring>
#include <limits>
#include <vector>
namespace mta::game_de
{
    namespace AnimationBlockFactoryDetail
    {
        namespace Blocks = NativeAnimationBlockProfile;
        namespace Hierarchy = NativeAnimationBlockHierarchyProfile;
        namespace Image = NativeImageAccess;
        bool Terminated(const NativeAnimationBlockName& name) noexcept
        {
            for (const auto value : name)
                if (!value)
                    return true;
            return false;
        }
        bool SameRange(const NativeAnimationBlockValues& first, const NativeAnimationBlockValues& second) noexcept
        {
            return first.index == second.index && first.idOffset == second.idOffset && first.animationCount == second.animationCount && first.loaded &&
                   second.loaded;
        }
        bool HierarchyProfile(NativeAnimationBlockFactoryMemory::Binding binding, std::uintptr_t rowRva) noexcept
        {
            if (!NativeAnimationManagerMemory::Revalidate(binding))
                return false;
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                !Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)))
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                bytes > NativePedPoolProfile::ImageSize - at || !Image::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(rowRva, Hierarchy::RowBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (const auto& pin : Hierarchy::Code)
            {
                std::array<unsigned char, Hierarchy::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        ClockResult RowOwner(NativeAnimationBlockFactoryMemory::Binding binding, std::uintptr_t rva, std::uint32_t& output) noexcept
        {
            if (!HierarchyProfile(binding, rva))
                return ClockResult::BindingChanged;
            std::array<unsigned char, Hierarchy::RowBytes> row{};
            if (!Image::ReadImage(binding.image, binding.image + rva, row.data(), row.size(), true))
                return ClockResult::ReadFailure;
            std::memcpy(&output, row.data() + Hierarchy::OwningBlockOffset, sizeof(output));
            return ClockResult::Ok;
        }
    }
    ClockResult NativeAnimationBlockFactoryMemory::ReadIdentity(std::uint32_t index, NativeAnimationBlockIdentity& output) const noexcept
    {
        namespace Detail = AnimationBlockFactoryDetail;
        if (index >= Detail::Blocks::ObservationLimit)
            return ClockResult::InvalidArgument;
        NativeAnimationManagerMemory manager(binding, owner);
        NativeAnimationBlockMemory   block(binding, owner);
        NativeAnimationManagerCounts first, second;
        auto                         result = manager.ReadCounts(first);
        if (result != ClockResult::Ok)
            return result;
        if (first.blocks < 0 || static_cast<std::uint32_t>(first.blocks) > Detail::Blocks::ObservationLimit)
            return ClockResult::BindingChanged;
        if (index >= static_cast<std::uint32_t>(first.blocks))
            return ClockResult::NotInitialized;
        const auto                   rva = Detail::Blocks::BankRva + static_cast<std::uintptr_t>(index) * Detail::Blocks::RowBytes;
        NativeAnimationBlockIdentity next;
        next.index = index;
        if (!Detail::Image::ReadImage(binding.image, binding.image + rva, next.name.data(), next.name.size(), true))
            return ClockResult::ReadFailure;
        if (!Detail::Terminated(next.name))
            return ClockResult::BindingChanged;
        NativeAnimationBlockValues ignored;
        if ((result = block.Read(next, ignored)) != ClockResult::Ok || (result = manager.ReadCounts(second)) != ClockResult::Ok)
            return result;
        if (first.blocks != second.blocks)
            return ClockResult::BindingChanged;
        NativeAnimationBlockName current{};
        if (!Detail::Image::ReadImage(binding.image, binding.image + rva, current.data(), current.size(), true))
            return ClockResult::ReadFailure;
        if (current != next.name)
            return ClockResult::BindingChanged;
        if ((result = block.Read(next, ignored)) != ClockResult::Ok)
            return result;
        // Use actual row spelling, including case/high bytes and bytes after
        // NUL. A folded lookup query is not this current native identity.
        output = next;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationBlockFactoryMemory::ResolveInterface(std::uintptr_t locator, NativeAnimationBlockIdentity& output) const noexcept
    {
        namespace Blocks = NativeAnimationBlockProfile;
        if (binding.image > (std::numeric_limits<std::uintptr_t>::max)() - Blocks::BankRva)
            return ClockResult::InvalidArgument;
        const auto base = binding.image + Blocks::BankRva;
        if (locator < base)
            return ClockResult::InvalidArgument;
        const auto displacement = locator - base;
        if (displacement % Blocks::RowBytes || displacement / Blocks::RowBytes >= Blocks::ObservationLimit)
            return ClockResult::InvalidArgument;
        // A raw locator supplies an exact current lookup index, never proof of
        // issuance, old SDK lifetime, or permission to adopt foreign storage.
        return ReadIdentity(static_cast<std::uint32_t>(displacement / Blocks::RowBytes), output);
    }
    ClockResult NativeAnimationBlockFactoryMemory::ReadHierarchy(const NativeAnimationBlockIdentity& identity, std::size_t requestedIndex,
                                                                 std::uintptr_t& output) const noexcept
    {
        namespace Detail = AnimationBlockFactoryDetail;
        namespace Hierarchy = NativeAnimationBlockHierarchyProfile;
        const auto                 expected = identity;
        const auto                 ordinal = requestedIndex;
        NativeAnimationBlockMemory block(binding, owner);
        NativeAnimationBlockValues first, second, final;
        auto                       result = block.Read(expected, first);
        if (result != ClockResult::Ok)
            return result;
        // Preserve the actual MTA order. An unloaded block has no hierarchy
        // address even when a caller's index or raw native range is invalid.
        if (!first.loaded)
        {
            output = 0;
            return ClockResult::Ok;
        }
        if (first.idOffset < 0 || first.animationCount > static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)()) || !first.animationCount)
            return ClockResult::BindingChanged;
        if (ordinal >= static_cast<std::size_t>(first.animationCount))
            return ClockResult::InvalidArgument;
        NativeAnimationManagerMemory manager(binding, owner);
        NativeAnimationManagerCounts extent, after;
        if ((result = manager.ReadCounts(extent)) != ClockResult::Ok)
            return result;
        if (extent.animations <= 0)
            return ClockResult::BindingChanged;
        const auto start = static_cast<std::uint32_t>(first.idOffset);
        const auto limit = static_cast<std::uint32_t>(extent.animations);
        if (start >= limit || first.animationCount > limit - start)
            return ClockResult::BindingChanged;
        // Full size_t comparison precedes conversion; the current signed
        // native range now bounds both the addition and the row40 scaling.
        const auto selected = static_cast<std::uintptr_t>(start) + ordinal;
        if (selected > (NativePedPoolProfile::ImageSize - Hierarchy::BankRva - Hierarchy::RowBytes) / Hierarchy::RowBytes)
            return ClockResult::BindingChanged;
        const auto rva = Hierarchy::BankRva + selected * Hierarchy::RowBytes;
        if (binding.image > (std::numeric_limits<std::uintptr_t>::max)() - rva - Hierarchy::RowBytes)
            return ClockResult::BindingChanged;
        std::uint32_t firstOwner = 0, secondOwner = 0;
        if ((result = Detail::RowOwner(binding, rva, firstOwner)) != ClockResult::Ok)
            return result;
        if (firstOwner != expected.index)
            return ClockResult::BindingChanged;
        if ((result = manager.ReadCounts(after)) != ClockResult::Ok || (result = block.Read(expected, second)) != ClockResult::Ok)
            return result;
        if (extent.blocks != after.blocks || extent.animations != after.animations || !Detail::SameRange(first, second))
            return ClockResult::BindingChanged;
        if ((result = Detail::RowOwner(binding, rva, secondOwner)) != ClockResult::Ok || (result = block.Read(expected, final)) != ClockResult::Ok)
            return result;
        if (secondOwner != expected.index || !Detail::SameRange(first, final))
            return ClockResult::BindingChanged;
        // Only the static row address is borrowed. Dynamic sequence/cache
        // state may change; this neither freezes it nor retains it past the
        // current callback or a subsequent native operation/unload/reload.
        output = binding.image + rva;
        return ClockResult::Ok;
    }

    namespace AnimationBlockHashDetail
    {
        namespace Origin = NativeAnimationHierarchyOrigin;
        namespace Hierarchy = NativeAnimationBlockHierarchyProfile;
        using Binding = NativeAnimationBlockFactoryMemory::Binding;
        using CheckOwner = NativeAnimationBlockFactoryMemory::CheckOwner;
        struct Range
        {
            NativeAnimationBlockValues   block{};
            NativeAnimationManagerCounts extent{};
        };
        bool Same(const Range& first, const Range& next) noexcept
        {
            return first.block.index == next.block.index && first.block.idOffset == next.block.idOffset &&
                   first.block.animationCount == next.block.animationCount && first.block.loaded == next.block.loaded &&
                   first.extent.blocks == next.extent.blocks && first.extent.animations == next.extent.animations;
        }
        ClockResult CaptureRange(Binding binding, CheckOwner owner, const NativeAnimationBlockIdentity& expected, Range& output) noexcept
        {
            Range next;
            auto  result = NativeAnimationBlockMemory(binding, owner).Read(expected, next.block);
            if (result != ClockResult::Ok || (result = NativeAnimationManagerMemory(binding, owner).ReadCounts(next.extent)) != ClockResult::Ok)
                return result;
            if (next.extent.blocks < 0 || static_cast<std::uint32_t>(next.extent.blocks) > NativeAnimationBlockProfile::ObservationLimit ||
                expected.index >= static_cast<std::uint32_t>(next.extent.blocks) || next.extent.animations < 0)
                return ClockResult::BindingChanged;
            if (next.block.animationCount)
            {
                if (next.block.idOffset < 0)
                    return ClockResult::BindingChanged;
                const auto start = static_cast<std::uint32_t>(next.block.idOffset);
                const auto highwater = static_cast<std::uint32_t>(next.extent.animations);
                if (start >= highwater || next.block.animationCount > highwater - start)
                    return ClockResult::BindingChanged;
                const auto     last = static_cast<std::uintptr_t>(start) + next.block.animationCount - 1;
                constexpr auto maximum = (NativePedPoolProfile::ImageSize - Hierarchy::BankRva - Hierarchy::RowBytes) / Hierarchy::RowBytes;
                if (last > maximum ||
                    binding.image > (std::numeric_limits<std::uintptr_t>::max)() - Hierarchy::BankRva - last * Hierarchy::RowBytes - Hierarchy::RowBytes)
                    return ClockResult::BindingChanged;
            }
            // Zero count searches no hierarchy cell. No pointer arithmetic or
            // invented loaded-state predicate is necessary to acknowledge it.
            if ((result = Origin::Owner(binding, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        ClockResult ObserveHash(Binding binding, std::uintptr_t row, std::uint32_t& output) noexcept
        {
            return NativeImageAccess::ReadImage(binding.image, row, &output, sizeof(output), true) ? ClockResult::Ok : ClockResult::ReadFailure;
        }
        ClockResult CaptureHash(Binding binding, CheckOwner owner, const NativeAnimationBlockIdentity& expected, const Range& range, std::size_t index,
                                std::uint32_t& output) noexcept
        {
            const auto                         ordinal = static_cast<std::uintptr_t>(range.block.idOffset) + index;
            const auto                         rva = Hierarchy::BankRva + ordinal * Hierarchy::RowBytes;
            Origin::Observation<std::uint32_t> current;
            auto                               result = Origin::Capture(binding, owner, ordinal, rva, &ObserveHash, current);
            if (result != ClockResult::Ok)
                return result;
            if (current.origin.block != expected.index || current.origin.start != range.block.idOffset || current.origin.count != range.block.animationCount ||
                current.origin.extent.blocks != range.extent.blocks || current.origin.extent.animations != range.extent.animations)
                return ClockResult::BindingChanged;
            output = current.payload;
            return ClockResult::Ok;
        }
    }
    ClockResult NativeAnimationBlockFactoryMemory::FindHierarchyByHash(const NativeAnimationBlockIdentity& identity, std::uint32_t requestedHash,
                                                                       std::uintptr_t& output) const noexcept
    {
        namespace Detail = AnimationBlockHashDetail;
        namespace Hierarchy = NativeAnimationBlockHierarchyProfile;
        const auto    expected = identity;
        const auto    hash = requestedHash;
        Detail::Range first, middle, final;
        auto          result = Detail::CaptureRange(binding, owner, expected, first);
        if (result != ClockResult::Ok)
            return result;
        std::vector<std::uint32_t> prefix;
        std::uintptr_t             selected = 0;
        for (std::uint32_t index = 0; index != first.block.animationCount; ++index)
        {
            std::uint32_t current = 0;
            if ((result = Detail::CaptureHash(binding, owner, expected, first, index, current)) != ClockResult::Ok)
                return result;
            // Only the actually searched prefix is retained. This transient
            // CPU allocation is neither a native bank copy nor a new owner.
            // Failure must refuse instead of escaping this noexcept boundary.
            try
            {
                prefix.push_back(current);
            }
            catch (...)
            {
                return ClockResult::WriteFailure;
            }
            if (current == hash)
            {
                const auto ordinal = static_cast<std::uintptr_t>(first.block.idOffset) + index;
                selected = binding.image + Hierarchy::BankRva + ordinal * Hierarchy::RowBytes;
                break;
            }
        }
        if ((result = Detail::CaptureRange(binding, owner, expected, middle)) != ClockResult::Ok)
            return result;
        if (!Detail::Same(first, middle))
            return ClockResult::BindingChanged;
        // Reobserve every searched row, not only the eventual match. A torn
        // earlier match or incomplete miss must never select the wrong facade.
        for (std::size_t index = 0; index != prefix.size(); ++index)
        {
            std::uint32_t current = 0;
            if ((result = Detail::CaptureHash(binding, owner, expected, first, index, current)) != ClockResult::Ok)
                return result;
            if (current != prefix[index])
                return ClockResult::BindingChanged;
        }
        if ((result = Detail::CaptureRange(binding, owner, expected, final)) != ClockResult::Ok)
            return result;
        if (!Detail::Same(first, final))
            return ClockResult::BindingChanged;
        if ((result = Detail::Origin::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // Full uint32 hashes, including0/ffffffff, are values. Only the exact
        // qualified first match or complete no-match result is published.
        output = selected;
        return ClockResult::Ok;
    }
}

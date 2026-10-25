/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native signed counters and genuine folded name selection
 ****************************************************************************/
#include "NativeAnimationManagerMemory.h"
#include "NativeAnimationManagerProfile.h"
#include "NativeAnimationAssocDefinitionProfile.h"
#include "NativeAnimationBlockMemory.h"
#include "NativeAnimationBlockProfile.h"
#include "NativeGameStateMemory.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace AnimationManagerDetail
    {
        namespace P = NativeAnimationManagerProfile;
        namespace B = NativeAnimationBlockProfile;
        namespace I = NativeImageAccess;
        ClockResult Owner(NativeAnimationManagerMemory::Binding binding, NativeAnimationManagerMemory::CheckOwner owner) noexcept
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
        bool Terminated(const NativeAnimationBlockName& name) noexcept
        {
            for (const auto byte : name)
                if (!byte)
                    return true;
            return false;
        }
        bool Sections(const NativeAnimationManagerMemory::Binding& binding, std::uintptr_t nameRva = B::BankRva) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!I::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                !I::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
                nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
                nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                bytes > NativePedPoolProfile::ImageSize - at || !I::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                    if (I::SectionContains(sections[i], rva, size, required, forbidden))
                        return true;
                return false;
            };
            constexpr auto rw = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            constexpr auto rx = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            if (!contains(B::CountRva, sizeof(std::int32_t), rw, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(P::AnimationCountRva, sizeof(std::int32_t), rw, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(nameRva, NativeAnimationBlockName{}.size(), rw, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(P::NameFoldRva, P::NameFold.size(), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (const auto& pin : P::Code)
                if (!contains(pin.rva, pin.size, rx, IMAGE_SCN_MEM_WRITE))
                    return false;
            return true;
        }
        ClockResult Begin(NativeAnimationManagerMemory::Binding binding, NativeAnimationManagerMemory::CheckOwner owner) noexcept
        {
            const auto result = Owner(binding, owner);
            return result != ClockResult::Ok ? result : NativeAnimationManagerMemory::Revalidate(binding) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        ClockResult End(NativeAnimationManagerMemory::Binding binding, NativeAnimationManagerMemory::CheckOwner owner) noexcept
        {
            if (!NativeAnimationManagerMemory::Revalidate(binding))
                return ClockResult::BindingChanged;
            return Owner(binding, owner);
        }
        ClockResult Counts(NativeAnimationManagerMemory::Binding binding, NativeAnimationManagerMemory::CheckOwner owner,
                           NativeAnimationManagerCounts& output) noexcept
        {
            auto result = Begin(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationManagerCounts next;
            if (!I::ReadImage(binding.image, binding.image + B::CountRva, &next.blocks, sizeof(next.blocks), true) ||
                !I::ReadImage(binding.image, binding.image + P::AnimationCountRva, &next.animations, sizeof(next.animations), true))
                return ClockResult::ReadFailure;
            if ((result = End(binding, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        struct Names
        {
            std::int32_t                                              count = 0;
            std::array<NativeAnimationBlockName, B::ObservationLimit> rows{};
        };
        ClockResult Capture(NativeAnimationManagerMemory::Binding binding, NativeAnimationManagerMemory::CheckOwner owner, Names& output) noexcept
        {
            auto result = Begin(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            Names next;
            if (!I::ReadImage(binding.image, binding.image + B::CountRva, &next.count, sizeof(next.count), true))
                return ClockResult::ReadFailure;
            // The genuine leaf returns null for every nonpositive signed count.
            // A larger positive count exceeds our observation budget, not a
            // proved native capacity; it must never become a false "not found".
            if (next.count > static_cast<std::int32_t>(B::ObservationLimit))
                return ClockResult::BindingChanged;
            for (std::int32_t i = 0; i < next.count; ++i)
            {
                const auto rva = B::BankRva + static_cast<std::uintptr_t>(i) * B::RowBytes;
                if (!Sections(binding, rva) ||
                    !I::ReadImage(binding.image, binding.image + rva, next.rows[static_cast<std::size_t>(i)].data(), NativeAnimationBlockName{}.size(), true))
                    return ClockResult::ReadFailure;
            }
            std::int32_t after = 0;
            if (!I::ReadImage(binding.image, binding.image + B::CountRva, &after, sizeof(after), true))
                return ClockResult::ReadFailure;
            if (after != next.count)
                return ClockResult::BindingChanged;
            if ((result = End(binding, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool Equal(const NativeAnimationBlockName& row, const NativeAnimationBlockName& query) noexcept
        {
            for (std::size_t i = 0; i != query.size(); ++i)
            {
                const auto a = row[i], b = query[i];
                if (a == b)
                {
                    if (!a)
                        return true;
                }
                else if (a >= P::NameFold.size() || b >= P::NameFold.size() || P::NameFold[a] != P::NameFold[b])
                    return false;
            }
            return false;
        }
    }
    bool NativeAnimationManagerMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativeAnimationBlockMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativeAnimationManagerMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace D = AnimationManagerDetail;
        namespace P = NativeAnimationManagerProfile;
        if (!NativeAnimationBlockMemory::Revalidate(binding) || !D::Sections(binding))
            return false;
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            if (!pin.size || pin.size > bytes.size() || !D::I::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        std::array<unsigned char, P::NameFold.size()> table{};
        return D::I::ReadImage(binding.image, binding.image + P::NameFoldRva, table.data(), table.size()) && table == P::NameFold;
    }
    ClockResult NativeAnimationManagerMemory::ReadCounts(NativeAnimationManagerCounts& output) const noexcept
    {
        namespace D = AnimationManagerDetail;
        NativeAnimationManagerCounts first, second;
        auto                         result = D::Counts(binding, owner, first);
        if (result != ClockResult::Ok || (result = D::Counts(binding, owner, second)) != ClockResult::Ok)
            return result;
        if (first.blocks != second.blocks || first.animations != second.animations)
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // These are raw signed32 counters, as in the actual SDK getters. The
        // animation counter is a hierarchy-table extent/high-water mark, not
        // the sum of loaded flags and not permission to index that table.
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationManagerMemory::FindBlockIndex(const NativeAnimationBlockName& name, std::int32_t& output) const noexcept
    {
        namespace D = AnimationManagerDetail;
        const auto query = name;
        if (!D::Terminated(query))
            return ClockResult::InvalidArgument;
        D::Names first, second;
        auto     result = D::Capture(binding, owner, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, second)) != ClockResult::Ok)
            return result;
        if (name != query || first.count != second.count || first.rows != second.rows)
            return ClockResult::BindingChanged;
        std::int32_t selected = -1;
        for (std::int32_t i = 0; i < second.count; ++i)
            if (D::Equal(second.rows[static_cast<std::size_t>(i)], query))
            {
                selected = i;
                break;
            }
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        if (name != query)
            return ClockResult::BindingChanged;
        output = selected;
        return ClockResult::Ok;
    }

    namespace AnimationDefinitionDetail
    {
        namespace Definition = NativeAnimationAssocDefinitionProfile;
        namespace Image = NativeImageAccess;
        using Binding = NativeAnimationManagerMemory::Binding;
        using CheckOwner = NativeAnimationManagerMemory::CheckOwner;
        bool Sections(Binding binding, std::uintptr_t rowRva, const NativePedMoveAnimProfile::Pin* extra = nullptr, std::size_t extraCount = 0,
                      bool fold = false) noexcept
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
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            constexpr auto rw = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            constexpr auto rx = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            if (!contains(Definition::CountRva, sizeof(std::int32_t), rw, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(Definition::GroupSlotRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                (rowRva && !contains(rowRva, Definition::RowBytes, rw, IMAGE_SCN_MEM_EXECUTE)))
                return false;
            for (const auto& pin : Definition::Code)
                if (!contains(pin.rva, pin.size, rx, IMAGE_SCN_MEM_WRITE))
                    return false;
            for (std::size_t index = 0; index != extraCount; ++index)
                if (!contains(extra[index].rva, extra[index].size, rx, IMAGE_SCN_MEM_WRITE))
                    return false;
            return !fold || contains(NativeAnimationManagerProfile::NameFoldRva, NativeAnimationManagerProfile::NameFold.size(), IMAGE_SCN_MEM_READ,
                                     IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE);
        }
        bool Profile(Binding binding, std::uintptr_t rowRva = 0) noexcept
        {
            if (!NativePedPoolReader::Revalidate(binding) || !Sections(binding, rowRva))
                return false;
            for (const auto& pin : Definition::Code)
            {
                std::array<unsigned char, Definition::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            std::uintptr_t group = 0;
            return Image::ReadImage(binding.image, binding.image + Definition::GroupSlotRva, &group, sizeof(group)) &&
                   group == binding.image + Definition::GroupRva;
        }
        ClockResult Begin(Binding binding, CheckOwner owner, std::uintptr_t rowRva = 0) noexcept
        {
            const auto result = AnimationManagerDetail::Owner(binding, owner);
            return result != ClockResult::Ok ? result : Profile(binding, rowRva) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        struct NameObservation
        {
            std::int32_t                                    count = 0;
            std::uintptr_t                                  rva = 0;
            std::array<unsigned char, Definition::RowBytes> bytes{};
            std::size_t                                     length = 0;
        };
        ClockResult CaptureName(Binding binding, CheckOwner owner, std::uint32_t index, NameObservation& output) noexcept
        {
            auto result = Begin(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            NameObservation next;
            if (!Image::ReadImage(binding.image, binding.image + Definition::CountRva, &next.count, sizeof(next.count), true))
                return ClockResult::ReadFailure;
            if (next.count <= 0)
                return ClockResult::NotInitialized;
            constexpr auto maximum = (NativePedPoolProfile::ImageSize - Definition::BankRva - Definition::RowBytes) / Definition::RowBytes;
            // The physical image is a safety bound, not a guessed definition
            // capacity. The raw count getter above keeps every signed bit.
            if (static_cast<std::uint32_t>(next.count) > maximum + 1)
                return ClockResult::BindingChanged;
            if (index >= static_cast<std::uint32_t>(next.count))
                return ClockResult::InvalidArgument;
            if (index > maximum)
                return ClockResult::BindingChanged;
            next.rva = Definition::BankRva + static_cast<std::uintptr_t>(index) * Definition::RowBytes;
            if (binding.image > (std::numeric_limits<std::uintptr_t>::max)() - next.rva - Definition::RowBytes || !Profile(binding, next.rva))
                return ClockResult::BindingChanged;
            if (!Image::ReadImage(binding.image, binding.image + next.rva, next.bytes.data(), next.bytes.size(), true))
                return ClockResult::ReadFailure;
            while (next.length != next.bytes.size() && next.bytes[next.length])
                ++next.length;
            // Only the exact known row span is borrowed. Do not invent an SA
            // name-field size or read a string across another native row.
            if (next.length == next.bytes.size())
                return ClockResult::BindingChanged;
            std::int32_t after = 0;
            if (!Image::ReadImage(binding.image, binding.image + Definition::CountRva, &after, sizeof(after), true))
                return ClockResult::ReadFailure;
            if (after != next.count)
                return ClockResult::BindingChanged;
            if ((result = Begin(binding, owner, next.rva)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
    }
    ClockResult NativeAnimationManagerMemory::ReadDefinitionCount(std::int32_t& output) const noexcept
    {
        namespace Detail = AnimationDefinitionDetail;
        auto result = Detail::Begin(binding, owner);
        if (result != ClockResult::Ok)
            return result;
        std::int32_t first = 0, second = 0;
        if (!Detail::Image::ReadImage(binding.image, binding.image + Detail::Definition::CountRva, &first, sizeof(first), true))
            return ClockResult::ReadFailure;
        if ((result = Detail::Begin(binding, owner)) != ClockResult::Ok)
            return result;
        if (!Detail::Image::ReadImage(binding.image, binding.image + Detail::Definition::CountRva, &second, sizeof(second), true))
            return ClockResult::ReadFailure;
        if (first != second)
            return ClockResult::BindingChanged;
        if ((result = Detail::Begin(binding, owner)) != ClockResult::Ok)
            return result;
        // Actual MTA returns a raw signed32 scalar: negative/zero are observed
        // values, not a name-range prerequisite or an invented missing state.
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationManagerMemory::ReadGroupName(std::uint32_t requestedIndex, std::uintptr_t& output) const noexcept
    {
        namespace Detail = AnimationDefinitionDetail;
        const auto              index = requestedIndex;
        Detail::NameObservation first, second;
        auto                    result = Detail::CaptureName(binding, owner, index, first);
        if (result != ClockResult::Ok || (result = Detail::CaptureName(binding, owner, index, second)) != ClockResult::Ok)
            return result;
        if (first.count != second.count || first.rva != second.rva || first.length != second.length ||
            std::memcmp(first.bytes.data(), second.bytes.data(), first.length + 1))
            return ClockResult::BindingChanged;
        if ((result = Detail::Begin(binding, owner, second.rva)) != ClockResult::Ok)
            return result;
        // This is the current row's native NUL string, not a CPU string cache
        // or an association/group/block facade with new native lifetime.
        output = binding.image + second.rva;
        return ClockResult::Ok;
    }

    namespace AnimationDefinitionBlockNameDetail
    {
        namespace Definition = NativeAnimationAssocDefinitionProfile;
        namespace Image = NativeImageAccess;
        using Binding = NativeAnimationManagerMemory::Binding;
        using CheckOwner = NativeAnimationManagerMemory::CheckOwner;
        bool Profile(Binding binding, std::uintptr_t rowRva, bool fold) noexcept
        {
            if (!AnimationDefinitionDetail::Profile(binding, rowRva) ||
                !AnimationDefinitionDetail::Sections(binding, rowRva, Definition::BlockNameCode.data(), Definition::BlockNameCode.size(), fold))
                return false;
            for (const auto& pin : Definition::BlockNameCode)
            {
                std::array<unsigned char, Definition::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            if (fold)
            {
                std::array<unsigned char, NativeAnimationManagerProfile::NameFold.size()> table{};
                if (!Image::ReadImage(binding.image, binding.image + NativeAnimationManagerProfile::NameFoldRva, table.data(), table.size()) ||
                    table != NativeAnimationManagerProfile::NameFold)
                    return false;
            }
            return true;
        }
        ClockResult Begin(Binding binding, CheckOwner owner, std::uintptr_t rowRva = 0, bool fold = false) noexcept
        {
            const auto result = AnimationManagerDetail::Owner(binding, owner);
            return result != ClockResult::Ok ? result : Profile(binding, rowRva, fold) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        struct Name
        {
            std::uintptr_t                                        rva = 0;
            std::size_t                                           length = 0;
            std::array<unsigned char, Definition::BlockNameBytes> bytes{};
            std::int32_t                                          count = 0;
            std::uint32_t                                         reserved = 0;
        };
        ClockResult Capture(Binding binding, CheckOwner owner, std::uint32_t index, bool currentExtent, Name& output) noexcept
        {
            auto result = Begin(binding, owner, 0, !currentExtent);
            if (result != ClockResult::Ok)
                return result;
            Name           next;
            constexpr auto maximum = (NativePedPoolProfile::ImageSize - Definition::BankRva - Definition::RowBytes) / Definition::RowBytes;
            if (currentExtent)
            {
                if (!Image::ReadImage(binding.image, binding.image + Definition::CountRva, &next.count, sizeof(next.count), true))
                    return ClockResult::ReadFailure;
                if (next.count <= 0)
                    return ClockResult::NotInitialized;
                if (static_cast<std::uint32_t>(next.count) > maximum + 1)
                    return ClockResult::BindingChanged;
                if (index >= static_cast<std::uint32_t>(next.count))
                    return ClockResult::InvalidArgument;
            }
            else if (index >= Definition::BlockNamePrefixCount)
                return ClockResult::InvalidArgument;
            if (index > maximum)
                return ClockResult::BindingChanged;
            next.rva = Definition::BankRva + static_cast<std::uintptr_t>(index) * Definition::RowBytes;
            if (binding.image > (std::numeric_limits<std::uintptr_t>::max)() - next.rva - Definition::RowBytes || !Profile(binding, next.rva, !currentExtent))
                return ClockResult::BindingChanged;
            if (!Image::ReadImage(binding.image, binding.image + next.rva + Definition::BlockNameOffset, next.bytes.data(), next.bytes.size(), true))
                return ClockResult::ReadFailure;
            while (next.length != next.bytes.size() && next.bytes[next.length])
                ++next.length;
            if (next.length == next.bytes.size())
                return ClockResult::BindingChanged;
            if (currentExtent)
            {
                std::int32_t after = 0;
                if (!Image::ReadImage(binding.image, binding.image + Definition::CountRva, &after, sizeof(after), true))
                    return ClockResult::ReadFailure;
                if (after != next.count)
                    return ClockResult::BindingChanged;
            }
            if ((result = Begin(binding, owner, next.rva, !currentExtent)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        ClockResult CapturePrefix(Binding binding, std::uint32_t index, Name& output) noexcept
        {
            if (index >= Definition::BlockNamePrefixCount)
                return ClockResult::InvalidArgument;
            constexpr auto last = Definition::BankRva + (Definition::BlockNamePrefixCount - 1) * Definition::RowBytes;
            static_assert(last + Definition::RowBytes <= NativePedPoolProfile::ImageSize);
            Name next;
            next.rva = Definition::BankRva + static_cast<std::uintptr_t>(index) * Definition::RowBytes;
            if (binding.image > (std::numeric_limits<std::uintptr_t>::max)() - next.rva - Definition::RowBytes ||
                !AnimationDefinitionDetail::Sections(binding, next.rva, Definition::BlockNameCode.data(), Definition::BlockNameCode.size(), true))
                return ClockResult::BindingChanged;
            // Code/fold/owner surround the whole finite prefix pass. Each
            // actual row still requires genuine PE/page origin independently;
            // no mutable current-count predicate is imported into FF1070.
            if (!Image::ReadImage(binding.image, binding.image + next.rva + Definition::BlockNameOffset, next.bytes.data(), next.bytes.size(), true))
                return ClockResult::ReadFailure;
            while (next.length != next.bytes.size() && next.bytes[next.length])
                ++next.length;
            if (next.length == next.bytes.size())
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
        bool Same(const Name& first, const Name& second) noexcept
        {
            return first.rva == second.rva && first.length == second.length && first.count == second.count &&
                   !std::memcmp(first.bytes.data(), second.bytes.data(), first.length + 1);
        }
        bool Equal(const Name& name, const std::string& query) noexcept
        {
            for (std::size_t index = 0; index <= name.length; ++index)
            {
                const auto row = name.bytes[index];
                const auto source = index < query.size() ? static_cast<unsigned char>(query[index]) : static_cast<unsigned char>(0);
                if (row == source)
                {
                    if (!row)
                        return true;
                    continue;
                }
                // Saved FF1070 folds only unequal seven-bit bytes. High
                // bytes compare equal only through the exact-byte branch.
                if ((row | source) & 0x80)
                    return false;
                if (NativeAnimationManagerProfile::NameFold[row] != NativeAnimationManagerProfile::NameFold[source])
                    return false;
            }
            return false;
        }
    }
    ClockResult NativeAnimationManagerMemory::ReadGroupBlockName(std::uint32_t requestedIndex, std::uintptr_t& output) const noexcept
    {
        namespace Detail = AnimationDefinitionBlockNameDetail;
        const auto   index = requestedIndex;
        Detail::Name first, second;
        auto         result = Detail::Capture(binding, owner, index, true, first);
        if (result != ClockResult::Ok || (result = Detail::Capture(binding, owner, index, true, second)) != ClockResult::Ok)
            return result;
        if (!Detail::Same(first, second))
            return ClockResult::BindingChanged;
        if ((result = Detail::Begin(binding, owner, second.rva)) != ClockResult::Ok)
            return result;
        output = binding.image + second.rva + Detail::Definition::BlockNameOffset;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationManagerMemory::FindFirstGroupByBlockName(const std::string& sourceName, std::uint32_t& output) const noexcept
    {
        namespace Detail = AnimationDefinitionBlockNameDetail;
        std::string query;
        try
        {
            query = sourceName;
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        // This bounded stack is the exact genuine118-row prefix, not a
        // guessed association count/capacity or a new global registry.
        auto result = Detail::Begin(binding, owner, 0, true);
        if (result != ClockResult::Ok)
            return result;
        std::array<Detail::Name, Detail::Definition::BlockNamePrefixCount> prefix{};
        std::size_t                                                        observed = 0;
        std::uint32_t                                                      selected = Detail::Definition::BlockNamePrefixCount;
        for (std::uint32_t index = 0; index != Detail::Definition::BlockNamePrefixCount; ++index)
        {
            if ((result = Detail::CapturePrefix(binding, index, prefix[index])) != ClockResult::Ok)
                return result;
            ++observed;
            if (Detail::Equal(prefix[index], query))
            {
                selected = index;
                break;
            }
        }
        if ((result = Detail::Begin(binding, owner, 0, true)) != ClockResult::Ok)
            return result;
        for (std::size_t index = 0; index != observed; ++index)
        {
            Detail::Name current;
            if ((result = Detail::CapturePrefix(binding, static_cast<std::uint32_t>(index), current)) != ClockResult::Ok)
                return result;
            if (!Detail::Same(prefix[index], current))
                return ClockResult::BindingChanged;
        }
        if ((result = Detail::Begin(binding, owner, 0, true)) != ClockResult::Ok)
            return result;
        output = selected;
        return ClockResult::Ok;
    }
}

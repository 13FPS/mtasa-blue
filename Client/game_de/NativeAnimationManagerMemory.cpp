/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native signed counters and genuine folded name selection
 ****************************************************************************/
#include "NativeAnimationManagerMemory.h"
#include "NativeAnimationManagerProfile.h"
#include "NativeAnimationBlockMemory.h"
#include "NativeAnimationBlockProfile.h"
#include "NativeGameStateMemory.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include <array>
#include <cstring>
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
}

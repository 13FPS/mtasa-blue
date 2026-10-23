/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh animation block/index/name observations within the real owner
 ****************************************************************************/
#include "NativeAnimationBlockMemory.h"
#include "NativeAnimationBlockProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <array>
#include <cstring>
namespace mta::game_de
{
    namespace AnimationBlockDetail
    {
        namespace P = NativeAnimationBlockProfile;
        namespace I = NativeImageAccess;
        bool Terminated(const NativeAnimationBlockName& name) noexcept
        {
            for (const auto byte : name)
                if (!byte)
                    return true;
            return false;
        }
        ClockResult Owner(NativeAnimationBlockMemory::Binding binding, NativeAnimationBlockMemory::CheckOwner owner) noexcept
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
        bool Sections(const NativeAnimationBlockMemory::Binding& binding, std::uintptr_t rowRva = P::BankRva) noexcept
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
            if (!contains(P::CountRva, 4, rw, IMAGE_SCN_MEM_EXECUTE) || !contains(rowRva, P::RowBytes, rw, IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (const auto& pin : P::Code)
                if (!contains(pin.rva, pin.size, rx, IMAGE_SCN_MEM_WRITE))
                    return false;
            return contains(P::GroupSlotRva, 8, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) &&
                   contains(P::RemapRva + P::LoadedOpcodeIndex, 1, rx, IMAGE_SCN_MEM_WRITE) &&
                   contains(P::DispatchRva + P::LoadedSelector * 4, 4, rx, IMAGE_SCN_MEM_WRITE);
        }
        struct Snapshot
        {
            std::int32_t                           count = 0;
            std::array<unsigned char, P::RowBytes> row{};
        };
        ClockResult Capture(NativeAnimationBlockMemory::Binding binding, NativeAnimationBlockMemory::CheckOwner owner,
                            const NativeAnimationBlockIdentity& identity, Snapshot& output) noexcept
        {
            auto result = Owner(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!NativeAnimationBlockMemory::Revalidate(binding))
                return ClockResult::BindingChanged;
            Snapshot next;
            if (!I::ReadImage(binding.image, binding.image + P::CountRva, &next.count, sizeof(next.count), true))
                return ClockResult::ReadFailure;
            if (next.count < 0 || static_cast<std::uint32_t>(next.count) > P::ObservationLimit)
                return ClockResult::BindingChanged;
            if (identity.index >= static_cast<std::uint32_t>(next.count))
                return ClockResult::NotInitialized;
            const auto rva = P::BankRva + static_cast<std::uintptr_t>(identity.index) * P::RowBytes;
            // Read only the selected current row. The conservative work limit
            // is not permission to infer the native allocation's full capacity.
            if (!Sections(binding, rva) || !I::ReadImage(binding.image, binding.image + rva, next.row.data(), next.row.size(), true))
                return ClockResult::ReadFailure;
            if (std::memcmp(next.row.data(), identity.name.data(), identity.name.size()))
                return ClockResult::BindingChanged;
            std::int32_t countAfter = 0;
            if (!I::ReadImage(binding.image, binding.image + P::CountRva, &countAfter, sizeof(countAfter), true))
                return ClockResult::ReadFailure;
            if (countAfter != next.count || !NativeAnimationBlockMemory::Revalidate(binding))
                return ClockResult::BindingChanged;
            if ((result = Owner(binding, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
    }
    bool NativeAnimationBlockMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativeAnimationBlockMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace D = AnimationBlockDetail;
        namespace P = NativeAnimationBlockProfile;
        if (!NativePedPoolReader::Revalidate(binding) || !D::Sections(binding))
            return false;
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            if (!pin.size || pin.size > bytes.size() || !D::I::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        std::uintptr_t group = 0;
        std::uint8_t   selector = 0;
        std::uint32_t  branch = 0;
        return D::I::ReadImage(binding.image, binding.image + P::GroupSlotRva, &group, sizeof(group)) && group == binding.image + P::GroupRva &&
               D::I::ReadImage(binding.image, binding.image + P::RemapRva + P::LoadedOpcodeIndex, &selector, sizeof(selector), false, true) &&
               selector == P::LoadedSelector &&
               D::I::ReadImage(binding.image, binding.image + P::DispatchRva + P::LoadedSelector * 4, &branch, sizeof(branch), false, true) &&
               branch == P::LoadedBranchRva;
    }
    ClockResult NativeAnimationBlockMemory::Read(const NativeAnimationBlockIdentity& identity, NativeAnimationBlockValues& output) const noexcept
    {
        namespace D = AnimationBlockDetail;
        namespace P = NativeAnimationBlockProfile;
        if (identity.index >= P::ObservationLimit || !D::Terminated(identity.name))
            return ClockResult::InvalidArgument;
        D::Snapshot first, second;
        auto        result = D::Capture(binding, owner, identity, first);
        if (result != ClockResult::Ok)
            return result;
        if ((result = D::Capture(binding, owner, identity, second)) != ClockResult::Ok)
            return result;
        if (first.count != second.count || first.row != second.row)
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        NativeAnimationBlockValues next;
        next.index = identity.index;
        next.loaded = second.row[P::LoadedOffset];
        std::memcpy(&next.idOffset, second.row.data() + P::IdOffset, sizeof(next.idOffset));
        std::memcpy(&next.animationCount, second.row.data() + P::AnimationCountOffset, sizeof(next.animationCount));
        // Classic nAnimations is an unsigned32 storage field. Preserve its full
        // bits; the native loader's signed loop guard does not sanitize this SDK
        // observation or authorize a hierarchy pointer for the returned count.
        output = next;
        return ClockResult::Ok;
    }
}

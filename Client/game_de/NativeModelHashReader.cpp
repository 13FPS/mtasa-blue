/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Stable qualifying prefixes for the actual ascending hash lookup
 ****************************************************************************/
#include "NativeModelHashReader.h"
#include "NativeModelHashProfile.h"
#include "NativeImageAccess.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>
namespace mta::game_de
{
    namespace ModelHashDetail
    {
        namespace P = NativeModelHashProfile;
        namespace Bank = NativeModelBankProfile;
        struct Entry
        {
            NativeModelBankReader::Value model{};
            std::uint32_t                hash = 0;
        };
        bool MetadataPrefix(std::uintptr_t address) noexcept
        {
            // Use the existing ModelObjectReader metadata-readable policy,
            // restricted to the independent hash field's minimal extent.
            // Private/mapped/image pages grant reads, never native ownership.
            if (!address || address % alignof(std::uintptr_t) || P::PrefixBytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + P::PrefixBytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED && page.Type != MEM_IMAGE) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto protection = page.Protect & 0xff;
                if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress), current = reinterpret_cast<std::uintptr_t>(page.AllocationBase);
                if (!current || (allocation && current != allocation) || base > address ||
                    page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + page.RegionSize <= address)
                    return false;
                allocation = current;
                address = (std::min)(end, base + page.RegionSize);
            }
            return true;
        }
        ClockResult Read(NativeModelHashReader::Binding binding, std::uint32_t index, Entry& output) noexcept
        {
            Entry next;
            next.model = {index, binding.image + Bank::BankRva + static_cast<std::uintptr_t>(index) * Bank::SlotBytes, 0};
            if (!NativeImageAccess::ReadImage(binding.image, next.model.slotAddress, &next.model.nativeModelInfo, sizeof(next.model.nativeModelInfo), true))
                return ClockResult::ReadFailure;
            if (next.model.nativeModelInfo)
            {
                SIZE_T copied = 0;
                if (!MetadataPrefix(next.model.nativeModelInfo) ||
                    !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(next.model.nativeModelInfo + P::HashOffset), &next.hash, sizeof(next.hash),
                                       &copied) ||
                    copied != sizeof(next.hash))
                    return ClockResult::ReadFailure;
            }
            output = next;
            return ClockResult::Ok;
        }
        bool Same(const Entry& a, const Entry& b) noexcept
        {
            return a.model.modelIndex == b.model.modelIndex && a.model.slotAddress == b.model.slotAddress &&
                   a.model.nativeModelInfo == b.model.nativeModelInfo && a.hash == b.hash;
        }
    }
    bool NativeModelHashReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativeModelBankReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativeModelHashReader::Revalidate(const Binding& binding) noexcept
    {
        if (!NativeModelBankReader::Revalidate(binding))
            return false;
        for (const auto& pin : NativeModelHashProfile::Code)
        {
            std::array<unsigned char, NativeModelHashProfile::MaximumPinBytes> bytes{};
            if (!pin.size || pin.size > bytes.size() ||
                !NativeImageAccess::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativeModelHashReader::FindFirstByHash(std::uint32_t requestedHash, Value& output) const noexcept
    {
        namespace D = ModelHashDetail;
        if (!owner)
            return ClockResult::NotInitialized;
        const auto            hash = requestedHash;
        std::vector<D::Entry> prefix;
        try
        {
            // Reserve the real finite bank bound before any traversal. No CPU
            // allocation failure can turn an incomplete scan into a miss.
            prefix.resize(NativeModelBankProfile::ModelCount);
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        NativeModelBankReader        bank(binding, owner);
        NativeModelBankReader::Value anchor;
        auto                         result = bank.Read(0, anchor);
        if (result != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        Value       next;
        std::size_t count = 0;
        for (std::uint32_t index = 0; index < NativeModelBankProfile::ModelCount; ++index)
        {
            auto& entry = prefix[index];
            if ((result = D::Read(binding, index, entry)) != ClockResult::Ok)
                return result;
            if (!index && (entry.model.slotAddress != anchor.slotAddress || entry.model.nativeModelInfo != anchor.nativeModelInfo))
                return ClockResult::BindingChanged;
            ++count;
            if (entry.model.nativeModelInfo && entry.hash == hash)
            {
                next.found = true;
                next.model = entry.model;
                next.hash = entry.hash;
                break;
            }
        }
        if ((result = bank.Recheck(anchor)) != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        // Re-admit every inspected slot and nonnull hash in the qualifying
        // prefix. Rechecking only a matching row could miss an earlier model
        // appearing or changing its key, violating source first-match order.
        for (std::size_t index = 0; index < count; ++index)
        {
            D::Entry confirmed;
            if ((result = D::Read(binding, static_cast<std::uint32_t>(index), confirmed)) != ClockResult::Ok)
                return result;
            if (!D::Same(prefix[index], confirmed))
                return ClockResult::BindingChanged;
        }
        if (next.found && (result = bank.Recheck(next.model)) != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = bank.Recheck(anchor)) != ClockResult::Ok)
            return result;
        // Success/miss is an immediate same-callback observation. No name
        // cursor, model-type filter, vtable call or retained lease is implied.
        output = next;
        return ClockResult::Ok;
    }
}

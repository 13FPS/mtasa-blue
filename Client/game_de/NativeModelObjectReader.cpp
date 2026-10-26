/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bookend raw model tags without acquiring render or allocation rights
 ****************************************************************************/
#include "NativeModelObjectReader.h"
#include "NativeModelObjectProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace ModelObjectDetail
    {
        using Reader = NativeModelObjectReader;
        namespace P = NativeModelObjectProfile;
        ClockResult Owner(Reader::Binding binding, Reader::CheckOwner owner) noexcept
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
        bool Prefix(std::uintptr_t address) noexcept
        {
            if (!address || address % alignof(std::uintptr_t) || P::ModelPrefixBytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + P::ModelPrefixBytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    // Bank metadata may be static image data or dynamically
                    // allocated. Readability does not confer allocation rights.
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
        bool RenderField(std::uintptr_t receiver, std::uintptr_t& output) noexcept
        {
            SIZE_T         copied = 0;
            std::uintptr_t next = 0;
            if (!Prefix(receiver) ||
                !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(receiver + P::RenderOffset), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
    }
    bool NativeModelObjectReader::Qualify(Binding& output) noexcept
    {
        return NativeModelBankReader::Qualify(output);
    }
    bool NativeModelObjectReader::Revalidate(const Binding& binding) noexcept
    {
        return NativeModelBankReader::Revalidate(binding);
    }
    bool NativeModelObjectReader::RevalidateRender(const Binding& binding) noexcept
    {
        if (!Revalidate(binding))
            return false;
        for (const auto& pin : NativeModelObjectProfile::Code)
        {
            std::array<unsigned char, NativeModelObjectProfile::MaximumPinBytes> bytes{};
            if (!pin.size || pin.size > bytes.size() ||
                !NativeImageAccess::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativeModelObjectReader::ReadInterface(std::uint32_t model, std::uintptr_t& output) const noexcept
    {
        NativeModelBankReader        bank(binding, owner);
        NativeModelBankReader::Value word;
        auto                         result = bank.Read(model, word);
        if (result != ClockResult::Ok || (result = bank.Recheck(word)) != ClockResult::Ok)
            return result;
        // This operation copies only the bank word, including null/poison.
        // Do not require render pages, a vtable, loaded state or provider rows.
        output = word.nativeModelInfo;
        return ClockResult::Ok;
    }
    ClockResult NativeModelObjectReader::ReadRender(std::uint32_t model, std::uintptr_t& output) const noexcept
    {
        namespace D = ModelObjectDetail;
        NativeModelBankReader        bank(binding, owner);
        NativeModelBankReader::Value word;
        auto                         result = bank.Read(model, word);
        if (result != ClockResult::Ok)
            return result;
        if (!word.nativeModelInfo)
        {
            if ((result = bank.Recheck(word)) != ClockResult::Ok)
                return result;
            output = 0;
            return ClockResult::Ok;
        }
        if (!RevalidateRender(binding))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        std::uintptr_t first = 0, second = 0;
        if (!D::RenderField(word.nativeModelInfo, first))
            return ClockResult::ReadFailure;
        if ((result = bank.Recheck(word)) != ClockResult::Ok || (result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        if (!D::RenderField(word.nativeModelInfo, second))
            return ClockResult::ReadFailure;
        if (first != second)
            return ClockResult::BindingChanged;
        if ((result = bank.Recheck(word)) != ClockResult::Ok)
            return result;
        if (!RevalidateRender(binding))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // Only the current receiver prefix and field were accessed. The raw
        // render value itself is never queried, cast to a schema, or retained.
        output = second;
        return ClockResult::Ok;
    }
}

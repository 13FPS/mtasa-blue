/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bookend a qualified current model animation index without a vcall
 ****************************************************************************/
#include "NativeModelAnimationReader.h"
#include "NativeModelAnimationProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace ModelAnimationDetail
    {
        namespace P = NativeModelAnimationProfile;
        namespace Image = NativeImageAccess;
        using Reader = NativeModelAnimationReader;
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
        bool Span(std::uintptr_t address, std::size_t size) noexcept
        {
            if (!address || size > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + size;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED && page.Type != MEM_IMAGE) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto access = page.Protect & 0xff;
                if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY)
                    return false;
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress), current = reinterpret_cast<std::uintptr_t>(page.AllocationBase);
                if (!current || (allocation && allocation != current) || begin > address ||
                    page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                    return false;
                allocation = current;
                address = (std::min)(end, begin + page.RegionSize);
            }
            return true;
        }
        bool Copy(std::uintptr_t address, void* output, std::size_t size) noexcept
        {
            SIZE_T copied = 0;
            return Span(address, size) && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), output, size, &copied) && copied == size;
        }
        bool ReadOnly(std::uintptr_t image, std::uintptr_t address, std::size_t size) noexcept
        {
            if (!Image::ImagePages(image, address, size))
                return false;
            const auto end = address + size;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.Protect != PAGE_READONLY)
                    return false;
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                    return false;
                address = (std::min)(end, begin + page.RegionSize);
            }
            return true;
        }
        struct Value
        {
            std::uintptr_t vtable = 0, method = 0;
            std::uint64_t  raw = 0;
            bool           operator==(const Value& b) const noexcept { return vtable == b.vtable && method == b.method && raw == b.raw; }
        };
        ClockResult Capture(Reader::Binding binding, std::uintptr_t model, Value& output) noexcept
        {
            Value next;
            if (model % alignof(std::uintptr_t) || !Span(model, P::ReceiverBytes) || !Copy(model, &next.vtable, sizeof(next.vtable)))
                return ClockResult::ReadFailure;
            if (!next.vtable || next.vtable % alignof(std::uintptr_t) ||
                P::GetterSlot + sizeof(std::uintptr_t) > (std::numeric_limits<std::uintptr_t>::max)() - next.vtable ||
                !ReadOnly(binding.image, next.vtable + P::GetterSlot, sizeof(next.method)) ||
                !Image::ReadImage(binding.image, next.vtable + P::GetterSlot, &next.method, sizeof(next.method)))
                return ClockResult::BindingChanged;
            // A different current native class may share this exact inherited
            // leaf. Qualify the actual slot/body, never infer IsVehicle/type6.
            if (next.method != binding.image + P::GetterRva)
                return ClockResult::UnsupportedBuild;
            if (!Copy(model + P::ValueOffset, &next.raw, sizeof(next.raw)))
                return ClockResult::ReadFailure;
            output = next;
            return ClockResult::Ok;
        }
    }
    bool NativeModelAnimationReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativeModelBankReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativeModelAnimationReader::Revalidate(const Binding& binding) noexcept
    {
        namespace D = ModelAnimationDetail;
        namespace P = NativeModelAnimationProfile;
        if (!NativeModelBankReader::Revalidate(binding))
            return false;
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            if (!pin.size || pin.size > bytes.size() || !D::Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        std::uintptr_t method = 0;
        return D::ReadOnly(binding.image, binding.image + P::ObservedVtableRva + P::GetterSlot, sizeof(method)) &&
               D::Image::ReadImage(binding.image, binding.image + P::ObservedVtableRva + P::GetterSlot, &method, sizeof(method)) &&
               method == binding.image + P::GetterRva;
    }
    ClockResult NativeModelAnimationReader::Read(std::uint32_t model, std::uint32_t& output) const noexcept
    {
        namespace D = ModelAnimationDetail;
        NativeModelBankReader        bank(binding, owner);
        NativeModelBankReader::Value word;
        auto                         result = bank.Read(model, word);
        if (result != ClockResult::Ok)
            return result;
        if (!word.nativeModelInfo)
            return ClockResult::NotInitialized;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        D::Value first, second;
        if ((result = D::Capture(binding, word.nativeModelInfo, first)) != ClockResult::Ok || (result = bank.Recheck(word)) != ClockResult::Ok ||
            (result = D::Owner(binding, owner)) != ClockResult::Ok || (result = D::Capture(binding, word.nativeModelInfo, second)) != ClockResult::Ok)
            return result;
        if (!(first == second))
            return ClockResult::BindingChanged;
        if ((result = bank.Recheck(word)) != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // Preserve fullQWORD equality above, including high bits which the
        // caller later discards. No target pointer is dereferenced or retained.
        output = static_cast<std::uint32_t>(second.raw);
        return ClockResult::Ok;
    }
}

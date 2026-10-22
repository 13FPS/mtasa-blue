/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual request context qualification for scoped model ref intent
 ****************************************************************************/
#include "NativeStreamingModelContext.h"
#include "NativeStreamingRequestProfile.h"
#include "NativeModelBankProfile.h"
#include "NativeImageAccess.h"
#include "NativeUObjectMembershipProfile.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <tuple>
namespace mta::game_de::NativeStreamingModelContext
{
    namespace P = NativeStreamingRequestProfile;
    namespace Image = NativeImageAccess;
    bool Readable(std::uintptr_t address, std::size_t bytes, bool writable = false, bool imageOnly = false) noexcept
    {
        if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
            return false;
        const auto end = address + bytes;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        while (address < end)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !page.RegionSize)
                return false;
            const auto access = page.Protect & 0xff;
            if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY)
                return false;
            if (writable && access != PAGE_READWRITE && access != PAGE_WRITECOPY)
                return false;
            if (page.Type == MEM_IMAGE)
            {
                if (reinterpret_cast<std::uintptr_t>(page.AllocationBase) != image)
                    return false;
            }
            else if (imageOnly || (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED))
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                return false;
            address = (std::min)(end, begin + page.RegionSize);
        }
        return true;
    }
    bool Copy(std::uintptr_t address, void* output, std::size_t bytes) noexcept
    {
        if (!Readable(address, bytes))
            return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), output, bytes, &copied) && copied == bytes;
    }
    bool ReadOnlyImage(std::uintptr_t image, std::uintptr_t address, std::size_t bytes) noexcept
    {
        if (!Image::ImagePages(image, address, bytes))
            return false;
        const auto end = address + bytes;
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
    template <class T>
    bool At(std::uintptr_t base, std::size_t offset, T& value) noexcept
    {
        return base && offset <= (std::numeric_limits<std::uintptr_t>::max)() - base && Copy(base + offset, &value, sizeof(value));
    }
    bool QualifyProfile(std::uintptr_t image) noexcept
    {
        std::array<unsigned char, P::MaximumPinBytes> actual{};
        for (const auto& pin : P::Code)
            if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        // The raw membership reader reuses these already-qualified native
        // registry semantics; no serial is allocated and no GC lease is
        // invented by seeing a UObject in a coherent current slot.
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            if (pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        return true;
    }
    bool Same(const Value& a, const Value& b) noexcept
    {
        return std::tie(a.root, a.provider, a.rows, a.count, a.modelVtable, a.typeMethod, a.animationMethod) ==
               std::tie(b.root, b.provider, b.rows, b.count, b.modelVtable, b.typeMethod, b.animationMethod);
    }
    ClockResult Capture(std::uintptr_t image, const NativeModelBankReader::Value& model, Value& output) noexcept
    {
        Value next;
        if (!Image::ReadImage(image, image + P::RootRva, &next.root, sizeof(next.root), true))
            return ClockResult::ReadFailure;
        if (!next.root)
            return ClockResult::NotInitialized;
        NativeUObjectMembershipAccess membership(image, &Copy);
        if (next.root % alignof(std::uintptr_t) || !Readable(next.root, P::ProviderOffset + sizeof(std::uintptr_t), true) ||
            !membership.Capture(next.root, next.rootMembership) || next.rootMembership.vtableAddress != image + P::RootVtableRva ||
            !At(next.root, P::ProviderOffset, next.provider))
            return ClockResult::BindingChanged;
        if (!next.provider)
            return ClockResult::NotInitialized;
        if (next.provider == next.root || next.provider % alignof(std::uintptr_t) ||
            !Readable(next.provider, P::ProviderCountOffset + sizeof(std::int32_t), true) || !membership.Capture(next.provider, next.providerMembership) ||
            !At(next.provider, P::ProviderRowsOffset, next.rows) || !At(next.provider, P::ProviderCountOffset, next.count))
            return ClockResult::BindingChanged;
        // The native consumer bounds its selected model against count,
        // then scales by160 bytes. No inferred surrounding array fields
        // or unused capacity are treated as native allocation authority.
        if (next.count <= 0 || next.count > static_cast<std::int32_t>(NativeModelBankProfile::ModelCount) ||
            model.modelIndex >= static_cast<std::uint32_t>(next.count) || !next.rows || next.rows % alignof(std::uintptr_t) ||
            !Readable(next.rows, static_cast<std::size_t>(next.count) * P::ProviderRowStride, true))
            return ClockResult::BindingChanged;
        // Request invokes model metadata virtuals and may print its name.
        // An opaque bank word alone cannot authorize those dereferences.
        // Keep this immediate supported-image metadata admission separate
        // from the bank's intentionally permissive read-only observation.
        if (!model.nativeModelInfo || model.nativeModelInfo % alignof(std::uintptr_t) || !Readable(model.nativeModelInfo, P::ModelPrefixBytes, true) ||
            !At(model.nativeModelInfo, 0, next.modelVtable) || !ReadOnlyImage(image, next.modelVtable, P::AnimationMethodSlot + sizeof(std::uintptr_t)) ||
            !Image::ReadImage(image, next.modelVtable + P::TypeMethodSlot, &next.typeMethod, sizeof(next.typeMethod)) ||
            !Image::ReadImage(image, next.modelVtable + P::AnimationMethodSlot, &next.animationMethod, sizeof(next.animationMethod)) ||
            !Image::ImagePages(image, next.typeMethod, 1, false, true) || !Image::ImagePages(image, next.animationMethod, 1, false, true))
            return ClockResult::BindingChanged;
        if (!membership.Recheck(next.rootMembership) || !membership.Recheck(next.providerMembership))
            return ClockResult::BindingChanged;
        output = next;
        return ClockResult::Ok;
    }

    bool MembershipUnchanged(std::uintptr_t image, const Value& value) noexcept
    {
        NativeUObjectMembershipAccess membership(image, &Copy);
        return membership.Recheck(value.rootMembership) && membership.Recheck(value.providerMembership);
    }
    ClockResult Recheck(std::uintptr_t image, const NativeModelBankReader::Value& model, const Value& previous) noexcept
    {
        Value      current;
        const auto result = Capture(image, model, current);
        return result != ClockResult::Ok                                         ? result
               : Same(previous, current) && MembershipUnchanged(image, previous) ? ClockResult::Ok
                                                                                 : ClockResult::BindingChanged;
    }

}

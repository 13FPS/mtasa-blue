/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native asynchronous request effects on the existing owner
 ****************************************************************************/
#include "NativeStreamingRequestCalls.h"
#include "NativeStreamingRequestProfile.h"
#include "NativeModelBankProfile.h"
#include "NativeImageAccess.h"
#include "NativeUObjectMembershipAccess.h"
#include "NativeUObjectMembershipProfile.h"
#include <game/CModelRequestPolicy.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <tuple>

namespace mta::game_de::NativeStreamingRequestCalls
{
    namespace
    {
        namespace P = NativeStreamingRequestProfile;
        namespace Image = NativeImageAccess;
        struct Context
        {
            std::uintptr_t                          root = 0, provider = 0, rows = 0, modelVtable = 0, typeMethod = 0, animationMethod = 0;
            std::int32_t                            count = 0;
            NativeUObjectMembershipAccess::Snapshot rootMembership{}, providerMembership{};
        };
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
        bool Pins(std::uintptr_t image) noexcept
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
        bool Same(const Context& a, const Context& b) noexcept
        {
            return std::tie(a.root, a.provider, a.rows, a.count, a.modelVtable, a.typeMethod, a.animationMethod) ==
                   std::tie(b.root, b.provider, b.rows, b.count, b.modelVtable, b.typeMethod, b.animationMethod);
        }
        ClockResult Capture(std::uintptr_t image, const NativeModelBankReader::Value& model, Context& output) noexcept
        {
            Context next;
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
#if !defined(MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uint32_t model, std::uint32_t flags, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(static_cast<std::int32_t>(model), flags, 0);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    ClockResult Request(std::uintptr_t image, std::uint32_t modelIndex, std::uint32_t flags, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        Receipt receipt;
        receipt.entryRva = P::RequestRva;
        receipt.modelIndex = modelIndex;
        receipt.flags = flags;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount)
            return finish(ClockResult::InvalidArgument);
        // MTA deliberately dispatches these IDs through RequestVehicleUpgrade.
        // Ordinary RequestModel is not a substitute for that missing DE route.
        if (MtaGame::CModelRequestPolicy::IsUpgradeModel(modelIndex))
            return finish(ClockResult::UnsupportedBuild);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        NativeModelBankReader          bank({image}, owner);
        NativeModelLoadedReader        loaded({image}, owner);
        NativeModelBankReader::Value   model;
        NativeModelLoadedReader::Value before, after;
        auto                           result = bank.Read(modelIndex, model);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!model.nativeModelInfo)
            return finish(ClockResult::NotInitialized);
        if ((result = loaded.Read(modelIndex, before)) != ClockResult::Ok)
            return finish(result);
        receipt.rawFlagsBefore = before.rawFlags;
        receipt.rawStateBefore = before.rawLoadState;
        if (before.rawLoadState > 4)
            return finish(ClockResult::BindingChanged);
        if (!Pins(image))
            return finish(ClockResult::UnsupportedBuild);
        Context first, confirmed;
        if ((result = Capture(image, model, first)) != ClockResult::Ok)
            return finish(result);
        if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = loaded.Recheck(before)) != ClockResult::Ok)
            return finish(result);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        if ((result = Capture(image, model, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, confirmed) || !Pins(image))
            return finish(ClockResult::BindingChanged);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        receipt.root = first.root;
        receipt.provider = first.provider;
        receipt.modelInfo = model.nativeModelInfo;
        const auto function = reinterpret_cast<NativeFunction>(image + P::RequestRva);
#if defined(MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        receipt.entered = true;
        result = entry(function, modelIndex, flags, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if ((result = loaded.Read(modelIndex, after)) != ClockResult::Ok)
            return finish(result);
        receipt.rawFlagsAfter = after.rawFlags;
        receipt.rawStateAfter = after.rawLoadState;
        if ((result = bank.Recheck(model)) != ClockResult::Ok)
            return finish(result);
        if ((result = Capture(image, model, confirmed)) != ClockResult::Ok)
            return finish(result);
        NativeUObjectMembershipAccess membership(image, &Copy);
        if (!Same(first, confirmed) || !membership.Recheck(first.rootMembership) || !membership.Recheck(first.providerMembership) || !Pins(image))
            return finish(ClockResult::BindingChanged);
        // Async completion may advance or intentionally reject the request.
        // Acknowledgement means the real entry returned on the same admitted
        // owner; it neither fabricates loaded state nor rewinds native jobs.
        return finish(owner());
    }
}

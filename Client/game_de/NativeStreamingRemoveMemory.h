/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate typed native removal dependencies, never retained leases
 ****************************************************************************/
#pragma once
#include "NativeStreamingRemoveAssets.h"
#include "NativeModelReferenceHeap.h"
#include "NativeStreamingFileIdsProfile.h"
#include "NativeUObjectMembershipAccess.h"
#include "NativeModelTypeLeaf.h"
#include "NativeModelLoadedProfile.h"
#include <tuple>
namespace mta::game_de::NativeStreamingRemoveMemory
{
    namespace P = NativeStreamingRemoveProfile;
    namespace M = NativeModelReferenceMap;
    namespace A = NativeStreamingRemoveAssets;
    namespace C = NativeStreamingModelContext;
    namespace H = NativeModelReferenceHeap;
    namespace I = NativeImageAccess;
    using StreamRow = std::array<unsigned char, 16>;
    template <class T, std::size_t N>
    inline T Field(const std::array<unsigned char, N>& bytes, std::size_t at) noexcept
    {
        T value{};
        std::memcpy(&value, bytes.data() + at, sizeof(value));
        return value;
    }
    inline bool Read(std::uintptr_t at, void* output, std::size_t size) noexcept
    {
        if (!at || !size || size > (std::numeric_limits<std::uintptr_t>::max)() - at)
            return false;
        const auto end = at + size;
        auto       cursor = at;
        while (cursor < end)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(cursor), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !page.RegionSize)
                return false;
            const auto protection = page.Protect & 0xff;
            if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                return false;
            if (page.Type == MEM_IMAGE)
            {
                if (page.AllocationBase != GetModuleHandleW(nullptr))
                    return false;
            }
            else if (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (begin > cursor || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= cursor)
                return false;
            cursor = (std::min)(end, begin + page.RegionSize);
        }
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), output, size, &copied) && copied == size;
    }
    struct Value
    {
        C::Value                                             context{};
        H::Value                                             heap{};
        A::Snapshot                                          assets{};
        std::uintptr_t                                       model = 0, bundle = 0, association = 0, deleteMethod = 0, setMethod = 0;
        std::array<std::uintptr_t, 2>                        selectedAssets{};
        StreamRow                                            selected{}, previous{}, next{};
        std::int16_t                                         previousIndex = -1, nextIndex = -1;
        std::array<unsigned char, 6>                         typeCode{};
        std::array<unsigned char, 5>                         setterCode{};
        std::array<unsigned char, 0x58>                      bundleBytes{};
        std::array<std::uintptr_t, 3>                        classes{};
        std::vector<NativeUObjectMembershipAccess::Snapshot> memberships;
        std::array<std::uintptr_t, 30>                       sharedModels{}, sharedBundles{};
        std::array<std::int32_t, 24>                         pedIds{};
        std::uint32_t                                        pedIdCount = 0, pedCount = 0, vehicleCount = 0;
        std::uint8_t                                         type = 0;
        bool                                                 dropAssets = true;
    };
    inline bool QualifyProfile(std::uintptr_t image) noexcept
    {
        std::array<unsigned char, P::MaximumPinBytes> bytes{};
        for (const auto& pin : P::Code)
            if (!pin.size || pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        const auto& initializer = NativeStreamingFileIdsProfile::initialize;
        return I::ReadImage(image, image + NativeStreamingFileIdsProfile::initializeRva, bytes.data(), initializer.size(), false, true) &&
               std::memcmp(bytes.data(), initializer.data(), initializer.size()) == 0 && C::QualifyProfile(image);
    }
    inline bool Member(std::uintptr_t image, std::uintptr_t object, Value& value, NativeUObjectMembershipAccess::Snapshot& sample)
    {
        NativeUObjectMembershipAccess access(image, &Read);
        if (!access.Capture(object, sample))
            return false;
        value.memberships.push_back(sample);
        return true;
    }
    inline bool WeakAsset(std::uintptr_t image, Value& value, std::size_t at, std::size_t soft, std::size_t which)
    {
        const auto index = Field<std::int32_t>(value.bundleBytes, at);
        const auto serial = Field<std::uint32_t>(value.bundleBytes, at + 4);
        const auto path = Field<std::uintptr_t>(value.bundleBytes, soft);
        // Canonical empty weak+empty soft path cannot enter native fallback.
        // Stale weak handles are refused, rather than silently resolving/loading.
        if (index == -1 && serial == 0 && path == 0)
            return true;
        if (index < 0 || !serial)
            return false;
        std::int32_t   count = 0;
        std::uintptr_t chunks = 0, chunk = 0, object = 0;
        if (!I::ReadImage(image, image + NativeStartupProvidersProfile::objectCountRva, &count, sizeof(count), true) || index >= count ||
            !I::ReadImage(image, image + NativeStartupProvidersProfile::objectChunksRva, &chunks, sizeof(chunks), true) ||
            !Read(chunks + (static_cast<std::uint32_t>(index) >> 16) * 8, &chunk, sizeof(chunk)) || !chunk ||
            !Read(chunk + (static_cast<std::uint32_t>(index) & 0xffffu) * 24, &object, sizeof(object)) || !object)
            return false;
        NativeUObjectMembershipAccess::Snapshot asset{}, assetClass{};
        if (!Member(image, object, value, asset) || !NativeUObjectMembershipAccess::MatchesWeak(asset, index, serial) ||
            !Member(image, asset.classAddress, value, assetClass))
            return false;
        NativeClassChainAccess chain(&Read);
        bool                   matches = false;
        if (!chain.IsChildOf(asset.classAddress, value.classes[which ? 1 : 0], matches))
            return false;
        if (matches && which && !chain.IsChildOf(object, value.classes[2], matches))
            return false;
        // Native ignores a resolved object with the wrong class relation. That
        // false answer remains distinct from an unreadable/malformed class chain.
        if (matches)
            value.selectedAssets[which] = object;
        return true;
    }
    inline bool Memberships(std::uintptr_t image, const Value& value) noexcept
    {
        NativeUObjectMembershipAccess access(image, &Read);
        for (const auto& member : value.memberships)
            if (!access.Recheck(member))
                return false;
        return C::MembershipUnchanged(image, value.context);
    }
    inline ClockResult Capture(std::uintptr_t image, const NativeModelBankReader::Value& model, Value& output)
    {
        Value v;
        v.model = model.nativeModelInfo;
        auto result = C::Capture(image, model, v.context);
        if (result != ClockResult::Ok)
            return result;
        if (!M::PrivateWritable(v.context.root, P::AssetMapOffset + P::HeaderBytes) ||
            !M::PrivateWritable(v.context.provider, NativeStreamingRequestProfile::ProviderCountOffset + 4) ||
            !M::PrivateWritable(v.context.rows, static_cast<std::size_t>(v.context.count) * NativeStreamingRequestProfile::ProviderRowStride) ||
            !M::PrivateWritable(v.model, NativeStreamingRequestProfile::ModelPrefixBytes) || !H::ReadOnlyImage(image, v.context.modelVtable, 0x88) ||
            !I::ReadImage(image, v.context.modelVtable + 0x50, &v.deleteMethod, sizeof(v.deleteMethod)) ||
            !I::ReadImage(image, v.context.modelVtable + 0x80, &v.setMethod, sizeof(v.setMethod)) || !I::ImagePages(image, v.deleteMethod, 1, false, true) ||
            !I::ImagePages(image, v.setMethod, 1, false, true) || !NativeModelTypeLeaf::Read(image, v.context.typeMethod, v.typeCode, v.type) ||
            !M::Copy(v.model + 0x58, &v.association, sizeof(v.association)))
            return ClockResult::BindingChanged;
        if (v.type == 6 && model.modelIndex >= 409 && model.modelIndex <= 481)
            return ClockResult::UnsupportedBuild;
        if (!H::QualifyProfile(image))
            return ClockResult::UnsupportedBuild;
        if ((result = H::Capture(image, v.heap)) != ClockResult::Ok)
            return result;
        const auto     selected = image + NativeModelLoadedProfile::ArrayRva + static_cast<std::uintptr_t>(model.modelIndex) * 16;
        std::uintptr_t list = 0;
        if (!I::ReadImage(image, selected, v.selected.data(), v.selected.size(), true) || v.selected[13] != 1 ||
            !I::ReadImage(image, image + P::ListBaseRva, &list, sizeof(list), true))
            return ClockResult::ReadFailure;
        if (list != image + NativeModelLoadedProfile::ArrayRva)
            return ClockResult::BindingChanged;
        v.previousIndex = Field<std::int16_t>(v.selected, 8);
        v.nextIndex = Field<std::int16_t>(v.selected, 10);
        if (v.previousIndex == -1)
        {
            if (v.nextIndex != -1)
                return ClockResult::BindingChanged;
        }
        else
        {
            if (v.previousIndex < 0 || v.nextIndex < 0 || static_cast<std::size_t>(v.previousIndex) >= P::StreamRows ||
                static_cast<std::size_t>(v.nextIndex) >= P::StreamRows || static_cast<std::uint32_t>(v.previousIndex) == model.modelIndex ||
                static_cast<std::uint32_t>(v.nextIndex) == model.modelIndex ||
                !I::ReadImage(image, list + static_cast<std::uintptr_t>(v.previousIndex) * 16, v.previous.data(), 16, true) ||
                !I::ReadImage(image, list + static_cast<std::uintptr_t>(v.nextIndex) * 16, v.next.data(), 16, true))
                return ClockResult::BindingChanged;
            if (Field<std::int16_t>(v.previous, 10) != static_cast<std::int16_t>(model.modelIndex) ||
                Field<std::int16_t>(v.next, 8) != static_cast<std::int16_t>(model.modelIndex))
                return ClockResult::BindingChanged;
        }
        v.bundle = Field<std::uintptr_t>(v.selected, 0);
        if (v.association != v.bundle)
            return ClockResult::BindingChanged;
        if (v.type == 7)
        {
            if (!I::ReadImage(image, image + 0x5300dc0, v.pedIds.data(), sizeof(v.pedIds), true) ||
                !I::ReadImage(image, image + 0x5300d68, &v.pedIdCount, 4, true))
                return ClockResult::ReadFailure;
            const auto matches = static_cast<std::uint32_t>(std::count(v.pedIds.begin(), v.pedIds.end(), static_cast<std::int32_t>(model.modelIndex)));
            if (v.pedIdCount < matches)
                return ClockResult::BindingChanged;
        }
        if (v.type == 6)
        {
            // Outside the explicitly refused animation-dispatch keys, the
            // genuine helper only compacts these fixed, in-image WORD arrays.
            if (!I::ImagePages(image, image + 0x526e8a0, 0x59c, true) || !I::ImagePages(image, image + 0x5300e20, 100, true))
                return ClockResult::BindingChanged;
        }
        if (v.bundle)
        {
            if (v.bundle < v.context.rows || (v.bundle - v.context.rows) % NativeStreamingRequestProfile::ProviderRowStride ||
                (v.bundle - v.context.rows) / NativeStreamingRequestProfile::ProviderRowStride >= static_cast<std::size_t>(v.context.count) ||
                !M::Copy(v.bundle, v.bundleBytes.data(), v.bundleBytes.size()) ||
                !I::ReadImage(image, v.setMethod, v.setterCode.data(), v.setterCode.size(), false, true))
                return ClockResult::BindingChanged;
            const std::array<unsigned char, 5> setter{0x48, 0x89, 0x51, 0x58, 0xc3};
            if (v.setterCode != setter)
                return ClockResult::UnsupportedBuild;
            const std::array<std::uintptr_t, 3> caches{P::ObjectClassRva, P::MetaClassRva, P::ActorClassRva};
            bool                                hasWeak = false;
            for (const auto at : {std::size_t{0x18}, std::size_t{0x40}})
                hasWeak = hasWeak || Field<std::int32_t>(v.bundleBytes, at) != -1 || Field<std::uint32_t>(v.bundleBytes, at + 4) != 0 ||
                          Field<std::uintptr_t>(v.bundleBytes, at + 0x10) != 0;
            if (hasWeak)
                for (std::size_t i = 0; i < caches.size(); ++i)
                {
                    NativeUObjectMembershipAccess::Snapshot member{};
                    if (!I::ReadImage(image, image + caches[i], &v.classes[i], 8, true) || !v.classes[i])
                        return ClockResult::NotInitialized;
                    if (!Member(image, v.classes[i], v, member))
                        return ClockResult::BindingChanged;
                }
            if (!WeakAsset(image, v, 0x18, 0x28, 0) || !WeakAsset(image, v, 0x40, 0x50, 1))
                return ClockResult::BindingChanged;
            if (model.modelIndex >= 290 && model.modelIndex <= 319)
            {
                if (!I::ReadImage(image, image + NativeModelBankProfile::BankRva + 290 * 8, v.sharedModels.data(), sizeof(v.sharedModels), true))
                    return ClockResult::ReadFailure;
                for (std::size_t i = 0; i < v.sharedModels.size(); ++i)
                    if (v.sharedModels[i])
                    {
                        if (!M::PrivateWritable(v.sharedModels[i], 0x60) || !M::Copy(v.sharedModels[i] + 0x58, &v.sharedBundles[i], 8))
                            return ClockResult::BindingChanged;
                        if (i + 290 != model.modelIndex && v.sharedBundles[i] == v.bundle)
                            v.dropAssets = false;
                    }
            }
            if (v.dropAssets)
            {
                if ((result = A::Capture(v.context, model, v.assets)) != ClockResult::Ok)
                    return result;
                if (v.type == 6 && (!I::ReadImage(image, image + 0x572bec4, &v.vehicleCount, 4, true) || !v.vehicleCount))
                    return ClockResult::BindingChanged;
                if (v.type == 7 && (!I::ReadImage(image, image + 0x572bec8, &v.pedCount, 4, true) || !v.pedCount))
                    return ClockResult::BindingChanged;
            }
        }
        if (!Memberships(image, v))
            return ClockResult::BindingChanged;
        output = std::move(v);
        return ClockResult::Ok;
    }
    inline bool Same(const Value& a, const Value& b) noexcept
    {
        return C::Same(a.context, b.context) && H::Same(a.heap, b.heap) &&
               std::tie(a.model, a.bundle, a.association, a.deleteMethod, a.setMethod, a.selectedAssets, a.selected, a.previous, a.next, a.previousIndex,
                        a.nextIndex, a.typeCode, a.setterCode, a.bundleBytes, a.classes, a.sharedModels, a.sharedBundles, a.pedIds, a.pedIdCount, a.pedCount,
                        a.vehicleCount, a.type, a.dropAssets) == std::tie(b.model, b.bundle, b.association, b.deleteMethod, b.setMethod, b.selectedAssets,
                                                                          b.selected, b.previous, b.next, b.previousIndex, b.nextIndex, b.typeCode,
                                                                          b.setterCode, b.bundleBytes, b.classes, b.sharedModels, b.sharedBundles, b.pedIds,
                                                                          b.pedIdCount, b.pedCount, b.vehicleCount, b.type, b.dropAssets) &&
               A::Same(a.assets, b.assets);
    }
    inline ClockResult RecheckReturned(std::uintptr_t image, const NativeModelBankReader::Value& model, const Value& before)
    {
        auto result = C::Recheck(image, model, before.context);
        if (result != ClockResult::Ok)
            return result;
        H::Value heap{};
        if ((result = H::Capture(image, heap)) != ClockResult::Ok)
            return result;
        if (!H::Same(before.heap, heap) || !Memberships(image, before))
            return ClockResult::BindingChanged;
        std::array<unsigned char, 6> code{};
        std::array<unsigned char, 5> setter{};
        std::uint8_t                 type = 0;
        std::uintptr_t               method = 0, association = 0, list = 0;
        if (!NativeModelTypeLeaf::Read(image, before.context.typeMethod, code, type) || code != before.typeCode || type != before.type ||
            !I::ReadImage(image, before.context.modelVtable + 0x50, &method, 8) || method != before.deleteMethod ||
            !I::ReadImage(image, before.context.modelVtable + 0x80, &method, 8) || method != before.setMethod ||
            !M::Copy(before.model + 0x58, &association, 8) || association != 0 || !I::ReadImage(image, image + P::ListBaseRva, &list, 8, true) ||
            list != image + NativeModelLoadedProfile::ArrayRva)
            return ClockResult::BindingChanged;
        auto expected = before.selected;
        std::fill(expected.begin() + 8, expected.begin() + 12, static_cast<unsigned char>(0xff));
        expected[13] = 0;
        if (before.bundle)
            std::fill(expected.begin(), expected.begin() + 8, static_cast<unsigned char>(0));
        StreamRow actual{};
        if (!I::ReadImage(image, image + NativeModelLoadedProfile::ArrayRva + static_cast<std::uintptr_t>(model.modelIndex) * 16, actual.data(), 16, true) ||
            actual != expected)
            return ClockResult::BindingChanged;
        if (before.previousIndex != -1)
        {
            auto previous = before.previous, next = before.next;
            std::memcpy(previous.data() + 10, &before.nextIndex, 2);
            std::memcpy(next.data() + 8, &before.previousIndex, 2);
            if (before.previousIndex == before.nextIndex)
            {
                std::memcpy(previous.data() + 8, &before.previousIndex, 2);
                if (!I::ReadImage(image, list + static_cast<std::uintptr_t>(before.previousIndex) * 16, actual.data(), 16, true) || actual != previous)
                    return ClockResult::BindingChanged;
            }
            else if (!I::ReadImage(image, list + static_cast<std::uintptr_t>(before.previousIndex) * 16, actual.data(), 16, true) || actual != previous ||
                     !I::ReadImage(image, list + static_cast<std::uintptr_t>(before.nextIndex) * 16, actual.data(), 16, true) || actual != next)
                return ClockResult::BindingChanged;
        }
        if (before.bundle)
        {
            std::array<unsigned char, 0x58> bundle{};
            if (!M::Copy(before.bundle, bundle.data(), bundle.size()) || bundle != before.bundleBytes ||
                !I::ReadImage(image, before.setMethod, setter.data(), setter.size(), false, true) || setter != before.setterCode)
                return ClockResult::BindingChanged;
            for (std::size_t i = 0; i < before.classes.size(); ++i)
                if (before.classes[i] &&
                    (!I::ReadImage(image, image + std::array<std::uintptr_t, 3>{P::ObjectClassRva, P::MetaClassRva, P::ActorClassRva}[i], &method, 8, true) ||
                     method != before.classes[i]))
                    return ClockResult::BindingChanged;
            // The registry header alone does not bookend the class-chain
            // relation used by the native asset-selection branches.
            Value typed;
            typed.context = before.context;
            typed.bundleBytes = before.bundleBytes;
            typed.classes = before.classes;
            if (!WeakAsset(image, typed, 0x18, 0x28, 0) || !WeakAsset(image, typed, 0x40, 0x50, 1) || typed.selectedAssets != before.selectedAssets ||
                !Memberships(image, typed))
                return ClockResult::BindingChanged;
            if (before.dropAssets)
            {
                A::Snapshot actualAssets{}, expectedAssets = before.assets;
                for (const auto key : before.selectedAssets)
                    if (key)
                        A::ExpectedDrop(expectedAssets, key);
                if ((result = A::Capture(before.context, model, actualAssets)) != ClockResult::Ok)
                    return result;
                if (!A::Same(expectedAssets, actualAssets))
                    return ClockResult::BindingChanged;
                std::uint32_t count = 0;
                if (before.type == 6 && (!I::ReadImage(image, image + 0x572bec4, &count, 4, true) || count != before.vehicleCount - 1))
                    return ClockResult::BindingChanged;
                if (before.type == 7 && (!I::ReadImage(image, image + 0x572bec8, &count, 4, true) || count != before.pedCount - 1))
                    return ClockResult::BindingChanged;
            }
            if (model.modelIndex >= 290 && model.modelIndex <= 319)
            {
                std::array<std::uintptr_t, 30> models{};
                if (!I::ReadImage(image, image + NativeModelBankProfile::BankRva + 290 * 8, models.data(), sizeof(models), true) ||
                    models != before.sharedModels)
                    return ClockResult::BindingChanged;
                for (std::size_t i = 0; i < models.size(); ++i)
                    if (models[i] && i + 290 != model.modelIndex && (!M::Copy(models[i] + 0x58, &method, 8) || method != before.sharedBundles[i]))
                        return ClockResult::BindingChanged;
            }
        }
        if (before.type == 7)
        {
            auto ids = before.pedIds;
            auto count = before.pedIdCount;
            for (auto& id : ids)
                if (id == static_cast<std::int32_t>(model.modelIndex))
                {
                    id = -1;
                    --count;
                }
            std::array<std::int32_t, 24> actualIds{};
            std::uint32_t                actualCount = 0;
            if (!I::ReadImage(image, image + 0x5300dc0, actualIds.data(), sizeof(actualIds), true) || actualIds != ids ||
                !I::ReadImage(image, image + 0x5300d68, &actualCount, 4, true) || actualCount != count)
                return ClockResult::BindingChanged;
        }
        return ClockResult::Ok;
    }
}

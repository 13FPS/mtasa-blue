/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep native weapon/actor/model-reference effects inside one admitted grant
 ****************************************************************************/
#include "NativePedWeaponGrantCalls.h"
#include "NativePedWeaponGrantProfile.h"
#include "NativePedWeaponContext.h"
#include "NativePedMetadataMemory.h"
#include "NativeModelLoadedReader.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
namespace mta::game_de::NativePedWeaponGrantCalls
{
    namespace
    {
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativePedWeaponGrantProfile;
        constexpr std::uint32_t standardCount = 47;
        bool                    Profile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            std::uintptr_t group = 0;
            std::uint32_t  branch = 0;
            unsigned char  selector = 0;
            if (!NativeImageAccess::ReadImage(image, image + P::GroupSlotRva, &group, sizeof(group)) || group != image + P::GroupRva ||
                !NativeImageAccess::ReadImage(image, image + P::RemapRva, &selector, sizeof(selector)) || selector != P::Selector ||
                !NativeImageAccess::ReadImage(image, image + P::TableSlotRva, &branch, sizeof(branch)) || branch != P::BranchRva)
                return false;
            for (const auto& storage : P::ImmediateStorage)
                if (!NativeImageAccess::ImagePages(image, image + storage.rva, storage.bytes))
                    return false;
            return NativePedWeaponContext::Revalidate({image}) && D::Profile(image);
        }
        ClockResult Info(const NativeWeaponInfoReader& reader, std::uint32_t index, NativeWeaponInfoValues& output) noexcept
        {
            switch (reader.Read(index, output))
            {
                case Status::Ok:
                    return ClockResult::Ok;
                case Status::NotInitialized:
                    return ClockResult::NotInitialized;
                case Status::WrongThread:
                    return ClockResult::WrongThread;
                default:
                    return ClockResult::BindingChanged;
            }
        }
        struct Base
        {
            D::Value          graph;
            NativePedMetadata metadata;
        };
        ClockResult CaptureBase(std::uintptr_t image, const Ped& ped, const NativeWeaponInfoReader& reader, std::uint32_t type, CheckOwner owner,
                                Base& next) noexcept
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            NativeWeaponInfoValues ignored;
            if ((result = Info(reader, type, ignored)) != ClockResult::Ok ||
                (result = NativePedMetadataMemory({image}, owner).Read(ped, next.metadata)) != ClockResult::Ok ||
                (result = D::Capture(image, next.graph)) != ClockResult::Ok || (result = NativePedPoolReader({image}, owner).Recheck(ped)) != ClockResult::Ok)
                return result;
            if (next.metadata.entityType != 3 || !D::Fence(image, next.graph) || !Profile(image))
                return ClockResult::BindingChanged;
            return D::Owner(image, owner);
        }
        bool Same(const Base& a, const Base& b, bool mode) noexcept
        {
            return D::Same(a.graph, b.graph, mode) && a.metadata.modelIndex == b.metadata.modelIndex && a.metadata.entityType == b.metadata.entityType;
        }
        struct Ready
        {
            struct Row
            {
                std::uint32_t          index = 0;
                NativeWeaponInfoValues value;
                bool                   models = false;
            };
            struct Model
            {
                NativeModelBankReader::Value   bank;
                NativeModelLoadedReader::Value loaded;
            };
            std::array<Row, 11>           rows{};
            std::array<Model, 22>         models{};
            std::size_t                   rowCount = 0, modelCount = 0;
            NativePedWeaponContext::Value selected;
            std::int32_t                  slot = -1;
            signed char                   currentSlot = 0;
        };
        ClockResult Preload(std::uintptr_t image, const Ped& ped, const NativeWeaponInfoReader& reader, std::uint32_t type, CheckOwner owner,
                            Ready& ready) noexcept
        {
            const auto add = [&](std::uint32_t index, bool models) -> ClockResult
            {
                for (std::size_t i = 0; i < ready.rowCount; ++i)
                    if (ready.rows[i].index == index)
                    {
                        ready.rows[i].models |= models;
                        return ClockResult::Ok;
                    }
                if (ready.rowCount == ready.rows.size())
                    return ClockResult::ReadFailure;
                auto& row = ready.rows[ready.rowCount++];
                row.index = index;
                row.models = models;
                return Info(reader, index, row.value);
            };
            auto result = add(type, false);
            if (result != ClockResult::Ok)
                return result;
            ready.slot = ready.rows[0].value.slot;
            // Native returns zero immediately for STD slot -1. It consumes no
            // inline row or models on that path; do not invent extra requests.
            if (ready.slot == -1)
                return ClockResult::Ok;
            if (ready.slot < 0 || ready.slot >= 13)
                return ClockResult::BindingChanged;
            if ((result = NativePedWeaponContext({image}, owner).Capture(ped, static_cast<std::uint32_t>(ready.slot), ready.selected)) != ClockResult::Ok)
                return result;
            if (ready.selected.rawType >= standardCount || !D::At(ped.nativePed, P::CurrentSlotOffset, ready.currentSlot) || ready.currentSlot < 0 ||
                ready.currentSlot >= 13)
                return ClockResult::BindingChanged;
            // Native chooses its own current skill. Requiring every possible
            // consumed row is deliberately limited preloaded admission, never
            // a new MTA skill selection or a request for additional models.
            for (const auto weapon : {type, ready.selected.rawType})
            {
                if ((result = add(weapon, true)) != ClockResult::Ok)
                    return result;
                if (weapon >= 22 && weapon <= 32)
                    for (const auto index : {weapon + 25, weapon + 36, weapon + 47, std::uint32_t{47}})
                        if ((result = add(index, true)) != ClockResult::Ok)
                            return result;
            }
            // The genuine goggles comparison consumes STD weapon45's slot.
            if ((result = add(45, false)) != ClockResult::Ok)
                return result;
            for (std::size_t i = 0; i < ready.rowCount; ++i)
            {
                if (!ready.rows[i].models)
                    continue;
                for (const auto model : {ready.rows[i].value.model, ready.rows[i].value.model2})
                {
                    if (model == -1)
                        continue;
                    if (model < 0 || model >= 20000)
                        return ClockResult::BindingChanged;
                    bool found = false;
                    for (std::size_t n = 0; n < ready.modelCount; ++n)
                        if (ready.models[n].bank.modelIndex == static_cast<std::uint32_t>(model))
                            found = true;
                    if (found)
                        continue;
                    if (ready.modelCount == ready.models.size())
                        return ClockResult::ReadFailure;
                    auto& next = ready.models[ready.modelCount++];
                    if ((result = NativeModelBankReader({image}, owner).Read(static_cast<std::uint32_t>(model), next.bank)) != ClockResult::Ok ||
                        (result = NativeModelLoadedReader({image}, owner).Read(static_cast<std::uint32_t>(model), next.loaded)) != ClockResult::Ok)
                        return result;
                    if (!next.bank.nativeModelInfo || !next.loaded.streamedIn)
                        return ClockResult::NotInitialized;
                }
            }
            return NativePedWeaponContext({image}, owner).Recheck(ready.selected);
        }
        bool SameReady(const Ready& a, const Ready& b) noexcept
        {
            if (a.slot != b.slot || a.rowCount != b.rowCount || a.modelCount != b.modelCount || a.currentSlot != b.currentSlot ||
                a.selected.nativeWeapon != b.selected.nativeWeapon || a.selected.rawType != b.selected.rawType)
                return false;
            for (std::size_t i = 0; i < a.rowCount; ++i)
                if (a.rows[i].index != b.rows[i].index || a.rows[i].models != b.rows[i].models ||
                    std::memcmp(&a.rows[i].value, &b.rows[i].value, sizeof(NativeWeaponInfoValues)))
                    return false;
            for (std::size_t i = 0; i < a.modelCount; ++i)
                if (a.models[i].bank.slotAddress != b.models[i].bank.slotAddress || a.models[i].bank.nativeModelInfo != b.models[i].bank.nativeModelInfo ||
                    a.models[i].loaded.rowAddress != b.models[i].loaded.rowAddress)
                    return false;
            return true;
        }
#if !defined(MTA_DE_PED_WEAPON_GRANT_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t ped, std::uint32_t type, std::uint32_t ammo, std::int32_t unused, std::uint32_t& slot,
                               std::uint32_t& exception) noexcept
        {
            __try
            {
                slot = function(ped, type, ammo, unused);
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
    ClockResult Grant(std::uintptr_t image, const Ped& source, const NativeWeaponInfoReader& reader, std::uint32_t type, std::uint32_t ammo,
                      std::uint32_t& output, CheckOwner owner, Receipt* receiptOutput, Invoke invoke) noexcept
    {
        const Ped ped = source;
        Receipt   receipt;
        receipt.nativePed = ped.nativePed;
        receipt.handle = ped.handle;
        receipt.type = type;
        receipt.ammo = ammo;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        // Reject aliases before signed-low-WORD native indexing. No full SDK
        // type normalization or ammo saturation is performed by this boundary.
        if (type >= standardCount)
            return finish(ClockResult::InvalidArgument);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope                         scope;
        NativePedWeaponContext::Mutation grantMutation;
        if (!grantMutation)
            return finish(ClockResult::Busy);
        auto result = D::Owner(image, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        Base  first, current;
        Ready initial, ready;
        if ((result = CaptureBase(image, ped, reader, type, owner, first)) != ClockResult::Ok ||
            (result = Preload(image, ped, reader, type, owner, initial)) != ClockResult::Ok ||
            (result = CaptureBase(image, ped, reader, type, owner, current)) != ClockResult::Ok ||
            (result = Preload(image, ped, reader, type, owner, ready)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, current, true) || !SameReady(initial, ready))
            return finish(ClockResult::BindingChanged);
        receipt.root = first.graph.root;
        receipt.gameMode = first.graph.gameMode;
        receipt.level = first.graph.level;
        receipt.world = first.graph.world;
#if defined(MTA_DE_PED_WEAPON_GRANT_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_PED_WEAPON_GRANT_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        std::uint32_t slot = 0;
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(image + P::EntryRva), ped.nativePed, type, ammo, 1, slot, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        receipt.rawSlot = slot;
        // Native owns row replacement, old-model release, new attachments,
        // goggles and refcounts. Only stable receiver/world/bank authority is
        // reacquired here; replaying or restoring those effects is forbidden.
        if ((result = CaptureBase(image, ped, reader, type, owner, current)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, current, false))
            return finish(ClockResult::BindingChanged);
        output = slot;
        return finish(ClockResult::Ok);
    }
}

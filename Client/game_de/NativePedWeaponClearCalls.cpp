/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep genuine ClearWeapon selection/model/actor effects inside one entry
 ****************************************************************************/
#include "NativePedWeaponClearCalls.h"
#include "NativePedWeaponClearProfile.h"
#include "NativePedWeaponContext.h"
#include "NativePedMetadataMemory.h"
#include "NativeModelLoadedReader.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
namespace mta::game_de::NativePedWeaponClearCalls
{
    namespace
    {
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativePedWeaponClearProfile;
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
                bool                   refs = false, attach = false;
            };
            struct Model
            {
                NativeModelBankReader::Value   bank;
                NativeModelLoadedReader::Value loaded;
                bool                           attach = false;
            };
            std::array<Row, 12>           rows{};
            std::array<Model, 22>         models{};
            std::size_t                   rowCount = 0, modelCount = 0;
            NativePedWeaponContext::Value selected, destination;
            std::int32_t                  slot = -1, unarmedSlot = -1;
            signed char                   currentSlot = 0;
            bool                          matched = false, maySwitch = false, mayAttach = false;
        };
        ClockResult Preflight(std::uintptr_t image, const Ped& ped, const NativeWeaponInfoReader& reader, std::uint32_t type, CheckOwner owner,
                              Ready& ready) noexcept
        {
            const auto add = [&](std::uint32_t index, bool refs, bool attach) -> ClockResult
            {
                for (std::size_t i = 0; i < ready.rowCount; ++i)
                    if (ready.rows[i].index == index)
                    {
                        ready.rows[i].refs |= refs;
                        ready.rows[i].attach |= attach;
                        return ClockResult::Ok;
                    }
                if (ready.rowCount == ready.rows.size())
                    return ClockResult::ReadFailure;
                auto& row = ready.rows[ready.rowCount++];
                row.index = index;
                row.refs = refs;
                row.attach = attach;
                return Info(reader, index, row.value);
            };
            const auto skills = [&](std::uint32_t weapon, bool attach) -> ClockResult
            {
                auto result = add(weapon, false, attach);
                if (result != ClockResult::Ok)
                    return result;
                if (weapon >= 22 && weapon <= 32)
                    for (auto index : {weapon + 25, weapon + 36, weapon + 47, std::uint32_t{47}})
                        if ((result = add(index, false, attach)) != ClockResult::Ok)
                            return result;
                return ClockResult::Ok;
            };
            auto result = add(type, false, false);
            if (result != ClockResult::Ok)
                return result;
            ready.slot = ready.rows[0].value.slot;
            // Preserve both genuine no-op paths by actually entering native.
            // Neither consumes model refs or current-selection prerequisites.
            if (ready.slot == -1)
                return ClockResult::Ok;
            if (ready.slot < 0 || ready.slot >= 13)
                return ClockResult::BindingChanged;
            if ((result = NativePedWeaponContext({image}, owner).Capture(ped, static_cast<std::uint32_t>(ready.slot), ready.selected)) != ClockResult::Ok)
                return result;
            ready.matched = ready.selected.rawType == type;
            if (!ready.matched)
                return ClockResult::Ok;
            ready.rows[0].refs = true;
            if (!D::At(ped.nativePed, P::CurrentSlotOffset, ready.currentSlot))
                return ClockResult::ReadFailure;
            ready.maySwitch = ready.currentSlot == ready.slot;
            if (ready.maySwitch)
            {
                // The genuine current-selector owns old actor/FX retirement.
                // Native chooses current skill; only preflight its possible
                // rows, never choose a CPU skill or manufacture a model load.
                if ((result = skills(type, false)) != ClockResult::Ok || (result = add(0, false, false)) != ClockResult::Ok)
                    return result;
                for (std::size_t i = 0; i < ready.rowCount; ++i)
                    if (ready.rows[i].index == 0)
                        ready.unarmedSlot = ready.rows[i].value.slot;
                if (ready.unarmedSlot != -1)
                {
                    if (ready.unarmedSlot < 0 || ready.unarmedSlot >= 13)
                        return ClockResult::BindingChanged;
                    if ((result = NativePedWeaponContext({image}, owner).Capture(ped, static_cast<std::uint32_t>(ready.unarmedSlot), ready.destination)) !=
                        ClockResult::Ok)
                        return result;
                    ready.mayAttach = ready.destination.rawType != 0;
                    if (ready.mayAttach)
                    {
                        if (ready.destination.rawType >= standardCount)
                            return ClockResult::BindingChanged;
                        if ((result = skills(ready.destination.rawType, true)) != ClockResult::Ok)
                            return result;
                    }
                }
            }
            for (std::size_t i = 0; i < ready.rowCount; ++i)
            {
                const auto& row = ready.rows[i];
                for (unsigned n = 0; n != 2; ++n)
                {
                    // Clear decrements BOTH STD model refs. The selected
                    // target's attach wrapper consumes model1 only. Merely
                    // decrementing refs needs no Grant streaming-load guard.
                    const bool attach = row.attach && n == 0;
                    if (!row.refs && !attach)
                        continue;
                    const auto model = n ? row.value.model2 : row.value.model;
                    if (model == -1)
                        continue;
                    if (model < 0 || model >= 20000)
                        return ClockResult::BindingChanged;
                    std::size_t found = 0;
                    for (; found < ready.modelCount; ++found)
                        if (ready.models[found].bank.modelIndex == static_cast<std::uint32_t>(model))
                            break;
                    if (found == ready.modelCount)
                    {
                        if (ready.modelCount == ready.models.size())
                            return ClockResult::ReadFailure;
                        auto& value = ready.models[ready.modelCount++];
                        if ((result = NativeModelBankReader({image}, owner).Read(static_cast<std::uint32_t>(model), value.bank)) != ClockResult::Ok)
                            return result;
                        if (!value.bank.nativeModelInfo)
                            return ClockResult::NotInitialized;
                        std::array<unsigned char, 0x24> copied{};
                        const auto                      word = value.bank.nativeModelInfo + 0x22;
                        if (!D::Copy(value.bank.nativeModelInfo, copied.data(), copied.size()) ||
                            (!NativePedWeaponContext::Writable(word, 2) && !NativeImageAccess::ImagePages(image, word, 2, true)))
                            return ClockResult::ReadFailure;
                    }
                    auto& value = ready.models[found];
                    if (attach && !value.attach)
                    {
                        if ((result = NativeModelLoadedReader({image}, owner).Read(static_cast<std::uint32_t>(model), value.loaded)) != ClockResult::Ok)
                            return result;
                        if (!value.loaded.streamedIn)
                            return ClockResult::NotInitialized;
                        value.attach = true;
                    }
                }
            }
            if (ready.maySwitch && ready.unarmedSlot != -1 && (result = NativePedWeaponContext({image}, owner).Recheck(ready.destination)) != ClockResult::Ok)
                return result;
            return NativePedWeaponContext({image}, owner).Recheck(ready.selected);
        }
        bool SameReady(const Ready& a, const Ready& b) noexcept
        {
            if (a.slot != b.slot || a.unarmedSlot != b.unarmedSlot || a.currentSlot != b.currentSlot || a.matched != b.matched || a.maySwitch != b.maySwitch ||
                a.mayAttach != b.mayAttach || a.rowCount != b.rowCount || a.modelCount != b.modelCount || a.selected.nativeWeapon != b.selected.nativeWeapon ||
                a.selected.rawType != b.selected.rawType || a.destination.nativeWeapon != b.destination.nativeWeapon ||
                a.destination.rawType != b.destination.rawType)
                return false;
            for (std::size_t i = 0; i < a.rowCount; ++i)
                if (a.rows[i].index != b.rows[i].index || a.rows[i].refs != b.rows[i].refs || a.rows[i].attach != b.rows[i].attach ||
                    std::memcmp(&a.rows[i].value, &b.rows[i].value, sizeof(NativeWeaponInfoValues)))
                    return false;
            // Mutable native WORD refs are validated as accessible, not frozen
            // equal values. They belong to the whole genuine engine operation.
            for (std::size_t i = 0; i < a.modelCount; ++i)
                if (a.models[i].bank.slotAddress != b.models[i].bank.slotAddress || a.models[i].bank.nativeModelInfo != b.models[i].bank.nativeModelInfo ||
                    a.models[i].attach != b.models[i].attach)
                    return false;
            return true;
        }
#if !defined(MTA_DE_PED_WEAPON_CLEAR_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t ped, std::uint32_t type, std::uint32_t& exception) noexcept
        {
            __try
            {
                (void)function(ped, type);
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
    ClockResult Clear(std::uintptr_t image, const Ped& source, const NativeWeaponInfoReader& reader, std::uint32_t type, CheckOwner owner,
                      Receipt* receiptOutput, Invoke invoke) noexcept
    {
        const Ped ped = source;
        Receipt   receipt;
        receipt.nativePed = ped.nativePed;
        receipt.handle = ped.handle;
        receipt.type = type;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (type >= standardCount)
            return finish(ClockResult::InvalidArgument);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope                         scope;
        NativePedWeaponContext::Mutation clearMutation;
        if (!clearMutation)
            return finish(ClockResult::Busy);
        auto result = D::Owner(image, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        Base  first, current;
        Ready initial, ready;
        if ((result = CaptureBase(image, ped, reader, type, owner, first)) != ClockResult::Ok ||
            (result = Preflight(image, ped, reader, type, owner, initial)) != ClockResult::Ok ||
            (result = CaptureBase(image, ped, reader, type, owner, current)) != ClockResult::Ok ||
            (result = Preflight(image, ped, reader, type, owner, ready)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, current, true) || !SameReady(initial, ready))
            return finish(ClockResult::BindingChanged);
        receipt.root = first.graph.root;
        receipt.gameMode = first.graph.gameMode;
        receipt.level = first.graph.level;
        receipt.world = first.graph.world;
        receipt.selectedSlot = ready.slot;
        receipt.matched = ready.matched;
        receipt.maySwitch = ready.maySwitch;
        receipt.mayAttach = ready.mayAttach;
#if defined(MTA_DE_PED_WEAPON_CLEAR_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_PED_WEAPON_CLEAR_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(image + P::EntryRva), ped.nativePed, type, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        // Clear may change selected rows/current slot, release old model refs,
        // actors/FX and goggles. Reacquire only stable ped/world/bound-reader
        // authority; those genuine effects must not trigger replay/rollback.
        if ((result = CaptureBase(image, ped, reader, type, owner, current)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, current, false))
            return finish(ClockResult::BindingChanged);
        return finish(ClockResult::Ok);
    }
}

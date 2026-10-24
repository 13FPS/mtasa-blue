/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow legitimate current receivers for one complete engine rebuild
 ****************************************************************************/
#include "NativePedRebuildCalls.h"
#include "NativePedRebuildProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativePedIntelligenceReader.h"
#include "NativeTaskPoolReader.h"
#include "NativePlayerInfoProfile.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelReferenceHeap.h"
#include "NativeModelTypeLeaf.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeStartGameCalls.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
namespace mta::game_de::NativePedRebuildCalls
{
    namespace
    {
        namespace P = NativePedRebuildProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace C = NativeStreamingModelContext;
        namespace M = NativeModelReferenceMap;
        namespace H = NativeModelReferenceHeap;
        namespace Image = NativeImageAccess;

        bool Profile(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            for (const auto& pin : P::Pins)
                if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            std::uintptr_t group = 0;
            std::uint32_t  branch = 0;
            if (!Image::ReadImage(image, image + P::GroupSlotRva, &group, sizeof(group)) || group != image + P::GroupRva ||
                !Image::ReadImage(image, image + P::TableSlotRva, &branch, sizeof(branch), false, true) || branch != P::BranchRva ||
                !H::ReadOnlyImage(image, image + P::PlayerVtableRva, 0x58))
                return false;
            for (const auto& slot : P::Virtuals)
            {
                std::uintptr_t target = 0;
                if (!Image::ReadImage(image, image + P::PlayerVtableRva + slot.slot, &target, sizeof(target)) || target != image + slot.target ||
                    !Image::ImagePages(image, target, 1, false, true))
                    return false;
            }
            return D::Profile(image) && H::QualifyProfile(image) && Image::ImagePages(image, image + P::GlobalClothesRva, P::ClothesBytes, true);
        }

        struct Value
        {
            D::Value                           world;
            H::Value                           heap;
            NativePedMetadata                  metadata;
            NativePlayerInfoMemory::Identity   player;
            NativePedIntelligenceReader::Value intelligence;
            NativeTaskPoolReader::Value        secondary;
            NativeModelBankReader::Value       model;
            NativeModelLoadedReader::Value     loaded;
            C::Value                           modelContext;
            M::Snapshot                        references;
            std::array<unsigned char, 6>       typeBytes{};
            std::uintptr_t                     clothes = 0, render = 0, container = 0, head = 0, containerOwner = 0, headBacklink = 0, retained = 0;
            std::uintptr_t                     pedStatsBank = 0, pedStats = 0, abortMethod = 0, renderVtable = 0, renderEventMethod = 0;
            std::int32_t                       pedStatIndex = -1;
            std::uint16_t                      modelReferences = 0;
            std::uint8_t                       nativeModelType = 0;
            float                              firstStat = 0, secondStat = 0;
        };

        bool SameMembership(const NativeUObjectMembershipAccess::Snapshot& a, const NativeUObjectMembershipAccess::Snapshot& b) noexcept
        {
            return a.objectAddress == b.objectAddress && a.vtableAddress == b.vtableAddress && a.classAddress == b.classAddress &&
                   a.outerAddress == b.outerAddress && a.itemAddress == b.itemAddress && a.index == b.index && a.serial == b.serial;
        }

        ClockResult Capture(std::uintptr_t image, const Ped& ped, const NativePlayerInfoMemory& playerInfo, CheckOwner owner, Value& output)
        {
            Value value;
            auto  result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            if ((result = NativePedPoolReader({image}, owner).Recheck(ped)) != ClockResult::Ok ||
                (result = NativePedMetadataMemory({image}, owner).Read(ped, value.metadata)) != ClockResult::Ok ||
                (result = playerInfo.ReadIdentity(value.player, owner)) != ClockResult::Ok)
                return result;
            // RebuildModel and the direct skin-change caller rebuild CJ only.
            // No global row selected by a script index can authorize remote
            // player-data ownership or rebuilding an unrelated skin.
            if (value.player.image != image || value.player.ped != ped.nativePed || value.metadata.entityType != 3 || value.metadata.modelIndex != 0)
                return ClockResult::UnsupportedBuild;
            if (!Image::ReadImage(image, image + P::ClothesSlotRva, &value.clothes, sizeof(value.clothes), true))
                return ClockResult::ReadFailure;
            if (!value.clothes || value.clothes % alignof(std::uintptr_t) || !M::PrivateWritable(value.clothes, P::ClothesBytes) ||
                !M::Disjoint(value.clothes, P::ClothesBytes, ped.nativePed, NativePedPoolProfile::ObjectStride))
                return ClockResult::BindingChanged;
            if ((result = NativePedIntelligenceReader({image}, owner).Read(ped, value.intelligence)) != ClockResult::Ok)
                return result;
            // The genuine entry may abort secondary slot5 through virtual30.
            // Reuse the real task-pool reader's bounded supported task types;
            // a manager pointer alone must not authorize an arbitrary call.
            if (value.intelligence.secondaryTasks[5])
            {
                if ((result = NativeTaskPoolReader({image}, owner).Read(value.intelligence.secondaryTasks[5], value.secondary)) != ClockResult::Ok)
                    return result;
                if (!Image::ReadImage(image, value.secondary.vtable + 0x30, &value.abortMethod, sizeof(value.abortMethod)) ||
                    !Image::ImagePages(image, value.abortMethod, 1, false, true))
                    return ClockResult::BindingChanged;
            }
            if ((result = NativeModelBankReader({image}, owner).Read(0, value.model)) != ClockResult::Ok ||
                (result = NativeModelLoadedReader({image}, owner).Read(0, value.loaded)) != ClockResult::Ok)
                return result;
            if (!value.model.nativeModelInfo || !value.loaded.streamedIn)
                return ClockResult::NotInitialized;
            if ((result = C::Capture(image, value.model, value.modelContext)) != ClockResult::Ok ||
                (result = M::Capture(value.modelContext, value.model, value.references)) != ClockResult::Ok ||
                (result = H::Capture(image, value.heap)) != ClockResult::Ok)
                return result;
            if (!NativeModelTypeLeaf::Read(image, value.modelContext.typeMethod, value.typeBytes, value.nativeModelType) || value.nativeModelType != 5 ||
                value.references.selected < 0 || !value.references.rows[static_cast<std::size_t>(value.references.selected)].count ||
                !D::At(value.model.nativeModelInfo, P::ModelReferenceOffset, value.modelReferences) || !value.modelReferences ||
                !M::PrivateWritable(value.model.nativeModelInfo + P::ModelReferenceOffset, sizeof(value.modelReferences)))
                return ClockResult::BindingChanged;
            // This is the exact current ped-stat producer in SetModelIndex:
            // metadata+78 selects a 34-byte record from the current bank.
            // Only the selected native record is observed; no array capacity
            // or independently owned stats allocation is manufactured.
            if (!D::At(value.model.nativeModelInfo, P::ModelPedStatOffset, value.pedStatIndex) || value.pedStatIndex < 0 ||
                !Image::ReadImage(image, image + P::PedStatsBankRva, &value.pedStatsBank, sizeof(value.pedStatsBank), true) || !value.pedStatsBank ||
                !D::At(ped.nativePed, P::PedStatOffset, value.pedStats) || !value.pedStats)
                return ClockResult::BindingChanged;
            const auto statOffset = static_cast<std::uintptr_t>(value.pedStatIndex) * P::PedStatStride;
            if (statOffset > (std::numeric_limits<std::uintptr_t>::max)() - value.pedStatsBank || value.pedStats != value.pedStatsBank + statOffset)
                return ClockResult::BindingChanged;
            std::uint32_t statValue = 0;
            if (!D::At(value.pedStats, 0x20, statValue))
                return ClockResult::ReadFailure;
            if (!Image::ReadImage(image, image + P::FirstStatRva, &value.firstStat, sizeof(value.firstStat), true) ||
                !Image::ReadImage(image, image + P::SecondStatRva, &value.secondStat, sizeof(value.secondStat), true))
                return ClockResult::ReadFailure;
            // Finite native stats bound this supported CJ subset before the
            // genuine float clamp/divide/material arithmetic. MTA does not
            // clamp, rewrite or add this rule to the common SDK contract.
            if (!std::isfinite(value.firstStat) || !std::isfinite(value.secondStat))
                return ClockResult::BindingChanged;
            if (!D::At(ped.nativePed, P::RenderOffset, value.render) || !value.render || value.render % alignof(std::uintptr_t) ||
                !M::PrivateWritable(value.render, P::ContainerOffset + sizeof(std::uintptr_t)) || !D::At(value.render, P::ContainerOffset, value.container) ||
                !value.container || value.container % alignof(std::uintptr_t) || !M::PrivateWritable(value.container, P::ContainerBytes) ||
                !D::At(value.container, 0, value.head) || !value.head || value.head % alignof(std::uintptr_t) ||
                !M::PrivateWritable(value.head, P::HeadBytes) || !D::At(value.container, P::ContainerOwnerOffset, value.containerOwner) ||
                value.containerOwner != ped.nativePed + P::PedOwnerFieldOffset || !D::At(value.head, P::HeadBacklinkOffset, value.headBacklink) ||
                value.headBacklink != value.container || !M::Disjoint(value.container, P::ContainerBytes, value.head, P::HeadBytes) ||
                !M::Disjoint(value.container, P::ContainerBytes, ped.nativePed, NativePedPoolProfile::ObjectStride) ||
                !M::Disjoint(value.head, P::HeadBytes, ped.nativePed, NativePedPoolProfile::ObjectStride) ||
                !M::Disjoint(value.container, P::ContainerBytes, value.clothes, P::ClothesBytes) ||
                !M::Disjoint(value.head, P::HeadBytes, value.clothes, P::ClothesBytes))
                return ClockResult::BindingChanged;
            // Native CreateRwObject installs this model-produced render receiver
            // in ped+20. The genuine rebuild uses its virtual218 event slot;
            // the producer's native class-chain test does not establish a
            // FUObjectArray index/serial protocol for this receiver.
            if (!D::At(value.render, 0, value.renderVtable) || !H::ReadOnlyImage(image, value.renderVtable, 0x220) ||
                !Image::ReadImage(image, value.renderVtable + 0x218, &value.renderEventMethod, sizeof(value.renderEventMethod)) ||
                !Image::ImagePages(image, value.renderEventMethod, 1, false, true) || !D::At(ped.nativePed, P::RetainedObjectOffset, value.retained))
                return ClockResult::BindingChanged;
            // The common ped destructor pairs this existing optional field
            // with the same native retirement call. Its object header/class
            // was not proved; retain only the current ped-owned pointer and
            // readable allocation, never invent a UObject registry token.
            if (value.retained && (!M::PrivateWritable(value.retained, sizeof(std::uintptr_t)) ||
                                   !M::Disjoint(value.retained, sizeof(std::uintptr_t), ped.nativePed, NativePedPoolProfile::ObjectStride) ||
                                   !M::Disjoint(value.retained, sizeof(std::uintptr_t), value.clothes, P::ClothesBytes)))
                return ClockResult::BindingChanged;
            if ((result = D::Capture(image, value.world)) != ClockResult::Ok || (result = NativePedPoolReader({image}, owner).Recheck(ped)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, value.world) || !Profile(image))
                return ClockResult::BindingChanged;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = std::move(value);
            return ClockResult::Ok;
        }

        bool SameBefore(const Value& a, const Value& b) noexcept
        {
            return a.player.image == b.player.image && a.player.ped == b.player.ped && a.clothes == b.clothes && a.render == b.render &&
                   a.container == b.container && a.head == b.head && a.containerOwner == b.containerOwner && a.headBacklink == b.headBacklink &&
                   a.retained == b.retained && a.renderVtable == b.renderVtable && a.renderEventMethod == b.renderEventMethod &&
                   a.model.slotAddress == b.model.slotAddress && a.model.nativeModelInfo == b.model.nativeModelInfo && a.loaded.rawFlags == b.loaded.rawFlags &&
                   a.loaded.rawLoadState == b.loaded.rawLoadState && a.pedStatsBank == b.pedStatsBank && a.pedStats == b.pedStats &&
                   a.pedStatIndex == b.pedStatIndex && a.modelReferences == b.modelReferences && a.nativeModelType == b.nativeModelType &&
                   a.typeBytes == b.typeBytes && a.firstStat == b.firstStat && a.secondStat == b.secondStat &&
                   a.intelligence.nativeIntelligence == b.intelligence.nativeIntelligence && a.intelligence.pool == b.intelligence.pool &&
                   a.intelligence.objects == b.intelligence.objects && a.intelligence.flags == b.intelligence.flags &&
                   a.intelligence.slot == b.intelligence.slot && a.intelligence.generation == b.intelligence.generation &&
                   a.intelligence.primaryTasks == b.intelligence.primaryTasks && a.intelligence.secondaryTasks == b.intelligence.secondaryTasks &&
                   a.abortMethod == b.abortMethod && a.secondary.nativeTask == b.secondary.nativeTask && a.secondary.vtable == b.secondary.vtable &&
                   a.secondary.pool == b.secondary.pool && a.secondary.objects == b.secondary.objects && a.secondary.flags == b.secondary.flags &&
                   a.secondary.slot == b.secondary.slot && a.secondary.generation == b.secondary.generation && M::Same(a.references, b.references, false, 0) &&
                   C::Same(a.modelContext, b.modelContext) && SameMembership(a.modelContext.rootMembership, b.modelContext.rootMembership) &&
                   SameMembership(a.modelContext.providerMembership, b.modelContext.providerMembership) && H::Same(a.heap, b.heap) &&
                   D::Same(a.world, b.world, true);
        }

#if !defined(MTA_DE_PED_REBUILD_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t ped, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(ped);
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

    ClockResult Rebuild(std::uintptr_t image, const Ped& input, const NativePlayerInfoMemory& playerInfo, CheckOwner owner, Receipt* output,
                        Invoke invoke) noexcept
    {
        const Ped ped = input;
        Receipt   receipt;
        receipt.entryRva = P::EntryRva;
        receipt.nativePed = ped.nativePed;
        receipt.handle = ped.handle;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        try
        {
            Value before, confirmed;
            auto  result = Capture(image, ped, playerInfo, owner, before);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.playerData = image + NativePlayerInfoProfile::BankRva + 8;
            receipt.clothes = before.clothes;
            receipt.root = before.world.root;
            receipt.gameMode = before.world.gameMode;
            receipt.level = before.world.level;
            receipt.world = before.world.world;
            receipt.modelInfo = before.model.nativeModelInfo;
            receipt.beforeRender = before.render;
            receipt.beforeContainer = before.container;
            receipt.preservedHead = before.head;
            if ((result = Capture(image, ped, playerInfo, owner, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!SameBefore(before, confirmed))
                return finish(ClockResult::BindingChanged);
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_PED_REBUILD_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_PED_REBUILD_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &NativeCall;
#endif
            receipt.entered = true;
            result = entry(reinterpret_cast<NativeFunction>(image + P::EntryRva), ped.nativePed, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.returned = true;
            if ((result = Capture(image, ped, playerInfo, owner, confirmed)) != ClockResult::Ok)
                return finish(result);
            receipt.afterRender = confirmed.render;
            receipt.afterContainer = confirmed.container;
            // The engine owns every intermediate retirement, model-ref change,
            // task abort, descriptor update and global clothes copy. Observe a
            // legitimate fresh receiver; equality with the retired one would
            // incorrectly reject the purpose of this whole native operation.
            if (confirmed.player.image != before.player.image || confirmed.player.ped != before.player.ped || confirmed.clothes != before.clothes ||
                confirmed.head != before.head || confirmed.intelligence.nativeIntelligence != before.intelligence.nativeIntelligence ||
                !H::Same(before.heap, confirmed.heap) || !D::Same(before.world, confirmed.world, false))
                return finish(ClockResult::BindingChanged);
            return finish(NativeStartGameCalls::CheckThread(image));
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}

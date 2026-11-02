/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reapply an existing player's current group once, retaining unknowns
 ****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include "NativePlayerMoveAnimReapply.h"
#include "NativePlayerMoveAnimReapplyProfile.h"
#include "NativePedMoveAnimReader.h"
#include "NativePedMetadataMemory.h"
#include "NativePedIntelligenceReader.h"
#include "NativePlayerInfoProfile.h"
#include "NativeAnimationAssociationProfile.h"
#include "NativeAnimationManagerMemory.h"
#include "NativeAnimationHierarchyMemory.h"
#include "NativeAnimationHierarchyReadProfile.h"
#include "NativeModelReferenceHeap.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeStartGameCalls.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace mta::game_de::NativePlayerMoveAnimReapply
{
    namespace
    {
        namespace P = NativePlayerMoveAnimReapplyProfile;
        namespace I = NativeImageAccess;
        namespace D = NativeStreamingCompletionContext;
        namespace H = NativeModelReferenceHeap;
        namespace M = NativeModelReferenceMap;
        using Source = NativeAnimationManagerMemory::AssociationGroupCopySource;

        bool Profile(std::uintptr_t image) noexcept
        {
            if (sizeof(void*) != 8 || !image || image > (std::numeric_limits<std::uintptr_t>::max)() - NativePedPoolProfile::ImageSize ||
                !NativePedMoveAnimReader::Revalidate({image}) || !D::Profile(image) || !H::QualifyProfile(image))
                return false;
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            for (const auto& pin : P::Pins)
                if (!pin.size || pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            // Reuse the actual indexed row/inline-data producer and player
            // publication, not the unrelated local-only money accessor.
            for (std::size_t index = 0; index != 3; ++index)
            {
                const auto& pin = NativePlayerInfoProfile::Code[index];
                if (!pin.size || pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            for (const auto& pin : NativeAnimationAssociationProfile::Code)
                if (!pin.size || pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            std::uintptr_t control = 0, deleting = 0;
            return H::ReadOnlyImage(image, image + NativePlayerInfoProfile::PlayerVtableRva, 0x58) &&
                   I::ReadImage(image, image + NativePlayerInfoProfile::PlayerVtableRva + 0x50, &control, sizeof(control)) &&
                   control == image + NativePlayerInfoProfile::PlayerControlRva &&
                   I::ReadImage(image, image + NativeAnimationAssociationProfile::VtableRva, &deleting, sizeof(deleting)) &&
                   deleting == image + NativeAnimationAssociationProfile::DestructorRva;
        }

        struct Association
        {
            std::uintptr_t                                                              address = 0;
            std::array<unsigned char, NativeAnimationAssociationProfile::ReceiverBytes> bytes{};
        };
        struct SourceIdentity
        {
            std::uintptr_t                                                              hierarchy = 0;
            std::array<unsigned char, 32>                                               row{};
            std::array<unsigned char, NativeAnimationHierarchyReadProfile::PrefixBytes> hierarchyPrefix{};
        };
        struct Snapshot
        {
            D::Value                           world;
            H::Value                           heap;
            NativePedMetadata                  metadata;
            NativePedIntelligenceReader::Value intelligence;
            std::uintptr_t                     playerData = 0, render = 0, container = 0, head = 0, containerOwner = 0;
            std::int32_t                       selector = -1, group = 0;
            std::array<Source, 5>              sources{};
            std::array<SourceIdentity, 5>      sourceIdentities{};
            std::vector<Association>           associations;
        };

        bool Anchors(std::uintptr_t image, const Ped& ped, const Snapshot& value) noexcept
        {
            // Run after admission callbacks: a callback may have changed the
            // token's occupied row or current group while reporting its status.
            // This final raw bookend invokes no provider or native operation.
            std::uintptr_t pool = 0, objects = 0, flags = 0, vtable = 0, firstMethod = 0, data = 0, render = 0, container = 0, containerOwner = 0,
                           published = 0, head = 0;
            std::int32_t  capacity = 0, cursor = -1, selector = -1, group = 0;
            unsigned char owns = 0, flag = 0;
            if (!I::ReadImage(image, image + NativePedPoolProfile::RootRva, &pool, sizeof(pool), true) || pool != ped.pool || !D::At(ped.pool, 0, objects) ||
                !D::At(ped.pool, 8, flags) || !D::At(ped.pool, 0x10, capacity) || !D::At(ped.pool, 0x14, cursor) || !D::At(ped.pool, 0x18, owns) ||
                !M::Copy(ped.flags + ped.slot, &flag, sizeof(flag)) || objects != ped.objects || flags != ped.flags || capacity != ped.capacity ||
                cursor != ped.cursor || owns != ped.ownsAllocations || (flag & NativePedPoolProfile::EmptyBit) ||
                (flag & NativePedPoolProfile::GenerationMask) != ped.generation || !D::At(ped.nativePed, 0, vtable) || vtable != ped.vtable ||
                !I::ReadImage(image, vtable, &firstMethod, sizeof(firstMethod)) || firstMethod != ped.firstMethod || !D::At(ped.nativePed, 0x7e0, selector) ||
                selector != value.selector || !D::At(ped.nativePed, 0x5f0, data) || data != value.playerData || !D::At(ped.nativePed, 0x6e0, group) ||
                group != value.group || !D::At(ped.nativePed, 0x20, render) || render != value.render || !D::At(render, 0x300, container) ||
                container != value.container || !D::At(container, 0x20, containerOwner) || containerOwner != value.containerOwner ||
                !D::At(container, 0, head) || head != value.head)
                return false;
            const auto row = image + NativePlayerInfoProfile::BankRva + static_cast<std::uintptr_t>(selector) * NativePlayerInfoProfile::RowBytes;
            if (!I::ReadImage(image, row, &published, sizeof(published), true) || published != ped.nativePed)
                return false;
            for (const auto& association : value.associations)
            {
                std::array<unsigned char, NativeAnimationAssociationProfile::ReceiverBytes> current{};
                if (!M::Copy(association.address, current.data(), current.size()) || current != association.bytes)
                    return false;
            }
            for (std::size_t index = 0; index != value.sources.size(); ++index)
            {
                const auto&                                                                 source = value.sources[index];
                const auto&                                                                 identity = value.sourceIdentities[index];
                std::array<unsigned char, 32>                                               parent{};
                std::array<unsigned char, 32>                                               selected{};
                std::array<unsigned char, NativeAnimationHierarchyReadProfile::PrefixBytes> hierarchy{};
                if (!M::Copy(source.group.locator, parent.data(), parent.size()) || parent != source.parent32 ||
                    !M::Copy(source.selected.locator, selected.data(), selected.size()) || selected != identity.row ||
                    !I::ReadImage(image, identity.hierarchy, hierarchy.data(), hierarchy.size(), true) || hierarchy != identity.hierarchyPrefix)
                    return false;
            }
            return D::Fence(image, value.world);
        }

        bool SameSource(const Source& a, const Source& b) noexcept
        {
            return a.group.index == b.group.index && a.group.locator == b.group.locator && a.groups == b.groups && a.children == b.children &&
                   a.groupCapacity == b.groupCapacity && a.childCapacity == b.childCapacity && a.groupCount == b.groupCount && a.rawIdOffset == b.rawIdOffset &&
                   a.selected.groupIndex == b.selected.groupIndex && a.selected.index == b.selected.index && a.selected.locator == b.selected.locator &&
                   a.parent32 == b.parent32;
        }

        ClockResult Sources(std::uintptr_t image, CheckOwner owner, std::int32_t current, std::array<Source, 5>& output,
                            std::array<SourceIdentity, 5>& identities)
        {
            if (current < 0)
                return ClockResult::UnsupportedBuild;
            NativeAnimationManagerMemory memory({image}, owner);
            // Admit the CURRENT movement group with MTA's five required
            // hierarchy-bearing IDs. Do not require unrelated groups0/76 or
            // inactive transition animations merely seen on other original
            // branches. This observes inputs; it adds no Request/AddRef intent.
            constexpr std::array<std::uint32_t, 5> ids{0, 1, 2, 3, 5};
            for (std::size_t index = 0; index != output.size(); ++index)
            {
                NativeAnimationAssociationGroupReferenceToken token;
                auto                                          result = memory.ResolveAssociationGroup(static_cast<std::uint32_t>(current), token);
                if (result != ClockResult::Ok || (result = memory.ReadAssociationGroupCopySource(token, ids[index], output[index])) != ClockResult::Ok)
                    return result;
                NativeAnimationStaticAssociationValues selected;
                NativeAnimationHierarchyValues         hierarchy;
                if ((result = memory.ReadStaticAssociation(output[index].selected, selected)) != ClockResult::Ok)
                    return result;
                if (!selected.hierarchyAddress)
                    return ClockResult::NotInitialized;
                if ((result = NativeAnimationHierarchyMemory({image}, owner).Read(selected.hierarchyAddress, hierarchy)) != ClockResult::Ok)
                    return result;
                if (hierarchy.interfaceAddress != selected.hierarchyAddress)
                    return ClockResult::BindingChanged;
                // Seal exactly what the typed readers just admitted. A final
                // owner callback can replace a child's hierarchy without
                // changing its parent32 or the selected physical row address.
                auto& identity = identities[index];
                identity.hierarchy = selected.hierarchyAddress;
                if (!M::Copy(output[index].selected.locator, identity.row.data(), identity.row.size()) ||
                    !I::ReadImage(image, identity.hierarchy, identity.hierarchyPrefix.data(), identity.hierarchyPrefix.size(), true))
                    return ClockResult::ReadFailure;
                std::uintptr_t rowHierarchy = 0;
                std::uint32_t  hash = 0, totalTime = 0;
                std::int32_t   block = 0;
                std::uint16_t  count = 0;
                std::memcpy(&rowHierarchy, identity.row.data() + 0x18, sizeof(rowHierarchy));
                std::memcpy(&hash, identity.hierarchyPrefix.data(), sizeof(hash));
                std::memcpy(&count, identity.hierarchyPrefix.data() + 0x10, sizeof(count));
                std::memcpy(&block, identity.hierarchyPrefix.data() + 0x14, sizeof(block));
                std::memcpy(&totalTime, identity.hierarchyPrefix.data() + 0x18, sizeof(totalTime));
                if (rowHierarchy != identity.hierarchy || hash != hierarchy.hash || count != hierarchy.sequenceCount || block != hierarchy.blockId ||
                    totalTime != hierarchy.totalTimeBits || identity.hierarchyPrefix[0x12] != hierarchy.runningCompressed)
                    return ClockResult::BindingChanged;
            }
            return ClockResult::Ok;
        }

        ClockResult Associations(std::uintptr_t image, CheckOwner owner, Snapshot& value)
        {
            auto link = value.head;
            auto previous = value.container;
            while (link)
            {
                // This is the real clump's linked association list, not a
                // separate facade registry. Bound current traversal work and
                // refuse foreign classes/links rather than publishing them.
                if (value.associations.size() >= NativeAnimationAssociationProfile::MaximumSourceParents || link < 8 || link % alignof(std::uintptr_t))
                    return ClockResult::ReadFailure;
                Association row;
                row.address = link - 8;
                if (!M::PrivateWritable(row.address, row.bytes.size()) || !M::Copy(row.address, row.bytes.data(), row.bytes.size()))
                    return ClockResult::ReadFailure;
                std::uintptr_t vtable = 0, next = 0, backlink = 0, hierarchy = 0;
                std::memcpy(&vtable, row.bytes.data(), sizeof(vtable));
                std::memcpy(&next, row.bytes.data() + 8, sizeof(next));
                std::memcpy(&backlink, row.bytes.data() + 16, sizeof(backlink));
                std::memcpy(&hierarchy, row.bytes.data() + 0x28, sizeof(hierarchy));
                if (vtable != image + NativeAnimationAssociationProfile::VtableRva || backlink != previous || !hierarchy)
                    return ClockResult::BindingChanged;
                NativeAnimationHierarchyValues observed;
                const auto                     result = NativeAnimationHierarchyMemory({image}, owner).Read(hierarchy, observed);
                if (result != ClockResult::Ok)
                    return result;
                if (observed.interfaceAddress != hierarchy)
                    return ClockResult::BindingChanged;
                for (const auto& existing : value.associations)
                    if (existing.address == row.address)
                        return ClockResult::BindingChanged;
                value.associations.push_back(row);
                previous = link;
                link = next;
            }
            return ClockResult::Ok;
        }

        ClockResult Capture(std::uintptr_t image, const Ped& ped, CheckOwner owner, CheckOwner afterApplicationOwner, Snapshot& output)
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok || (result = afterApplicationOwner()) != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            Snapshot value;
            if ((result = NativePedMetadataMemory({image}, owner).Read(ped, value.metadata)) != ClockResult::Ok ||
                (result = NativePedIntelligenceReader({image}, owner).Read(ped, value.intelligence)) != ClockResult::Ok ||
                (result = NativePedMoveAnimReader({image}, owner).Read(ped, value.group)) != ClockResult::Ok)
                return result;
            if (ped.vtable != image + NativePlayerInfoProfile::PlayerVtableRva || value.metadata.entityType != 3)
                return ClockResult::UnsupportedBuild;
            if (!D::At(ped.nativePed, 0x7e0, value.selector) || !D::At(ped.nativePed, 0x5f0, value.playerData) || !D::At(ped.nativePed, 0x20, value.render))
                return ClockResult::ReadFailure;
            if (value.selector != 0 && value.selector != 1)
                return ClockResult::UnsupportedBuild;
            const auto     row = image + NativePlayerInfoProfile::BankRva + static_cast<std::uintptr_t>(value.selector) * NativePlayerInfoProfile::RowBytes;
            std::uintptr_t published = 0;
            if (!I::ImagePages(image, row, NativePlayerInfoProfile::RowBytes, true) || !I::ReadImage(image, row, &published, sizeof(published), true) ||
                published != ped.nativePed || value.playerData != row + 8)
                return ClockResult::BindingChanged;
            if (!value.render || value.render % alignof(std::uintptr_t) || !M::PrivateWritable(value.render, 0x308) ||
                !D::At(value.render, 0x300, value.container) || !value.container || !M::PrivateWritable(value.container, 0x28) ||
                !D::At(value.container, 0, value.head) || !D::At(value.container, 0x20, value.containerOwner) || value.containerOwner != ped.nativePed + 0x6e4)
                return ClockResult::BindingChanged;
            if ((result = Sources(image, owner, value.group, value.sources, value.sourceIdentities)) != ClockResult::Ok ||
                (result = Associations(image, owner, value)) != ClockResult::Ok || (result = H::Capture(image, value.heap)) != ClockResult::Ok ||
                (result = D::Capture(image, value.world)) != ClockResult::Ok ||
                (result = NativePedPoolReader({image}, owner).Recheck(ped)) != ClockResult::Ok || (result = afterApplicationOwner()) != ClockResult::Ok)
                return result;
            if (!Profile(image) || !Anchors(image, ped, value))
                return ClockResult::BindingChanged;
            output = std::move(value);
            return ClockResult::Ok;
        }

        bool SameIdentity(const Snapshot& a, const Snapshot& b, bool before) noexcept
        {
            if (a.selector != b.selector || a.group != b.group || a.playerData != b.playerData || a.render != b.render || a.container != b.container ||
                a.containerOwner != b.containerOwner || a.metadata.entityType != b.metadata.entityType || a.metadata.modelIndex != b.metadata.modelIndex ||
                a.intelligence.nativeIntelligence != b.intelligence.nativeIntelligence || a.intelligence.taskManager != b.intelligence.taskManager ||
                a.intelligence.pool != b.intelligence.pool || a.intelligence.objects != b.intelligence.objects ||
                a.intelligence.flags != b.intelligence.flags || a.intelligence.slot != b.intelligence.slot ||
                a.intelligence.generation != b.intelligence.generation || !H::Same(a.heap, b.heap) || !D::Same(a.world, b.world, before))
                return false;
            for (std::size_t index = 0; index != a.sources.size(); ++index)
                if (!SameSource(a.sources[index], b.sources[index]))
                    return false;
            if (!before)
                return true;
            for (std::size_t index = 0; index != a.sourceIdentities.size(); ++index)
                if (a.sourceIdentities[index].hierarchy != b.sourceIdentities[index].hierarchy ||
                    a.sourceIdentities[index].row != b.sourceIdentities[index].row ||
                    a.sourceIdentities[index].hierarchyPrefix != b.sourceIdentities[index].hierarchyPrefix)
                    return false;
            if (a.head != b.head || a.intelligence.primaryTasks != b.intelligence.primaryTasks ||
                a.intelligence.secondaryTasks != b.intelligence.secondaryTasks || a.associations.size() != b.associations.size())
                return false;
            for (std::size_t index = 0; index != a.associations.size(); ++index)
                if (a.associations[index].address != b.associations[index].address || a.associations[index].bytes != b.associations[index].bytes)
                    return false;
            return true;
        }

#if !defined(MTA_DE_PLAYER_MOVEANIM_REAPPLY_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t ped, std::uint32_t& exceptionCode) noexcept
        {
            __try
            {
                function(ped);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exceptionCode = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }

    struct Controller::Data
    {
        Receipt firstUnknown;
    };
    Controller::Controller() noexcept : data(new (std::nothrow) Data)
    {
    }
    Controller::~Controller() = default;

    void Controller::RetainAfterEntry(const Receipt& entered, ClockResult result) noexcept
    {
        // Runtime API serialization encloses this CPU-only update. Preserve
        // the FIRST unknown original; later refusals cannot overwrite debt.
        if (!entered.entered || debt.load())
            return;
        if (data)
        {
            data->firstUnknown = entered;
            data->firstUnknown.phase = Phase::Unknown;
            data->firstUnknown.result = result;
            data->firstUnknown.unknown = true;
            data->firstUnknown.acknowledged = false;
        }
        debt.store(true);
    }

    ClockResult Controller::Reapply(std::uintptr_t image, const Ped& input, CheckOwner owner, CheckOwner afterApplicationOwner, Receipt* output,
                                    Invoke invoke) noexcept
    {
        const Ped ped = input;
        Receipt   receipt;
        receipt.image = image;
        receipt.entryRva = P::EntryRva;
        receipt.nativePed = ped.nativePed;
        receipt.handle = ped.handle;
        bool       ownsBusy = false;
        const auto finish = [&](ClockResult result)
        {
            receipt.result = result;
            if (receipt.entered && (result != ClockResult::Ok || !receipt.acknowledged))
            {
                receipt.phase = Phase::Unknown;
                receipt.unknown = true;
                RetainAfterEntry(receipt, result);
            }
            if (output)
                *output = receipt;
            if (ownsBusy)
                busy.store(false);
            return result;
        };
        if (!owner || !afterApplicationOwner || !data)
            return finish(ClockResult::NotInitialized);
        if (sizeof(void*) != 8)
            return finish(ClockResult::UnsupportedBuild);
        if (debt.load())
            return finish(ClockResult::Stopped);
        bool expected = false;
        if (!busy.compare_exchange_strong(expected, true))
            return finish(ClockResult::Busy);
        ownsBusy = true;
        if (debt.load())
            return finish(ClockResult::Stopped);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        try
        {
            Snapshot before, confirmed;
            auto     result = Capture(image, ped, owner, afterApplicationOwner, before);
            if (result != ClockResult::Ok || (result = Capture(image, ped, owner, afterApplicationOwner, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!SameIdentity(before, confirmed, true))
                return finish(ClockResult::BindingChanged);
            receipt.playerData = before.playerData;
            receipt.render = before.render;
            receipt.container = before.container;
            receipt.world = before.world.world;
            receipt.currentSelector = before.selector;
            receipt.beforeGroup = before.group;
            receipt.phase = Phase::Admitted;
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok || (result = afterApplicationOwner()) != ClockResult::Ok)
                return finish(result);
            if (!Profile(image) || !Anchors(image, ped, before))
                return finish(ClockResult::BindingChanged);
#if defined(MTA_DE_PLAYER_MOVEANIM_REAPPLY_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_PLAYER_MOVEANIM_REAPPLY_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &NativeCall;
#endif
            // The current group is an observed input. Do not write it, mint a
            // block reference or claim that MTA requested this native group.
            receipt.entered = true;
            receipt.phase = Phase::Entered;
            result = entry(reinterpret_cast<NativeFunction>(image + P::EntryRva), ped.nativePed, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.returned = true;
            receipt.phase = Phase::Returned;
            if ((result = Capture(image, ped, owner, afterApplicationOwner, confirmed)) != ClockResult::Ok)
                return finish(result);
            receipt.afterGroup = confirmed.group;
            // The original may replace association membership and payloads.
            // Re-admit that new native list, retaining only the same player,
            // current group/clump, input-source and world identity anchors.
            if (!SameIdentity(before, confirmed, false))
                return finish(ClockResult::BindingChanged);
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok || (result = afterApplicationOwner()) != ClockResult::Ok)
                return finish(result);
            if (!Profile(image) || !Anchors(image, ped, confirmed))
                return finish(ClockResult::BindingChanged);
            receipt.acknowledged = true;
            receipt.phase = Phase::Acknowledged;
            return finish(ClockResult::Ok);
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}

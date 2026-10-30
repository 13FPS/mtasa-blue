/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compose MTA's one-name group source without native-call replay
 ****************************************************************************/
#include "NativeAnimationAssociationGroupCreationController.h"
#include "NativeAnimationBlockFactoryMemory.h"
#include "NativeAnimationHierarchyMemory.h"
#include "NativeModelReferenceMap.h"
#include "NativeStaticAssociationArrayProfile.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <list>
#include <new>
namespace mta::game_de
{
    namespace GroupSourceCreationDetail
    {
        namespace C = NativeAnimationAssociationGroupCreation;
        namespace G = NativeAnimationAssociationGroupReplacementCalls;
        namespace A = NativeStaticAssociationArrayStorageCalls;
        namespace T = NativeTemporaryModelClumpCalls;
        namespace J = NativeTemporaryStaticAssociationInitializeCalls;
        struct Guard
        {
            std::atomic<bool>& busy;
            ~Guard() { busy.store(false, std::memory_order_release); }
        };
        bool Enter(std::atomic<bool>& busy) noexcept
        {
            bool expected = false;
            return busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }
        template <class Value>
        Value Field(const std::array<unsigned char, 32>& bytes, std::size_t at) noexcept
        {
            Value value;
            std::memcpy(&value, bytes.data() + at, sizeof(value));
            return value;
        }
    }
    struct NativeAnimationAssociationGroupCreationController::Data
    {
        using C = NativeAnimationAssociationGroupCreation::Receipt;
        using Phase = NativeAnimationAssociationGroupCreation::Phase;
        using Step = NativeAnimationAssociationGroupCreation::StepFacts;
        struct Row
        {
            Step                                           facts;
            NativeTemporaryModelClump                      temporary;
            NativeTemporaryStaticAssociationInitialization initialization;
        };
        struct Entry
        {
            std::uintptr_t                                        image = 0;
            CheckOwner                                            owner = nullptr;
            C                                                     receipt;
            std::string                                           name;
            NativeAnimationAssociationGroupChildBorrowInvalidator invalidator;
            Invocations                                           invocations;
            NativeAnimationAssociationGroupReplacement            replacement;
            NativeCustomAnimSequenceStorage                       array;
            NativeAnimationBlockValues                            blockValues{};
            NativeStaticAssociationArrayStorageCalls::Receipt     cleanup{};
            std::unique_ptr<Row[]>                                rows;
            bool                                                  opaqueGroupRetire = false;
        };
        std::list<std::unique_ptr<Entry>> entries;
        struct RowContext
        {
            Entry*        entry;
            std::uint32_t index;
        };
        static ClockResult Owner(Entry& e) noexcept { return e.owner ? e.owner() : ClockResult::NotInitialized; }
        static void        Classify(Entry& e) noexcept
        {
            auto&       r = e.receipt;
            const auto& g = r.replacement;
            r.hadEffects = g.retireEntered || g.blockStore.entered || g.childStore.entered || g.countStore.entered || r.array.entered || e.cleanup.entered;
            r.enteredUnknown = (g.retireEntered && !g.retireReturned) || (g.blockStore.entered && !g.blockStore.returned) ||
                               (g.childStore.entered && !g.childStore.returned) || (g.countStore.entered && !g.countStore.returned) ||
                               (r.array.entered && !r.array.returned) || (e.cleanup.entered && !e.cleanup.returned);
            r.nativeRecovery = e.opaqueGroupRetire || e.array.GetState() == NativeCustomAnimSequenceStorage::State::PublicationPinned ||
                               (e.array.HasOutstandingResource() &&
                                ((r.array.entered && !r.array.returned) || !r.array.initialized || (e.cleanup.entered && !e.cleanup.released)));
            r.retained = e.opaqueGroupRetire || e.array.HasOutstandingResource();
            if (e.rows)
                for (std::uint32_t i = 0; i < r.count; ++i)
                {
                    const auto& row = e.rows[i];
                    const auto& f = row.facts;
                    r.hadEffects |= f.temporary.createEntered || f.temporary.setupEntered || f.temporary.prepareEntered || f.temporary.retireEntered ||
                                    f.initialization.entered || f.idStore.entered || f.groupStore.entered;
                    r.enteredUnknown |=
                        (f.temporary.createEntered && !f.temporary.createReturned) || (f.temporary.setupEntered && !f.temporary.setupReturned) ||
                        (f.temporary.prepareEntered && !f.temporary.prepareReturned) || (f.temporary.retireEntered && !f.temporary.retireReturned) ||
                        (f.initialization.entered && !f.initialization.returned) || (f.idStore.entered && !f.idStore.verified) ||
                        (f.groupStore.entered && !f.groupStore.verified);
                    const bool pending = row.temporary.HasOutstandingResource() || row.initialization.HasOutstandingResource();
                    r.retained |= pending;
                    r.nativeRecovery |= pending;
                }
            r.privateCleanupPending = e.array.HasOutstandingResource() && !r.nativeRecovery && r.array.returned && r.array.allocated && r.array.initialized &&
                                      !e.array.PublicationAffinity() && !e.cleanup.entered;
            r.partialAccepted = !r.completed && (g.oldChildrenConsumed || g.blockStore.accepted || g.childStore.accepted || g.countStore.accepted ||
                                                 r.initialized || r.idsStored || r.groupsStored);
        }
        static ClockResult Finish(Entry& e, ClockResult result) noexcept
        {
            e.receipt.result = result;
            // Keep accepted native prefixes as facts. A failed scalar does
            // not create a new private owner of an engine-published array.
            Classify(e);
            return result;
        }
        static ClockResult Bookend(Entry& e) noexcept
        {
            auto result = Owner(e);
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationBlockValues current;
            if ((result = NativeAnimationBlockMemory({e.image}, e.owner).Read(e.receipt.block, current)) != ClockResult::Ok)
                return result;
            if (current.index != e.blockValues.index || current.idOffset != e.blockValues.idOffset || current.animationCount != e.blockValues.animationCount ||
                current.loaded != e.blockValues.loaded)
                return ClockResult::BindingChanged;
            return NativeAnimationManagerMemory({e.image}, e.owner).RecheckAssociationGroupReplacementLayout(e.receipt.replacement.observed);
        }
        static ClockResult InitializeRow(const NativeTemporaryModelClumpBorrow& borrow, void* context) noexcept
        {
            auto& selected = *static_cast<RowContext*>(context);
            auto& e = *selected.entry;
            auto& row = e.rows[selected.index];
            return NativeTemporaryStaticAssociationInitializeCalls::Initialize(e.image, row.initialization, e.receipt.generation, borrow, e.replacement,
                                                                               e.array, selected.index, row.facts.hierarchy, &row.facts.initialization,
                                                                               e.invocations.initialize);
        }
        static ClockResult RowReference(Entry& e, std::uint32_t index, NativeAnimationStaticAssociationReferenceToken& output) noexcept
        {
            auto result = Bookend(e);
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationStaticAssociationReferenceToken next;
            result = NativeAnimationManagerMemory({e.image}, e.owner).ResolveStaticAssociation(e.receipt.group.index, index, next);
            if (result != ClockResult::Ok)
                return result;
            if (next.locator != e.array.IssuedTag() + static_cast<std::uintptr_t>(index) * 32)
                return ClockResult::BindingChanged;
            next.ownerGeneration = e.receipt.generation;
            output = next;
            return ClockResult::Ok;
        }
        static ClockResult MissingFresh(Entry& e, std::uint32_t index) noexcept
        {
            NativeAnimationStaticAssociationReferenceToken token;
            auto                                           result = RowReference(e, index, token);
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationStaticAssociationValues value;
            if ((result = NativeAnimationManagerMemory({e.image}, e.owner).ReadStaticAssociation(token, value)) != ClockResult::Ok)
                return result;
            std::uintptr_t nodes = 0;
            if (!NativeModelReferenceMap::Copy(token.locator + 0x10, &nodes, sizeof(nodes)))
                return ClockResult::ReadFailure;
            // A missing model leaves the fresh source row untouched. WORD8
            // remains undefined and is not a node-capacity assertion.
            if (nodes || value.hierarchyAddress || value.animId != -1 || value.animGroup != -1 || value.flags)
                return ClockResult::BindingChanged;
            return Bookend(e);
        }
        static ClockResult Run(Entry& e) noexcept
        {
            namespace D = GroupSourceCreationDetail;
            auto& r = e.receipt;
            auto  result = Owner(e);
            if (result != ClockResult::Ok)
                return Finish(e, result);
            r.phase = Phase::RetireOld;
            result =
                D::G::RetireCurrentChildren(e.image, e.replacement, r.generation, r.group, e.owner, e.invalidator, &r.replacement, e.invocations.retireGroup);
            e.opaqueGroupRetire = r.replacement.retireEntered && !r.replacement.retireReturned;
            if (result != ClockResult::Ok)
                return Finish(e, result);
            r.phase = Phase::FindBlock;
            // Match the source's retirement-before-lookup order. Full source
            // text was already copied; this is explicit native name admission,
            // never a truncation or default-group substitution.
            const auto nul = e.name.find('\0');
            const auto length = nul == std::string::npos ? e.name.size() : nul;
            if (length >= r.query.size())
                return Finish(e, ClockResult::InvalidArgument);
            std::copy_n(e.name.data(), length, r.query.data());
            r.queryPrepared = true;
            std::int32_t                 block = -1;
            NativeAnimationManagerMemory memory({e.image}, e.owner);
            if ((result = memory.FindBlockIndex(r.query, block)) != ClockResult::Ok)
                return Finish(e, result);
            if (block < 0)
                return Finish(e, ClockResult::NotInitialized);
            NativeAnimationBlockFactoryMemory factory({e.image}, e.owner);
            if ((result = factory.ReadIdentity(static_cast<std::uint32_t>(block), r.block)) != ClockResult::Ok)
                return Finish(e, result);
            r.block.ownerGeneration = r.generation;
            if ((result = NativeAnimationBlockMemory({e.image}, e.owner).Read(r.block, e.blockValues)) != ClockResult::Ok)
                return Finish(e, result);
            r.count = e.blockValues.animationCount;
            if (r.count > static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)()) ||
                r.count > (std::numeric_limits<std::size_t>::max)() / sizeof(Row))
                return Finish(e, ClockResult::InvalidArgument);
            r.phase = Phase::PrepareRows;
            // Every temporary/joint journal exists before fresh allocation or
            // per-row entry. A later CPU OOM cannot orphan native children.
            try
            {
                if (r.count)
                    e.rows = std::make_unique<Row[]>(r.count);
            }
            catch (...)
            {
                return Finish(e, ClockResult::WriteFailure);
            }
            r.phase = Phase::PublishBlock;
            if ((result = D::G::PublishBlock(e.image, e.replacement, r.generation, r.block, e.owner, &r.replacement, e.invocations.qword)) != ClockResult::Ok)
                return Finish(e, result);
            // PublishBlock re-reads the current bank. Do not allocate a stale
            // extent when it no longer matches the already reserved journals.
            if (r.replacement.fullCount != r.count)
                return Finish(e, ClockResult::BindingChanged);
            if ((result = Bookend(e)) != ClockResult::Ok)
                return Finish(e, result);
            r.phase = Phase::AllocateArray;
            if ((result = D::A::AllocateFresh(e.image, e.array, r.generation, r.count, e.owner, &r.array, e.invocations.allocateArray)) != ClockResult::Ok)
                return Finish(e, result);
            r.phase = Phase::PublishChildren;
            if ((result = D::G::PublishChildren(e.image, e.replacement, r.generation, e.array, e.owner, &r.replacement, e.invocations.qword)) !=
                ClockResult::Ok)
                return Finish(e, result);
            r.phase = Phase::PublishCount;
            if ((result = D::G::PublishFullCount(e.image, e.replacement, r.generation, e.owner, &r.replacement, e.invocations.dword)) != ClockResult::Ok)
                return Finish(e, result);
            for (std::uint32_t i = 0; i < r.count; ++i)
            {
                auto& row = e.rows[i];
                auto& step = row.facts;
                step.index = i;
                step.visited = true;
                ++r.visited;
                r.failedIndex = i;
                auto finishRow = [&](ClockResult status)
                {
                    step.result = status;
                    r.lastStep = step;
                    return Finish(e, status);
                };
                r.phase = Phase::ReadHierarchy;
                if ((result = Bookend(e)) != ClockResult::Ok)
                    return finishRow(result);
                std::uintptr_t hierarchy = 0;
                if ((result = factory.ReadHierarchy(r.block, i, hierarchy)) != ClockResult::Ok)
                    return finishRow(result);
                if (!hierarchy)
                    return finishRow(ClockResult::NotInitialized);
                step.hierarchy = {r.generation, hierarchy};
                NativeAnimationHierarchyValues values;
                if ((result = NativeAnimationHierarchyMemory({e.image}, e.owner).Read(hierarchy, values)) != ClockResult::Ok)
                    return finishRow(result);
                if (values.blockId != static_cast<std::int32_t>(r.block.index))
                    return finishRow(ClockResult::BindingChanged);
                r.phase = Phase::FindModel;
                if ((result = NativeModelHashReader({e.image}, e.owner).FindFirstByHash(values.hash, step.model)) != ClockResult::Ok)
                    return finishRow(result);
                if (!step.model.found)
                {
                    if ((result = MissingFresh(e, i)) != ClockResult::Ok)
                        return finishRow(result);
                    step.missingModel = true;
                    step.result = ClockResult::Ok;
                    ++r.missingModels;
                    r.lastStep = step;
                    continue;
                }
                r.phase = Phase::Temporary;
                RowContext context{&e, i};
                result = D::T::WithTemporary(e.image, row.temporary, r.generation, step.model.model, e.owner, &InitializeRow, &context, &step.temporary,
                                             &e.invocations.temporary);
                if (step.initialization.returned && step.initialization.verified)
                {
                    step.initialized = true;
                    ++r.initialized;
                }
                if (result != ClockResult::Ok)
                    return finishRow(result);
                // Null create is not a missing-model shortcut. Only a verified
                // original Init and accepted deferred actor retirement reach IDs.
                if (!step.initialized || !step.temporary.retireAccepted || !step.temporary.verified)
                    return finishRow(ClockResult::NotInitialized);
                NativeAnimationStaticAssociationReferenceToken token;
                if ((result = RowReference(e, i, token)) != ClockResult::Ok)
                    return finishRow(result);
                NativeAnimationManagerMemory::AssociationGroupReplacementLayout current;
                if ((result = memory.ReadAssociationGroupReplacementLayout(r.group, current)) != ClockResult::Ok)
                    return finishRow(result);
                const auto id = static_cast<std::uint16_t>(D::Field<std::uint32_t>(current.parent, 0x14) + i);
                const auto group = static_cast<std::uint16_t>(D::Field<std::uint32_t>(current.parent, 0x18));
                r.phase = Phase::WriteID;
                if ((result = memory.WriteStaticAssociation(token, NativeAnimationManagerMemory::StaticAssociationScalar::AnimID, id, &step.idStore,
                                                            e.invocations.word)) != ClockResult::Ok)
                    return finishRow(result);
                step.idStored = true;
                ++r.idsStored;
                r.phase = Phase::WriteGroup;
                if ((result = memory.WriteStaticAssociation(token, NativeAnimationManagerMemory::StaticAssociationScalar::AnimGroup, group, &step.groupStore,
                                                            e.invocations.word)) != ClockResult::Ok)
                    return finishRow(result);
                step.groupStored = true;
                ++r.groupsStored;
                if ((result = Bookend(e)) != ClockResult::Ok)
                    return finishRow(result);
                step.result = ClockResult::Ok;
                r.lastStep = step;
            }
            r.phase = Phase::Finish;
            if ((result = D::G::FinishPublished(e.image, e.replacement, r.generation, e.owner, &r.replacement)) != ClockResult::Ok)
                return Finish(e, result);
            r.completed = true;
            r.failedIndex = 0xffffffffu;
            r.phase = Phase::Completed;
            return Finish(e, ClockResult::Ok);
        }
    };
    NativeAnimationAssociationGroupCreationController::NativeAnimationAssociationGroupCreationController() noexcept = default;
    NativeAnimationAssociationGroupCreationController::~NativeAnimationAssociationGroupCreationController() = default;
    void NativeAnimationAssociationGroupCreationController::Publish() noexcept
    {
        bool pending = false, unknown = false;
        if (data)
            for (auto& entry : data->entries)
            {
                Data::Classify(*entry);
                pending |= entry->receipt.retained;
                unknown |= entry->receipt.nativeRecovery;
            }
        retained.store(pending, std::memory_order_release);
        recovery.store(unknown, std::memory_order_release);
    }
    ClockResult NativeAnimationAssociationGroupCreationController::Create(std::uintptr_t image, std::uint64_t generation,
                                                                          const NativeAnimationAssociationGroupReferenceToken& group, const std::string& name,
                                                                          CheckOwner owner, NativeAnimationAssociationGroupChildBorrowInvalidator invalidator,
                                                                          Receipt* output, const Invocations* supplied) noexcept
    {
        if (output)
            *output = {};
        const auto request = group;
        const auto callback = invalidator;
        if (!generation || request.ownerGeneration != generation || !request.locator)
            return ClockResult::OwnerConflict;
        if (!GroupSourceCreationDetail::Enter(busy))
            return ClockResult::Busy;
        GroupSourceCreationDetail::Guard guard{busy};
        if (retained.load())
            return ClockResult::OwnerConflict;
        if (serial == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::OwnerConflict;
#if defined(MTA_DE_ANIMATION_GROUP_CREATION_INVOCATIONS)
        if (!supplied)
            supplied = MTA_DE_ANIMATION_GROUP_CREATION_INVOCATIONS();
#endif
        try
        {
            if (!data)
                data = std::make_unique<Data>();
            // Previous completed/known scalar-prefix records have no native
            // release authority. Keep just the latest bounded CPU audit record.
            data->entries.clear();
            auto entry = std::make_unique<Data::Entry>();
            entry->image = image;
            entry->owner = owner;
            entry->name = name;
            entry->invalidator = callback;
            if (supplied)
                entry->invocations = *supplied;
            entry->receipt.generation = generation;
            entry->receipt.serial = serial + 1;
            entry->receipt.group = request;
            entry->receipt.nameLength = entry->name.size();
            entry->receipt.nameCopied = true;
            data->entries.push_back(std::move(entry));
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        auto& entry = *data->entries.back();
        serial = entry.receipt.serial;
        const auto result = Data::Run(entry);
        Publish();
        if (output)
            *output = entry.receipt;
        return result;
    }
    ClockResult NativeAnimationAssociationGroupCreationController::DrainPrivatePending(std::uintptr_t image, std::uint64_t generation, CheckOwner owner,
                                                                                       std::uint32_t budget, DrainProgress& output) noexcept
    {
        output = {};
        if (!GroupSourceCreationDetail::Enter(busy))
            return ClockResult::Busy;
        GroupSourceCreationDetail::Guard guard{busy};
        if (!data)
            return ClockResult::Ok;
        ClockResult result = ClockResult::Ok;
        for (auto& pointer : data->entries)
        {
            auto& entry = *pointer;
            ++output.scanned;
            Data::Classify(entry);
            if (!entry.receipt.retained)
                continue;
            if (entry.receipt.generation != generation || entry.image != image)
            {
                result = ClockResult::OwnerConflict;
                continue;
            }
            if (entry.receipt.privateCleanupPending && output.attempted < budget)
            {
                ++output.attempted;
                entry.receipt.phase = NativeAnimationAssociationGroupCreation::Phase::DrainPrivate;
                const auto status = NativeStaticAssociationArrayStorageCalls::RetireFresh(image, entry.array, generation, owner, &entry.cleanup,
                                                                                          entry.invocations.retireArray, entry.invocations.freeArray);
                if (entry.cleanup.released)
                    ++output.released;
                if (status != ClockResult::Ok)
                {
                    result = status;
                    output.ownerStatus = status;
                }
                Data::Classify(entry);
            }
            if (entry.receipt.retained)
                ++output.retained;
        }
        Publish();
        return result;
    }
    ClockResult NativeAnimationAssociationGroupCreationController::ReadStep(std::uint64_t generation, std::uint64_t serialValue, std::uint32_t index,
                                                                            StepFacts& output) noexcept
    {
        if (!GroupSourceCreationDetail::Enter(busy))
            return ClockResult::Busy;
        GroupSourceCreationDetail::Guard guard{busy};
        if (data)
            for (const auto& entry : data->entries)
                if (entry->receipt.generation == generation && entry->receipt.serial == serialValue)
                {
                    if (!entry->rows || index >= entry->receipt.count)
                        return ClockResult::InvalidArgument;
                    output = entry->rows[index].facts;
                    return ClockResult::Ok;
                }
        return ClockResult::OwnerConflict;
    }
}

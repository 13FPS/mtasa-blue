/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native counter and missing-name facts without fallback
 ****************************************************************************/
#include "CAnimManagerDE.h"
#include "CAnimBlendAssocGroupDE.h"
#include "CAnimBlendAssociationDE.h"
#include "CAnimBlockRequestDE.h"
#include "CAnimBlendHierarchyDE.h"
#include "CAnimBlendSequenceDE.h"
#include "CAnimBlendStaticAssociationDE.h"
#include "CKeyGenDE.h"
#include "NativeClassicAnimationNameHash.h"
#include <algorithm>
#include <cstring>
#include <game/CAnimGatewayNames.h>
#include <game/CAnimGroupValidationPolicy.h>
#include <game/CAnimValidityPolicy.h>
#include <stdexcept>
#include <exception>
#include <new>
#include <string>
namespace mta::game_de
{
    namespace
    {
        NativeAnimationBlockName CopyName(const char* name)
        {
            if (!name)
                throw std::invalid_argument("DE animation block lookup requires a name");
            NativeAnimationBlockName copied{};
            for (std::size_t i = 0; i != copied.size(); ++i)
            {
                copied[i] = static_cast<unsigned char>(name[i]);
                if (!copied[i])
                    return copied;
            }
            throw std::invalid_argument("DE animation block lookup requires a terminated name within16bytes");
        }
        bool SameName(const NativeAnimationBlockName& query, const NativeAnimationBlockName& actual) noexcept
        {
            // The pinned native lookup folds ASCII only; equal high bytes
            // remain valid and a mismatch containing high bytes never folds.
            const auto fold = [](unsigned char value) { return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value; };
            for (std::size_t i = 0; i != query.size(); ++i)
            {
                if (query[i] != actual[i] && ((query[i] | actual[i]) & 0x80 || fold(query[i]) != fold(actual[i])))
                    return false;
                if (!query[i])
                    return true;
            }
            return false;
        }
        class CustomAnimationCleanupDE final : public CCustomAnimationCleanup
        {
        public:
            CustomAnimationCleanupDE(NativeCustomAnimationCleanupAccess& owner, std::uint64_t generation) noexcept : owner(owner), generation(generation) {}
            void Adopt(NativeCustomAnimationCleanupView&& issued, bool admitted) noexcept
            {
                view = std::move(issued);
                accepted = admitted;
            }
            SCustomAnimationCleanupResult Resume() noexcept override
            {
                constexpr SCustomAnimationCleanupResult refused{ECustomAnimationCleanupStatus::Refused, ECustomAnimationCleanupDisposition::Unresolved};
                const auto                              token = view.Token();
                if (!accepted || !view.IsIssued() || !generation || token.generation != generation || !token.command)
                    return refused;
                SCustomAnimationCleanupResult result;
                if (owner.ResumePrivateAnimationCleanup(token, result) != ClockResult::Ok)
                    return refused;
                // Generic status Ok is not resource disposal. Only the exact
                // provider's matching private detach and all release receipts
                // may authorize the common caller to drop CPU ownership.
                if (result.disposition != ECustomAnimationCleanupDisposition::Unresolved &&
                    result.disposition != ECustomAnimationCleanupDisposition::PrivateDetached)
                    return refused;
                switch (result.status)
                {
                    case ECustomAnimationCleanupStatus::Complete:
                        return result.disposition == ECustomAnimationCleanupDisposition::PrivateDetached ? result : refused;
                    case ECustomAnimationCleanupStatus::Pending:
                    case ECustomAnimationCleanupStatus::Refused:
                        return result;
                    default:
                        return refused;
                }
            }
            // The move-only view only decrements CPU facade-code lifetime.
            // No native release, command erasure or own-module unload runs.
        private:
            NativeCustomAnimationCleanupAccess& owner;
            const std::uint64_t                 generation;
            NativeCustomAnimationCleanupView    view;
            bool                                accepted = false;
        };
        class CustomAnimationSequenceOperationDE final : public CCustomAnimationSequenceOperation
        {
        public:
            CustomAnimationSequenceOperationDE(NativeCustomAnimationSequenceOperationAccess& owner, std::uint64_t generation,
                                               ECustomAnimationSequenceDisposition expected) noexcept
                : owner(owner), generation(generation), expected(expected)
            {
            }
            void Adopt(NativeCustomAnimationCleanupView&& issued, bool admitted) noexcept
            {
                view = std::move(issued);
                accepted = admitted;
            }
            SCustomAnimationSequenceResult Resume() noexcept override
            {
                constexpr SCustomAnimationSequenceResult refused{ECustomAnimationSequenceStatus::Refused, ECustomAnimationSequenceDisposition::Unresolved};
                const auto                               token = view.Token();
                if (!accepted || !view.IsIssued() || !generation || token.generation != generation || !token.command)
                    return refused;
                SCustomAnimationSequenceResult result;
                if (owner.ResumePrivateAnimationSequenceOperation(token, result) != ClockResult::Ok)
                    return refused;
                // A provably unentered Copy cancellation disposes the source
                // through that same original transfer journal. Return its exact
                // SourceReleased receipt without claiming a successful transfer;
                // the common caller still requires SourceTransferred to proceed.
                const bool cancellation = expected == ECustomAnimationSequenceDisposition::SourceTransferred &&
                                          result.disposition == ECustomAnimationSequenceDisposition::SourceReleased;
                if (result.disposition != ECustomAnimationSequenceDisposition::Unresolved && result.disposition != expected && !cancellation)
                    return refused;
                switch (result.status)
                {
                    case ECustomAnimationSequenceStatus::Complete:
                        return result.disposition == expected || cancellation ? result : refused;
                    case ECustomAnimationSequenceStatus::Pending:
                    case ECustomAnimationSequenceStatus::Refused:
                        return result;
                    default:
                        return refused;
                }
            }
            // The actual journal owns resources and accepted effects. This
            // facade's destruction only releases its CPU code-lifetime view.
        private:
            NativeCustomAnimationSequenceOperationAccess& owner;
            const std::uint64_t                           generation;
            const ECustomAnimationSequenceDisposition     expected;
            NativeCustomAnimationCleanupView              view;
            bool                                          accepted = false;
        };
        struct DeleteAssociationFacadeStorage
        {
            void operator()(void* value) const noexcept { ::operator delete(value); }
        };
        struct DeleteSequenceFacadeStorage
        {
            void operator()(void* value) const noexcept { ::operator delete(value); }
        };
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE animation manager owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
        }
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationBlockAccess& blocks, std::uint64_t generation)
        : native(manager), ownerGeneration(generation), blockAccess(&blocks)
    {
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&blocks))
            throw std::invalid_argument("DE animation references require one native owner for manager and blocks");
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationBlockAccess& blocks, NativeAnimationHierarchyAccess& hierarchy,
                                   std::uint64_t generation)
        : CAnimManagerDE(manager, blocks, generation)
    {
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&hierarchy))
            throw std::invalid_argument("DE animation hash lookup requires one native owner for manager, blocks and hierarchies");
        hierarchyAccess = &hierarchy;
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationBlockAccess& blocks, CGame& game, std::uint64_t generation)
        : native(manager), ownerGeneration(generation), blockAccess(&blocks), gameProvider(&game)
    {
        // Both polymorphic references must be subobjects of one CPU service
        // owner. A coincidental native image or callback generation cannot
        // make two independent owners interchangeable for a factory product.
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&blocks))
            throw std::invalid_argument("DE animation factory requires one native owner for manager and blocks");
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationSequenceStorageAccess& storage, std::uint64_t generation)
        : native(manager), ownerGeneration(generation), storageAccess(&storage)
    {
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&storage))
            throw std::invalid_argument("DE custom animation storage requires one native owner for manager and storage");
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationBlockAccess& blocks, NativeAnimationSequenceStorageAccess& storage,
                                   CGame& game, std::uint64_t generation)
        : CAnimManagerDE(manager, blocks, game, generation)
    {
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&storage))
            throw std::invalid_argument("DE custom animation storage requires one native owner for manager and storage");
        storageAccess = &storage;
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationHierarchyAccess& hierarchy, NativeAnimationSequenceAccess& sequence,
                                   NativeAnimationSequenceStorageAccess& storage, std::uint64_t generation)
        : CAnimManagerDE(manager, storage, generation)
    {
        const auto completeOwner = dynamic_cast<void*>(&manager);
        if (completeOwner != dynamic_cast<void*>(&hierarchy) || completeOwner != dynamic_cast<void*>(&sequence))
            throw std::invalid_argument("DE private animation factories require one native owner for every service");
        hierarchyAccess = &hierarchy;
        sequenceAccess = &sequence;
        // Cross-cast only from the already matched complete CPU owner. Old
        // observation constructors never silently acquire teardown authority.
        cleanupAccess = dynamic_cast<NativeCustomAnimationCleanupAccess*>(&manager);
        sequenceOperationAccess = dynamic_cast<NativeCustomAnimationSequenceOperationAccess*>(&manager);
    }
    CAnimManagerDE::AnimBlendAssoc_type CAnimManagerDE::CreateAnimAssociation(AssocGroupId requestedGroup, AnimationId requestedAnimation)
    {
        static_assert(sizeof(AssocGroupId) == sizeof(std::uint32_t) && sizeof(AnimationId) == sizeof(std::uint32_t));
        const auto group = static_cast<std::uint32_t>(requestedGroup);
        const auto animation = static_cast<std::uint32_t>(requestedAnimation);
        const auto generation = ownerGeneration;
        if (!groupAccess || !associationAccess || !generation)
            Require(ClockResult::NotInitialized, "CreateAnimAssociation");
        // This path composes actual group Copy directly. Requiring a CGame or
        // warming the owning group's block cache would add source policy.
        // Reserve the CPU product before any original native allocation.
        std::unique_ptr<void, DeleteAssociationFacadeStorage> memory(::operator new(sizeof(CAnimBlendAssociationDE)));
        NativeAnimationAssociationGroupReferenceToken         selected;
        Require(groupAccess->ResolveAnimationAssociationGroup(generation, group, selected), "CreateAnimAssociation");
        if (selected.ownerGeneration != generation || selected.index != group || !selected.locator)
            Require(ClockResult::BindingChanged, "CreateAnimAssociation");
        NativeAnimationAssociationIssuedCopy issued;
        Require(associationAccess->CopyPrivateAnimationAssociationFromGroup(selected, animation, issued), "CreateAnimAssociation");
        if (!issued.interfaceAddress)
        {
            if (issued.token.ownerGeneration || issued.token.serial || issued.code.IsIssued())
                Require(ClockResult::BindingChanged, "CreateAnimAssociation");
            return nullptr;
        }
        const auto codeToken = issued.code.Token();
        if (issued.token.ownerGeneration != generation || !issued.token.serial || !issued.code.IsIssued() ||
            codeToken.ownerGeneration != issued.token.ownerGeneration || codeToken.serial != issued.token.serial)
            Require(ClockResult::BindingChanged, "CreateAnimAssociation");
        // The same-call controller result is the issuance channel. A raw
        // locator, diagnostic receipt or second lookup cannot adopt storage.
        auto* facade = ::new (memory.get()) CAnimBlendAssociationDE(*associationAccess, issued.token, std::move(issued.code));
        memory.release();
        return AnimBlendAssoc_type(facade);
    }
    CAnimManagerDE::AnimBlendAssoc_type CAnimManagerDE::GetAnimBlendAssociation(CAnimBlendAssociationSAInterface* source)
    {
        const auto locator = reinterpret_cast<std::uintptr_t>(source);
        // Actual MTA returns null before creating a CPU wrapper. Missing
        // providers/generation must not turn that source null into a refusal.
        if (!locator)
            return nullptr;
        const auto generation = ownerGeneration;
        if (!associationAccess || !generation)
            Require(ClockResult::NotInitialized, "GetAnimBlendAssociation");
        // Reserve CPU storage before issuing another code-lifetime view. The
        // resolver may only select a record already issued by this same owner;
        // it performs no native copy/construction or arbitrary raw adoption.
        std::unique_ptr<void, DeleteAssociationFacadeStorage> memory(::operator new(sizeof(CAnimBlendAssociationDE)));
        NativeAnimationAssociationIssuedCopy                  issued;
        Require(associationAccess->ResolvePrivateAnimationAssociationInterface(generation, locator, issued), "GetAnimBlendAssociation");
        const auto codeToken = issued.code.Token();
        if (issued.interfaceAddress != locator || issued.token.ownerGeneration != generation || !issued.token.serial || !issued.code.IsIssued() ||
            codeToken.ownerGeneration != issued.token.ownerGeneration || codeToken.serial != issued.token.serial)
            Require(ClockResult::BindingChanged, "GetAnimBlendAssociation");
        // Base unique_ptr deletion releases this CPU product/view only. The
        // native ledger retains construction, mutation and retirement authority.
        auto* facade = ::new (memory.get()) CAnimBlendAssociationDE(*associationAccess, issued.token, std::move(issued.code));
        memory.release();
        return AnimBlendAssoc_type(facade);
    }
    CAnimManagerDE::StaticAssocIntface_type CAnimManagerDE::GetAnimStaticAssociation(eAnimGroup requestedGroup, eAnimID requestedAnimation) const
    {
        const auto group = static_cast<std::uint32_t>(requestedGroup);
        const auto animation = static_cast<std::uint32_t>(requestedAnimation);
        // Preserve the classic validity precheck before the actual static-row
        // selection. An unavailable provider must not look like absent data.
        if (!IsValidGroup(group))
            return nullptr;
        if (!staticAccess)
            Require(ClockResult::NotInitialized, "GetAnimStaticAssociation");
        NativeAnimationStaticAssociationReferenceToken token;
        Require(staticAccess->ResolveAnimationStaticAssociationById(ownerGeneration, group, animation, token), "GetAnimStaticAssociation");
        if (!ownerGeneration || token.ownerGeneration != ownerGeneration)
            Require(ClockResult::OwnerConflict, "GetAnimStaticAssociation");
        if (!token.locator)
            return nullptr;
        if (token.groupIndex != group)
            Require(ClockResult::OwnerConflict, "GetAnimStaticAssociation");
        // Construction is private to the matching manager; default deletion
        // through the common virtual destructor owns only this CPU wrapper.
        return StaticAssocIntface_type(new CAnimBlendStaticAssociationDE(*staticAccess, token));
    }
    CAnimManagerDE::StaticAssocIntface_type CAnimManagerDE::GetAnimStaticAssociation(CAnimBlendStaticAssociationSAInterface* source)
    {
        const auto locator = reinterpret_cast<std::uintptr_t>(source);
        if (!locator)
            return nullptr;
        if (!staticAccess)
            Require(ClockResult::NotInitialized, "GetAnimStaticAssociation(interface)");
        NativeAnimationStaticAssociationReferenceToken token;
        Require(staticAccess->ResolveAnimationStaticAssociationInterface(ownerGeneration, locator, token), "GetAnimStaticAssociation(interface)");
        if (!ownerGeneration || token.ownerGeneration != ownerGeneration)
            Require(ClockResult::OwnerConflict, "GetAnimStaticAssociation(interface)");
        if (!token.locator)
            return nullptr;
        if (token.locator != locator)
            Require(ClockResult::OwnerConflict, "GetAnimStaticAssociation(interface)");
        return StaticAssocIntface_type(new CAnimBlendStaticAssociationDE(*staticAccess, token));
    }
    std::unique_ptr<CCustomAnimationCleanup> CAnimManagerDE::BeginCustomAnimationCleanup(const SCustomAnimationCleanupRequest& request)
    {
        const auto copied = request;
        if (copied.origin != this)
            throw std::invalid_argument("DE cleanup requires the creating manager");
        if (!cleanupAccess || !hierarchyAccess)
            Require(ClockResult::NotInitialized, "BeginCustomAnimationCleanup");
        auto* hierarchy = dynamic_cast<CAnimBlendHierarchyDE*>(copied.hierarchy);
        if (!hierarchy || dynamic_cast<void*>(&hierarchy->native) != dynamic_cast<void*>(&native))
            throw std::invalid_argument("DE cleanup requires its same-owner private hierarchy facade");
        const auto generation = ownerGeneration;
        const auto reference = hierarchy->reference;
        if (!generation || reference.ownerGeneration != generation || !reference.locator)
            Require(ClockResult::BindingChanged, "BeginCustomAnimationCleanup");
        const NativeCustomAnimationCleanupRequest intent{generation, reference, reinterpret_cast<std::uintptr_t>(copied.originalArray), copied.capacity};
        // Reserve the actual CPU facade before any owner can accept native
        // claims. Adoption is noexcept, including an accepted late refusal.
        auto                                     ticket = std::make_unique<CustomAnimationCleanupDE>(*cleanupAccess, generation);
        NativeCustomAnimationCleanupBeginReceipt receipt;
        const auto                               result = cleanupAccess->BeginPrivateAnimationCleanup(intent, receipt);
        if (receipt.accepted || receipt.view.IsIssued())
        {
            ticket->Adopt(std::move(receipt.view), receipt.accepted);
            return ticket;
        }
        Require(result, "BeginCustomAnimationCleanup");
        Require(ClockResult::BindingChanged, "BeginCustomAnimationCleanup");
        return nullptr;
    }
    bool CAnimManagerDE::HasPendingCustomAnimationCleanup() const noexcept
    {
        // Missing/busy proof is retention, including partial observation-only
        // receivers. The actual owner counts commands and live completed views.
        return !cleanupAccess || cleanupAccess->HasPendingPrivateAnimationCleanup() ||
               (sequenceOperationAccess && sequenceOperationAccess->HasPendingPrivateAnimationSequenceOperations());
    }
    NativeAnimationSequenceReferenceToken CAnimManagerDE::SequenceOperationIdentity(CAnimBlendSequence* value, const char* operation) const
    {
        if (!sequenceOperationAccess || !sequenceAccess)
            Require(ClockResult::NotInitialized, operation);
        auto* sequence = dynamic_cast<CAnimBlendSequenceDE*>(value);
        if (!sequence || dynamic_cast<void*>(&sequence->native) != dynamic_cast<void*>(&native))
            throw std::invalid_argument("DE sequence operation requires its same-owner issued facade");
        // Read immutable CPU identity while the caller lends the facade.
        // No native getter can decide whether a prior write was accepted.
        const auto reference = sequence->reference;
        if (!ownerGeneration || reference.ownerGeneration != ownerGeneration || !reference.firstTag)
            Require(ClockResult::BindingChanged, operation);
        return reference;
    }

    std::unique_ptr<CAnimBlendSequence> CAnimManagerDE::GetCustomAnimBlendSequence()
    {
        if (!sequenceOperationAccess || !sequenceAccess || !ownerGeneration)
            Require(ClockResult::NotInitialized, "GetCustomAnimBlendSequence");
        // Allocate the CPU facade storage first. Its noexcept placement
        // constructor cannot strand a newly accepted native allocation if
        // the CPU allocator fails, and no guessed cleanup is needed here.
        std::unique_ptr<void, DeleteSequenceFacadeStorage> memory(::operator new(sizeof(CAnimBlendSequenceDE)));
        NativeAnimationSequenceReferenceToken              reference;
        Require(sequenceOperationAccess->AllocatePrivateStandaloneAnimationSequence(ownerGeneration, reference), "GetCustomAnimBlendSequence");
        if (!reference.firstTag)
            throw std::bad_alloc();
        if (reference.ownerGeneration != ownerGeneration || reference.index)
            Require(ClockResult::BindingChanged, "GetCustomAnimBlendSequence");
        auto* facade = ::new (memory.get()) CAnimBlendSequenceDE(*sequenceAccess, reference);
        memory.release();
        return std::unique_ptr<CAnimBlendSequence>(facade);
    }

    NativeAnimationSequenceReferenceToken CAnimManagerDE::ResolveCustomSequenceInterface(std::uintptr_t locator, const char* operation) const
    {
        if (!sequenceOperationAccess || !ownerGeneration)
            Require(ClockResult::NotInitialized, operation);
        const auto                              generation = ownerGeneration;
        const auto                              requestedAddress = locator;
        NativeAnimationSequenceResolvedIdentity identity;
        Require(sequenceOperationAccess->ResolvePrivateAnimationSequenceReference(generation, requestedAddress, identity), operation);
        if (identity.reference.ownerGeneration != generation || !identity.reference.firstTag || identity.interfaceAddress != requestedAddress)
            Require(ClockResult::BindingChanged, operation);
        // The provider resolved the actual row and full index. A raw locator
        // never authorizes client-side row arithmetic or native-bank adoption.
        return identity.reference;
    }
    std::unique_ptr<CAnimBlendSequence> CAnimManagerDE::GetCustomAnimBlendSequence(CAnimBlendSequenceSAInterface* raw)
    {
        if (!raw)
            return nullptr;
        if (!sequenceAccess)
            Require(ClockResult::NotInitialized, "GetCustomAnimBlendSequence");
        const auto locator = reinterpret_cast<std::uintptr_t>(raw);
        const auto reference = ResolveCustomSequenceInterface(locator, "GetCustomAnimBlendSequence");
        // This current same-owner borrow may identify an ArrayRow or a genuine
        // Standalone purpose. CPU allocation failure deletes neither resource.
        return std::unique_ptr<CAnimBlendSequence>(new CAnimBlendSequenceDE(*sequenceAccess, reference));
    }
    void CAnimManagerDE::DeleteCustomAnimSequenceInterface(CAnimBlendSequenceSAInterface* raw)
    {
        if (!raw)
            return;
        const auto locator = reinterpret_cast<std::uintptr_t>(raw);
        const auto reference = ResolveCustomSequenceInterface(locator, "DeleteCustomAnimSequenceInterface");
        // Only the dedicated service can authorize standalone metadata release.
        // Frames stay live/unbound/otherwise held; typed cleanup is a different
        // contract and must never substitute for this legacy void operation.
        Require(sequenceOperationAccess->DeletePrivateStandaloneAnimationSequenceInterface(reference), "DeleteCustomAnimSequenceInterface");
    }

    std::unique_ptr<CCustomAnimationSequenceOperation> CAnimManagerDE::BeginCustomAnimationSequenceCleanup(
        const SCustomAnimationSequenceCleanupRequest& request)
    {
        const auto copied = request;
        if (copied.origin != this)
            throw std::invalid_argument("DE sequence cleanup requires the creating manager");
        const auto source = SequenceOperationIdentity(copied.source, "BeginCustomAnimationSequenceCleanup");
        auto       ticket = std::make_unique<CustomAnimationSequenceOperationDE>(*sequenceOperationAccess, ownerGeneration,
                                                                                 ECustomAnimationSequenceDisposition::SourceReleased);
        NativeCustomAnimationSequenceOperationBeginReceipt receipt;
        const auto                                         result = sequenceOperationAccess->BeginPrivateAnimationSequenceCleanup(source, receipt);
        if (receipt.accepted || receipt.view.IsIssued())
        {
            ticket->Adopt(std::move(receipt.view), receipt.accepted);
            return ticket;
        }
        Require(result, "BeginCustomAnimationSequenceCleanup");
        Require(ClockResult::BindingChanged, "BeginCustomAnimationSequenceCleanup");
        return nullptr;
    }

    std::unique_ptr<CCustomAnimationSequenceOperation> CAnimManagerDE::BeginCustomAnimationSequenceTransfer(
        const SCustomAnimationSequenceTransferRequest& request)
    {
        const auto copied = request;
        if (copied.origin != this)
            throw std::invalid_argument("DE sequence transfer requires the creating manager");
        const auto source = SequenceOperationIdentity(copied.source, "BeginCustomAnimationSequenceTransfer");
        const auto destination = SequenceOperationIdentity(copied.destination, "BeginCustomAnimationSequenceTransfer");
        auto       ticket = std::make_unique<CustomAnimationSequenceOperationDE>(*sequenceOperationAccess, ownerGeneration,
                                                                                 ECustomAnimationSequenceDisposition::SourceTransferred);
        NativeCustomAnimationSequenceOperationBeginReceipt receipt;
        const auto result = sequenceOperationAccess->BeginPrivateAnimationSequenceTransfer(source, destination, receipt);
        if (receipt.accepted || receipt.view.IsIssued())
        {
            ticket->Adopt(std::move(receipt.view), receipt.accepted);
            return ticket;
        }
        Require(result, "BeginCustomAnimationSequenceTransfer");
        Require(ClockResult::BindingChanged, "BeginCustomAnimationSequenceTransfer");
        return nullptr;
    }

    std::unique_ptr<CCustomAnimationSequenceOperation> CAnimManagerDE::BeginCustomAnimationFrameAttachment(
        const SCustomAnimationFrameAttachmentRequest& request)
    {
        const auto copied = request;
        if (copied.origin != this)
            throw std::invalid_argument("DE frame attachment requires the creating manager");
        const auto target = SequenceOperationIdentity(copied.target, "BeginCustomAnimationFrameAttachment");
        auto       ticket = std::make_unique<CustomAnimationSequenceOperationDE>(*sequenceOperationAccess, ownerGeneration,
                                                                                 ECustomAnimationSequenceDisposition::FramesAttached);
        NativeCustomAnimationSequenceOperationBeginReceipt receipt;
        const auto result = sequenceOperationAccess->BeginPrivateAnimationFrameAttachment(target, copied.count, copied.root, copied.compressed,
                                                                                          reinterpret_cast<std::uintptr_t>(copied.originalFrames), receipt);
        if (receipt.accepted || receipt.view.IsIssued())
        {
            ticket->Adopt(std::move(receipt.view), receipt.accepted);
            return ticket;
        }
        Require(result, "BeginCustomAnimationFrameAttachment");
        Require(ClockResult::BindingChanged, "BeginCustomAnimationFrameAttachment");
        return nullptr;
    }

    std::unique_ptr<CAnimBlendHierarchy> CAnimManagerDE::GetAnimation(const char* name, std::unique_ptr<CAnimBlock>& block)
    {
        // Addressed classic 4D42F0 hashes the entire source name then calls
        // the hash-in-block search. Reuse its ASCII normalization/raw key;
        // native interning/CRT folding and sequence/block name limits differ.
        const auto hash = NativeClassicAnimationNamePolicy::Hash(name);
        return CAnimManagerDE::GetAnimation(hash, block);
    }
    std::unique_ptr<CAnimBlendHierarchy> CAnimManagerDE::GetAnimation(unsigned int sourceHash, std::unique_ptr<CAnimBlock>& block)
    {
        if (!blockAccess || !hierarchyAccess || !ownerGeneration)
            Require(ClockResult::NotInitialized, "GetAnimation(hash, block)");
        auto* facade = dynamic_cast<CAnimBlockDE*>(block.get());
        if (!facade || dynamic_cast<void*>(&facade->native) != dynamic_cast<void*>(&native))
            throw std::invalid_argument("DE animation hash lookup requires its same-owner block facade");
        static_assert(sizeof(sourceHash) == sizeof(std::uint32_t));
        const auto generation = ownerGeneration;
        const auto expected = facade->identity;
        const auto hash = static_cast<std::uint32_t>(sourceHash);
        if (expected.ownerGeneration != generation)
            Require(ClockResult::BindingChanged, "GetAnimation(hash, block)");
        std::uintptr_t address = 0;
        // The classic entry 4D39F0 takes a hash despite the SDK parameter's
        // old uiIndex name. Only the current complete owner may scan its block
        // range and prove the first match or a coherent missing result.
        Require(blockAccess->FindAnimationBlockHierarchy(expected, hash, address), "GetAnimation(hash, block)");
        if (!address)
            return nullptr;
        NativeAnimationHierarchyReferenceToken reference;
        Require(hierarchyAccess->ResolveBankAnimationHierarchyReference(generation, address, reference), "GetAnimation(hash, block)");
        if (reference.ownerGeneration != generation || reference.locator != address)
            Require(ClockResult::BindingChanged, "GetAnimation(hash, block)");
        // No virtual raw-interface getter, client row arithmetic, load/ref
        // change or lifetime adoption is needed to create this CPU borrow.
        return std::unique_ptr<CAnimBlendHierarchy>(new CAnimBlendHierarchyDE(*hierarchyAccess, reference));
    }
    std::unique_ptr<CAnimBlendHierarchy> CAnimManagerDE::GetAnimation(int index)
    {
        if (!hierarchyAccess || !ownerGeneration)
            Require(ClockResult::NotInitialized, "GetAnimation");
        // The proven domain is a current bank ordinal/range. Preserve refusal
        // for negative, out-of-range or stale requests instead of inventing the
        // original engine's behavior outside that qualified domain.
        if (index < 0)
            Require(ClockResult::InvalidArgument, "GetAnimation");
        NativeAnimationHierarchyReferenceToken reference;
        Require(hierarchyAccess->ResolveBankAnimationHierarchyAt(ownerGeneration, static_cast<std::uint32_t>(index), reference), "GetAnimation");
        if (reference.ownerGeneration != ownerGeneration || !reference.locator)
            Require(ClockResult::BindingChanged, "GetAnimation");
        return std::unique_ptr<CAnimBlendHierarchy>(new CAnimBlendHierarchyDE(*hierarchyAccess, reference));
    }

    std::unique_ptr<CAnimBlendHierarchy> CAnimManagerDE::GetAnimBlendHierarchy(CAnimBlendHierarchySAInterface* raw)
    {
        if (!raw)
            return nullptr;
        if (!hierarchyAccess || !ownerGeneration)
            Require(ClockResult::NotInitialized, "GetAnimBlendHierarchy");
        const auto                             locator = reinterpret_cast<std::uintptr_t>(raw);
        NativeAnimationHierarchyReferenceToken reference;
        Require(hierarchyAccess->ResolveBankAnimationHierarchyReference(ownerGeneration, locator, reference), "GetAnimBlendHierarchy");
        if (reference.ownerGeneration != ownerGeneration || reference.locator != locator)
            Require(ClockResult::BindingChanged, "GetAnimBlendHierarchy");
        // This CPU facade borrows the bank through the same native service.
        // It owns no row, child buffer, cache entry or native deletion right.
        return std::unique_ptr<CAnimBlendHierarchy>(new CAnimBlendHierarchyDE(*hierarchyAccess, reference));
    }

    std::unique_ptr<CAnimBlendHierarchy> CAnimManagerDE::GetCustomAnimBlendHierarchy()
    {
        if (!hierarchyAccess)
            Require(ClockResult::NotInitialized, "GetCustomAnimBlendHierarchy");
        const auto                             generation = ownerGeneration;
        NativeAnimationHierarchyReferenceToken token;
        Require(hierarchyAccess->CreatePrivateAnimationHierarchy(generation, token), "GetCustomAnimBlendHierarchy");
        if (!generation || token.ownerGeneration != generation || !token.locator)
            Require(ClockResult::BindingChanged, "GetCustomAnimBlendHierarchy");
        try
        {
            return std::unique_ptr<CAnimBlendHierarchy>(new CAnimBlendHierarchyDE(*hierarchyAccess, token));
        }
        catch (...)
        {
            // The owner issued a real CPU40 record before facade allocation.
            // Its matching service owns cleanup and any still-outstanding
            // debt after lost acknowledgement; absence is not cleanup success.
            const auto cleanup = hierarchyAccess->DestroyPrivateAnimationHierarchy(token);
            if (cleanup != ClockResult::Ok)
                std::throw_with_nested(std::runtime_error("DE hierarchy facade construction failed; owner cleanup acknowledgement refused: " +
                                                          std::to_string(static_cast<unsigned>(cleanup))));
            throw;
        }
    }
    std::unique_ptr<CAnimBlendHierarchy> CAnimManagerDE::GetCustomAnimBlendHierarchy(CAnimBlendHierarchySAInterface* hierarchy)
    {
        if (!hierarchy)
            return nullptr;
        if (!hierarchyAccess)
            Require(ClockResult::NotInitialized, "GetCustomAnimBlendHierarchy");
        const auto                             generation = ownerGeneration;
        const auto                             locator = reinterpret_cast<std::uintptr_t>(hierarchy);
        NativeAnimationHierarchyReferenceToken token;
        Require(hierarchyAccess->ResolvePrivateAnimationHierarchyReference(generation, locator, token), "GetCustomAnimBlendHierarchy");
        if (!generation || token.ownerGeneration != generation || token.locator != locator)
            Require(ClockResult::BindingChanged, "GetCustomAnimBlendHierarchy");
        // This is a CPU-only borrowed clone. Allocation failure cannot delete
        // the pre-existing native owner's record or grant a new lifetime.
        return std::unique_ptr<CAnimBlendHierarchy>(new CAnimBlendHierarchyDE(*hierarchyAccess, token));
    }
    void CAnimManagerDE::DeleteCustomAnimHierarchyInterface(CAnimBlendHierarchySAInterface* hierarchy)
    {
        // Preserve classic delete-null's no-resource CPU path. A nonnull tag
        // must reach the actual private command/retirement service.
        if (!hierarchy)
            return;
        if (!hierarchyAccess)
            Require(ClockResult::NotInitialized, "DeleteCustomAnimHierarchyInterface");
        const NativeAnimationHierarchyReferenceToken token{ownerGeneration, reinterpret_cast<std::uintptr_t>(hierarchy)};
        Require(hierarchyAccess->DestroyPrivateAnimationHierarchy(token), "DeleteCustomAnimHierarchyInterface");
    }
    std::unique_ptr<CAnimBlendSequence> CAnimManagerDE::GetCustomAnimBlendSequenceAt(CAnimBlendSequenceSAInterface* originalArray, DWORD index)
    {
        if (!originalArray)
            return nullptr;
        if (!sequenceAccess)
            Require(ClockResult::NotInitialized, "GetCustomAnimBlendSequenceAt");
        static_assert(sizeof(index) == sizeof(std::uint32_t));
        const auto                            generation = ownerGeneration;
        const auto                            firstTag = reinterpret_cast<std::uintptr_t>(originalArray);
        const auto                            ordinal = static_cast<std::uint32_t>(index);
        bool                                  present = false;
        NativeAnimationSequenceReferenceToken token;
        Require(sequenceAccess->ResolvePrivateAnimationSequenceAt(generation, firstTag, ordinal, present, token), "GetCustomAnimBlendSequenceAt");
        // Only the qualified owner's explicit successful absence represents
        // source At's out-of-capacity null. Binding failures remain failures.
        if (!present)
            return nullptr;
        if (!generation || token.ownerGeneration != generation || token.firstTag != firstTag || token.index != static_cast<std::size_t>(ordinal))
            Require(ClockResult::BindingChanged, "GetCustomAnimBlendSequenceAt");
        return std::unique_ptr<CAnimBlendSequence>(new CAnimBlendSequenceDE(*sequenceAccess, token));
    }
    CAnimBlendSequenceSAInterface* CAnimManagerDE::AllocateCustomAnimSequences(std::size_t capacity)
    {
        if (!storageAccess)
            Require(ClockResult::NotInitialized, "AllocateCustomAnimSequences");
        // Zero capacity still dispatches: only the current admitted runtime
        // may authorize even a null result for this borrowed generation.
        const auto     generation = ownerGeneration;
        std::uintptr_t tag = 0;
        Require(storageAccess->AllocateAnimationSequences(generation, capacity, tag), "AllocateCustomAnimSequences");
        if (capacity && !tag)
            Require(ClockResult::WriteFailure, "AllocateCustomAnimSequences");
        if (!capacity && tag)
            Require(ClockResult::BindingChanged, "AllocateCustomAnimSequences");
        return reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag);
    }
    void CAnimManagerDE::FreeCustomAnimSequences(CAnimBlendSequenceSAInterface* tag)
    {
        if (!storageAccess)
            Require(ClockResult::NotInitialized, "FreeCustomAnimSequences");
        // The real controller maps only its currently issued, unfreed first
        // tags to retained records. A raw SDK tag adds no serial/ABA promise,
        // native bank adoption, frame cleanup or destructor-owned retirement.
        const auto generation = ownerGeneration;
        const auto issuedTag = reinterpret_cast<std::uintptr_t>(tag);
        Require(storageAccess->FreeAnimationSequences(generation, issuedTag), "FreeCustomAnimSequences");
    }
    BYTE* CAnimManagerDE::AllocateKeyFramesMemory(std::uint32_t bytes)
    {
        if (!storageAccess)
            Require(ClockResult::NotInitialized, "AllocateKeyFramesMemory");
        const auto     generation = ownerGeneration;
        const auto     requestedBytes = bytes;
        std::uintptr_t address = 0;
        Require(storageAccess->AllocateAnimationKeyFrames(generation, requestedBytes, address), "AllocateKeyFramesMemory");
        // Actual MTA callers handle an allocator's null result. The owner
        // admits that fact separately from a binding/read/write refusal;
        // array-specific nonzero/null rules must not replace this contract.
        return reinterpret_cast<BYTE*>(address);
    }
    void CAnimManagerDE::FreeKeyFramesMemory(void* frames)
    {
        if (!storageAccess)
            Require(ClockResult::NotInitialized, "FreeKeyFramesMemory");
        // Null still reaches current-owner admission. The SDK adds neither
        // allocation adoption nor frame ownership to CPU facade destruction.
        const auto generation = ownerGeneration;
        const auto address = reinterpret_cast<std::uintptr_t>(frames);
        Require(storageAccess->FreeAnimationKeyFrames(generation, address), "FreeKeyFramesMemory");
    }
    NativeAnimationBlockIdentity CAnimManagerDE::ReadCounterIdentity(int index, const char* operation) const
    {
        if (!blockAccess)
            Require(ClockResult::NotInitialized, operation);
        static_assert(sizeof(int) == sizeof(std::int32_t));
        const int requestedIndex = index;
        if (requestedIndex < 0)
            Require(ClockResult::InvalidArgument, operation);
        const auto                   generation = ownerGeneration;
        const auto                   ordinal = static_cast<std::uint32_t>(requestedIndex);
        NativeAnimationBlockIdentity identity;
        Require(native.ReadAnimationBlockCounterIdentity(generation, ordinal, identity), operation);
        if (identity.index != ordinal || identity.ownerGeneration != generation)
            Require(ClockResult::BindingChanged, operation);
        // This is raw16 affinity, not a C-string lookup or factory identity.
        // Native counter admission owns current row/count/name coherence.
        return identity;
    }
    void CAnimManagerDE::AddAnimBlockRef(int index)
    {
        const auto identity = ReadCounterIdentity(index, "AddAnimBlockRef");
        Require(blockAccess->AddAnimationBlockCounterRef(identity), "AddAnimBlockRef");
    }
    int CAnimManagerDE::GetNumRefsToAnimBlock(int index)
    {
        const auto    identity = ReadCounterIdentity(index, "GetNumRefsToAnimBlock");
        std::uint16_t bits = 0;
        Require(blockAccess->ReadAnimationBlockCounterRefs(identity, bits), "GetNumRefsToAnimBlock");
        // The addressed manager getter promotes a signed int16 field, unlike
        // CAnimBlock::GetRefs which deliberately exposes raw unsigned16 bits.
        std::int16_t value = 0;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    void CAnimManagerDE::RemoveAnimBlockRefWithoutDelete(int index)
    {
        const auto identity = ReadCounterIdentity(index, "RemoveAnimBlockRefWithoutDelete");
        // Genuine original policy decrements modulo16 even at zero. Cleanup,
        // unload and the distinct RemoveAnimBlockRef remain separate APIs.
        Require(blockAccess->RemoveAnimationBlockRefWithoutDelete(identity), "RemoveAnimBlockRefWithoutDelete");
    }
    void CAnimManagerDE::RequireFactory(const char* operation) const
    {
        if (!blockAccess || !gameProvider)
            Require(ClockResult::NotInitialized, operation);
    }
    CAnimManagerDE::AnimAssocGroup_type CAnimManagerDE::MakeGroup(const NativeAnimationAssociationGroupReferenceToken& token, const char* operation) const
    {
        RequireFactory(operation);
        if (!groupAccess || !ownerGeneration)
            Require(ClockResult::NotInitialized, operation);
        if (token.ownerGeneration != ownerGeneration)
            Require(ClockResult::OwnerConflict, operation);
        if (!token.locator)
            Require(ClockResult::BindingChanged, operation);
        AnimAssocGroup_type result(new CAnimBlendAssocGroupDE(*groupAccess, token, *gameProvider));
        // Classic construction calls SetupAnimBlock once. Match that warm
        // through the actual DE path; unique_ptr cleans CPU caches on refusal.
        (void)result->GetAnimBlock();
        return result;
    }
    CAnimManagerDE::AnimAssocGroup_type CAnimManagerDE::GetAnimBlendAssoc(AssocGroupId requested) const
    {
        static_assert(sizeof(AssocGroupId) == sizeof(std::uint32_t));
        const auto index = static_cast<std::uint32_t>(requested);
        const auto generation = ownerGeneration;
        RequireFactory("GetAnimBlendAssoc");
        if (!groupAccess || !generation)
            Require(ClockResult::NotInitialized, "GetAnimBlendAssoc");
        NativeAnimationAssociationGroupReferenceToken token;
        Require(groupAccess->ResolveAnimationAssociationGroup(generation, index, token), "GetAnimBlendAssoc");
        if (token.ownerGeneration != generation)
            Require(ClockResult::OwnerConflict, "GetAnimBlendAssoc");
        if (token.index != index || !token.locator)
            Require(ClockResult::BindingChanged, "GetAnimBlendAssoc");
        return MakeGroup(token, "GetAnimBlendAssoc");
    }
    CAnimManagerDE::AnimAssocGroup_type CAnimManagerDE::GetAnimBlendAssocGroup(CAnimBlendAssocGroupSAInterface* raw)
    {
        const auto locator = reinterpret_cast<std::uintptr_t>(raw);
        if (!locator)
            return nullptr;
        const auto generation = ownerGeneration;
        RequireFactory("GetAnimBlendAssocGroup");
        if (!groupAccess || !generation)
            Require(ClockResult::NotInitialized, "GetAnimBlendAssocGroup");
        NativeAnimationAssociationGroupReferenceToken token;
        Require(groupAccess->ResolveAnimationAssociationGroupInterface(generation, locator, token), "GetAnimBlendAssocGroup");
        if (token.ownerGeneration != generation || token.locator != locator)
            Require(ClockResult::OwnerConflict, "GetAnimBlendAssocGroup");
        // The raw legacy type is only an opaque request for a freshly admitted
        // current bank row. It is never cast into a native or CPU object.
        return MakeGroup(token, "GetAnimBlendAssocGroup");
    }
    std::unique_ptr<CAnimBlock> CAnimManagerDE::MakeBlock(const NativeAnimationBlockIdentity& identity, const char* operation)
    {
        RequireFactory(operation);
        if (identity.ownerGeneration != ownerGeneration || std::find(identity.name.begin(), identity.name.end(), 0) == identity.name.end())
            Require(ClockResult::BindingChanged, operation);
        // This one allocation owns only the CPU facade. Native block refs,
        // loaded state and the borrowed game remain unchanged on destruction.
        return std::unique_ptr<CAnimBlock>(new CAnimBlockRequestDE(*blockAccess, identity, *gameProvider));
    }
    std::unique_ptr<CAnimBlock> CAnimManagerDE::GetAnimationBlock(int index)
    {
        RequireFactory("GetAnimationBlock");
        if (index < 0)
            Require(ClockResult::InvalidArgument, "GetAnimationBlock");
        NativeAnimationBlockIdentity identity;
        Require(native.ReadAnimationBlockIdentity(ownerGeneration, static_cast<std::uint32_t>(index), identity), "GetAnimationBlock");
        if (identity.index != static_cast<std::uint32_t>(index))
            Require(ClockResult::BindingChanged, "GetAnimationBlock");
        // The current native row, rather than a name lookup or cached count,
        // supplies the exact identity for this integer factory overload too.
        return MakeBlock(identity, "GetAnimationBlock");
    }
    std::unique_ptr<CAnimBlock> CAnimManagerDE::GetAnimationBlock(const char* name)
    {
        const auto query = CopyName(name);
        RequireFactory("GetAnimationBlock");
        std::int32_t index = -1;
        Require(native.FindAnimationBlockIndex(ownerGeneration, query, index), "GetAnimationBlock");
        if (index == -1)
            return nullptr;
        if (index < -1)
            Require(ClockResult::BindingChanged, "GetAnimationBlock");
        NativeAnimationBlockIdentity identity;
        Require(native.ReadAnimationBlockIdentity(ownerGeneration, static_cast<std::uint32_t>(index), identity), "GetAnimationBlock");
        if (identity.index != static_cast<std::uint32_t>(index) || !SameName(query, identity.name))
            Require(ClockResult::BindingChanged, "GetAnimationBlock");
        // Preserve the actual selected row's complete mutable16-byte name,
        // not the query's case-fold spelling or a fabricated zero-filled tail.
        return MakeBlock(identity, "GetAnimationBlock");
    }
    std::unique_ptr<CAnimBlock> CAnimManagerDE::GetAnimBlock(CAnimBlockSAInterface* tag)
    {
        // Classic's null clone path never queries a game or native service.
        if (!tag)
            return nullptr;
        RequireFactory("GetAnimBlock");
        NativeAnimationBlockIdentity identity;
        Require(native.ReadAnimationBlockInterfaceIdentity(ownerGeneration, reinterpret_cast<std::uintptr_t>(tag), identity), "GetAnimBlock");
        return MakeBlock(identity, "GetAnimBlock");
    }
    bool CAnimManagerDE::isGateWayAnimationHierarchy(CAnimBlendHierarchySAInterface* hierarchy)
    {
        const auto    generation = ownerGeneration;
        const auto    locator = reinterpret_cast<std::uintptr_t>(hierarchy);
        std::uint32_t actualHash = 0;
        Require(native.ReadAnimationHierarchyHash(generation, locator, actualHash), "isGateWayAnimationHierarchy");
        // Actual MTA compares its authored gateway key with this raw hash.
        // The existing key service is pure CPU policy, not the native name
        // interning helper, and the opaque pointer is never dereferenced here.
        CKeyGenDE keys;
        return keys.GetUppercaseKey(MtaGame::CAnimGatewayNames::GetAnimationName()) == actualHash;
    }
    void CAnimManagerDE::RemoveFromUncompressedCache(CAnimBlendHierarchy* hierarchy)
    {
        if (!hierarchyAccess || !ownerGeneration)
            Require(ClockResult::NotInitialized, "RemoveFromUncompressedCache");
        auto* facade = dynamic_cast<CAnimBlendHierarchyDE*>(hierarchy);
        if (!facade || dynamic_cast<void*>(&facade->native) != dynamic_cast<void*>(&native))
            throw std::invalid_argument("DE cache removal requires its same-owner hierarchy facade");
        const auto token = facade->reference;
        if (token.ownerGeneration != ownerGeneration || !token.locator)
            Require(ClockResult::BindingChanged, "RemoveFromUncompressedCache");
        // Copy immutable facade affinity directly. A virtual GetInterface
        // observation cannot grant ownership or turn a foreign/bank cache
        // into the qualified private cache0 domain supplied by this service.
        Require(hierarchyAccess->RemovePrivateAnimationHierarchyFromUncompressedCache(token), "RemoveFromUncompressedCache");
    }
    void CAnimManagerDE::RemoveFromUncompressedCache(CAnimBlendHierarchySAInterface* hierarchy)
    {
        if (!hierarchyAccess || !ownerGeneration)
            Require(ClockResult::NotInitialized, "RemoveFromUncompressedCache");
        const auto generation = ownerGeneration;
        const auto locator = reinterpret_cast<std::uintptr_t>(hierarchy);
        if (!locator)
            Require(ClockResult::InvalidArgument, "RemoveFromUncompressedCache");
        NativeAnimationHierarchyReferenceToken token;
        Require(hierarchyAccess->ResolvePrivateAnimationHierarchyReference(generation, locator, token), "RemoveFromUncompressedCache");
        if (token.ownerGeneration != generation || token.locator != locator)
            Require(ClockResult::BindingChanged, "RemoveFromUncompressedCache");
        // A raw interface must first resolve to a current issued private row.
        // Only the existing cache0/linked-child gate may admit the operation;
        // no bank adoption, cache splice, uncompress or frame release is added.
        Require(hierarchyAccess->RemovePrivateAnimationHierarchyFromUncompressedCache(token), "RemoveFromUncompressedCache");
    }

    NativeAnimationManagerCounts CAnimManagerDE::Read(const char* operation) const
    {
        NativeAnimationManagerCounts next;
        Require(native.ReadAnimationManagerCounts(ownerGeneration, next), operation);
        return next;
    }
    int CAnimManagerDE::GetNumAnimations()
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        return Read("GetNumAnimations").animations;
    }
    int CAnimManagerDE::GetNumAnimBlocks()
    {
        return Read("GetNumAnimBlocks").blocks;
    }
    // Gateway names belong to MTA itself. A native observation refusal must
    // not turn these authored constants into missing or synthetic data.
    const char* CAnimManagerDE::GetGateWayBlockName() const
    {
        return MtaGame::CAnimGatewayNames::GetBlockName();
    }
    const char* CAnimManagerDE::GetGateWayAnimationName() const
    {
        return MtaGame::CAnimGatewayNames::GetAnimationName();
    }
    int CAnimManagerDE::GetNumAnimAssocDefinitions()
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        const auto   generation = ownerGeneration;
        std::int32_t count = 0;
        Require(native.ReadAnimationAssocDefinitionCount(generation, count), "GetNumAnimAssocDefinitions");
        // This source getter reads a signed scalar. A positive name-table
        // extent is a separate admission rule, not a clamp on these bits.
        return static_cast<int>(count);
    }
    const char* CAnimManagerDE::GetAnimGroupName(AssocGroupId group)
    {
        static_assert(sizeof(group) == sizeof(std::uint32_t));
        const auto     generation = ownerGeneration;
        const auto     id = static_cast<std::uint32_t>(group);
        std::uintptr_t address = 0;
        Require(native.ReadAnimationGroupName(generation, id, address), "GetAnimGroupName");
        if (!address)
            Require(ClockResult::BindingChanged, "GetAnimGroupName");
        // This is the current native descriptor's exact borrowed name, not
        // a retained CPU copy or authority to publish an association group.
        // Native count/row/name bookends own the full-ID range admission.
        return reinterpret_cast<const char*>(address);
    }
    const char* CAnimManagerDE::GetAnimBlockName(AssocGroupId group)
    {
        static_assert(sizeof(group) == sizeof(std::uint32_t));
        const auto     generation = ownerGeneration;
        const auto     id = static_cast<std::uint32_t>(group);
        std::uintptr_t address = 0;
        Require(native.ReadAnimationGroupBlockName(generation, id, address), "GetAnimBlockName");
        if (!address)
            Require(ClockResult::BindingChanged, "GetAnimBlockName");
        // This is definition.BlockName, distinct from its GroupName or a
        // separately indexed animation block. Only the owner knows its row.
        return reinterpret_cast<const char*>(address);
    }
    AssocGroupId CAnimManagerDE::GetFirstAssocGroup(const char* name)
    {
        static_assert(sizeof(AssocGroupId) == sizeof(std::uint32_t));
        if (!name)
            throw std::invalid_argument("DE first association-group lookup requires a nonnull name");
        const auto        generation = ownerGeneration;
        const std::string copied(name);
        std::uint32_t     group = 0;
        // FF1070 searches the fixed first118 definition BlockNames and returns
        // 118 on a qualified miss. Copy the entire source input before callbacks;
        // the mutable definition count cannot substitute for that prefix.
        Require(native.FindFirstAnimationGroupByBlockName(generation, copied, group), "GetFirstAssocGroup");
        if (group > NativeAnimationAssociationGroupPrefixCount)
            Require(ClockResult::BindingChanged, "GetFirstAssocGroup");
        return static_cast<AssocGroupId>(group);
    }
    bool CAnimManagerDE::IsValidGroup(std::uint32_t group) const
    {
        const auto generation = ownerGeneration;
        return MtaGame::CAnimGroupValidationPolicy::IsValidGroup(group,
                                                                 [this, generation](std::uint32_t id)
                                                                 {
                                                                     bool created = false;
                                                                     // The actual current group-array owner supplies NumAnims!=0.
                                                                     // A missing/foreign lifetime is a refusal, never an unavailable
                                                                     // provider's false or a guessed association-pointer predicate.
                                                                     Require(native.ReadAnimationGroupCreated(generation, id, created), "IsValidGroup");
                                                                     return created;
                                                                 });
    }
    bool CAnimManagerDE::IsValidAnim(std::uint32_t group, std::uint32_t animation) const
    {
        const auto generation = ownerGeneration;
        return MtaGame::CAnimValidityPolicy::IsValidAnim(
            group, animation,
            [this, generation](std::uint32_t copiedGroup, std::uint32_t copiedAnimation, std::int16_t& selected)
            {
                // Match the actual GetAnimStaticAssociation group precheck.
                // Only its admitted current group may select a child ID.
                if (!IsValidGroup(copiedGroup))
                    return false;
                Require(native.ReadAnimationStaticAssociationAnimID(generation, copiedGroup, copiedAnimation, selected), "IsValidAnim");
                return true;
            });
    }
    int CAnimManagerDE::GetAnimationBlockIndex(const char* name)
    {
        const auto   copied = CopyName(name);
        std::int32_t index = -1;
        Require(native.FindAnimationBlockIndex(ownerGeneration, copied, index), "GetAnimationBlockIndex");
        if (index < -1)
            throw std::runtime_error("DE animation manager owner returned an invalid missing-name sentinel");
        // The genuine named native caller returns -1 for a missing block. Do
        // not turn an owner failure into that valid, observable result.
        return index;
    }
}

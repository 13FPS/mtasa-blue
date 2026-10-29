/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual animation-manager observations through one borrowed owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationManagerAccess.h"
#include "NativeAnimationStaticAssociationAccess.h"
#include "NativeAnimationSequenceStorageAccess.h"
#include "NativeAnimationHierarchyAccess.h"
#include "NativeAnimationSequenceAccess.h"
#include "NativeCustomAnimationCleanupAccess.h"
#include "NativeCustomAnimationSequenceOperationAccess.h"
// The common SDK expects BYTE/DWORD from the classic PCH. Standalone DE
// consumers need the same Windows types in both x86 and x64 compilation.
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <game/CAnimManager.h>
#include <type_traits>
class CGame;
namespace mta::game_de
{
    class CAnimManagerDE : public CAnimManager
    {
    public:
        std::unique_ptr<CCustomAnimationCleanup>           BeginCustomAnimationCleanup(const SCustomAnimationCleanupRequest&) override;
        bool                                               HasPendingCustomAnimationCleanup() const noexcept override;
        std::unique_ptr<CAnimBlendSequence>                GetCustomAnimBlendSequence() override;
        std::unique_ptr<CAnimBlendSequence>                GetCustomAnimBlendSequence(CAnimBlendSequenceSAInterface*) override;
        void                                               DeleteCustomAnimSequenceInterface(CAnimBlendSequenceSAInterface*) override;
        std::unique_ptr<CCustomAnimationSequenceOperation> BeginCustomAnimationSequenceCleanup(const SCustomAnimationSequenceCleanupRequest&) override;
        std::unique_ptr<CCustomAnimationSequenceOperation> BeginCustomAnimationSequenceTransfer(const SCustomAnimationSequenceTransferRequest&) override;
        std::unique_ptr<CCustomAnimationSequenceOperation> BeginCustomAnimationFrameAttachment(const SCustomAnimationFrameAttachmentRequest&) override;
        StaticAssocIntface_type                            GetAnimStaticAssociation(eAnimGroup, eAnimID) const override;
        StaticAssocIntface_type                            GetAnimStaticAssociation(CAnimBlendStaticAssociationSAInterface*) override;
        using CAnimManager::GetAnimation;
        std::unique_ptr<CAnimBlendHierarchy> GetAnimation(int) override;
        std::unique_ptr<CAnimBlendHierarchy> GetAnimation(unsigned int, std::unique_ptr<CAnimBlock>&) override;
        std::unique_ptr<CAnimBlendHierarchy> GetAnimation(const char*, std::unique_ptr<CAnimBlock>&) override;
        std::unique_ptr<CAnimBlendHierarchy> GetAnimBlendHierarchy(CAnimBlendHierarchySAInterface*) override;
        std::unique_ptr<CAnimBlendHierarchy> GetCustomAnimBlendHierarchy() override;
        std::unique_ptr<CAnimBlendHierarchy> GetCustomAnimBlendHierarchy(CAnimBlendHierarchySAInterface*) override;
        void                                 DeleteCustomAnimHierarchyInterface(CAnimBlendHierarchySAInterface*) override;
        std::unique_ptr<CAnimBlendSequence>  GetCustomAnimBlendSequenceAt(CAnimBlendSequenceSAInterface*, DWORD) override;
        bool                                 isGateWayAnimationHierarchy(CAnimBlendHierarchySAInterface*) override;
        void                                 RemoveFromUncompressedCache(CAnimBlendHierarchy*) override;
        void                                 RemoveFromUncompressedCache(CAnimBlendHierarchySAInterface*) override;
        void                                 AddAnimBlockRef(int) override;
        int                                  GetNumRefsToAnimBlock(int) override;
        void                                 RemoveAnimBlockRefWithoutDelete(int) override;
        BYTE*                                AllocateKeyFramesMemory(std::uint32_t) override;
        void                                 FreeKeyFramesMemory(void*) override;
        CAnimBlendSequenceSAInterface*       AllocateCustomAnimSequences(std::size_t) override;
        void                                 FreeCustomAnimSequences(CAnimBlendSequenceSAInterface*) override;
        std::unique_ptr<CAnimBlock>          GetAnimationBlock(int) override;
        std::unique_ptr<CAnimBlock>          GetAnimationBlock(const char*) override;
        std::unique_ptr<CAnimBlock>          GetAnimBlock(CAnimBlockSAInterface*) override;
        int                                  GetNumAnimations() override;
        int                                  GetNumAnimBlocks() override;
        int                                  GetNumAnimAssocDefinitions() override;
        const char*                          GetAnimGroupName(AssocGroupId) override;
        const char*                          GetAnimBlockName(AssocGroupId) override;
        AssocGroupId                         GetFirstAssocGroup(const char*) override;
        bool                                 IsValidGroup(std::uint32_t) const override;
        bool                                 IsValidAnim(std::uint32_t, std::uint32_t) const override;
        int                                  GetAnimationBlockIndex(const char*) override;
        const char*                          GetGateWayBlockName() const override;
        const char*                          GetGateWayAnimationName() const override;

    protected:
        CAnimManagerDE(NativeAnimationManagerAccess& value, std::uint64_t generation) noexcept : native(value), ownerGeneration(generation) {}
        // Integer counters need only the same borrowed native owner, not a
        // game, a streaming provider or permission to publish block facades.
        CAnimManagerDE(NativeAnimationManagerAccess&, NativeAnimationBlockAccess&, std::uint64_t);
        // A hash lookup borrows an existing same-owner block and returns a
        // bank facade. It needs no CGame, streaming or block creation provider.
        CAnimManagerDE(NativeAnimationManagerAccess&, NativeAnimationBlockAccess&, NativeAnimationHierarchyAccess&, std::uint64_t);
        // Factory products borrow these exact providers. They must outlive
        // every returned block; no global game or second native owner is used.
        CAnimManagerDE(NativeAnimationManagerAccess&, NativeAnimationBlockAccess&, CGame&, std::uint64_t);
        // Sequence storage is an explicit optional owner service. Ordinary
        // observation-only facades never acquire a second lifetime manager.
        CAnimManagerDE(NativeAnimationManagerAccess&, NativeAnimationSequenceStorageAccess&, std::uint64_t);
        CAnimManagerDE(NativeAnimationManagerAccess&, NativeAnimationBlockAccess&, NativeAnimationSequenceStorageAccess&, CGame&, std::uint64_t);

        // Private custom products borrow the same actual native provider;
        // no complete game or unrelated global factory is fabricated.
        CAnimManagerDE(NativeAnimationManagerAccess&, NativeAnimationHierarchyAccess&, NativeAnimationSequenceAccess&, NativeAnimationSequenceStorageAccess&,
                       std::uint64_t);

    private:
        NativeAnimationSequenceReferenceToken SequenceOperationIdentity(CAnimBlendSequence*, const char*) const;
        NativeAnimationSequenceReferenceToken ResolveCustomSequenceInterface(std::uintptr_t, const char*) const;
        NativeAnimationBlockIdentity          ReadCounterIdentity(int, const char*) const;
        void                                  RequireFactory(const char*) const;
        std::unique_ptr<CAnimBlock>           MakeBlock(const NativeAnimationBlockIdentity&, const char*);
        NativeAnimationManagerCounts          Read(const char*) const;
        NativeAnimationManagerAccess&         native;
        const std::uint64_t                   ownerGeneration;
        // Resolve this optional service only from the actual complete owner,
        // shared by every constructor; no second manager or CGame is fabricated.
        NativeAnimationStaticAssociationAccess* staticAccess = dynamic_cast<NativeAnimationStaticAssociationAccess*>(&native);
        NativeAnimationBlockAccess*             blockAccess = nullptr;
        CGame*                                  gameProvider = nullptr;
        NativeAnimationSequenceStorageAccess*   storageAccess = nullptr;
        NativeAnimationHierarchyAccess*         hierarchyAccess = nullptr;
        NativeAnimationSequenceAccess*          sequenceAccess = nullptr;
        // Cleanup is optional only for partial observation receivers. Such a
        // receiver cannot certify teardown and reports pending conservatively.
        NativeCustomAnimationCleanupAccess*           cleanupAccess = nullptr;
        NativeCustomAnimationSequenceOperationAccess* sequenceOperationAccess = nullptr;
    };
    // These factories create actual borrowed CPU facades over issued owner
    // records. Standalone creation and typed owner operations require the
    // explicit same-provider service. Raw interface deletion is metadata-only;
    // unrelated manager operations retain their genuine pure contracts;
    // full parser exposure is not implied by these factory products.
    static_assert(std::is_abstract_v<CAnimManagerDE> && std::is_base_of_v<CAnimManager, CAnimManagerDE>);
}

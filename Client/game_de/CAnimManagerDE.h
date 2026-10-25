/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual animation-manager observations through one borrowed owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationManagerAccess.h"
#include "NativeAnimationSequenceStorageAccess.h"
#include "NativeAnimationHierarchyAccess.h"
#include "NativeAnimationSequenceAccess.h"
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
        std::unique_ptr<CAnimBlendHierarchy> GetCustomAnimBlendHierarchy() override;
        std::unique_ptr<CAnimBlendHierarchy> GetCustomAnimBlendHierarchy(CAnimBlendHierarchySAInterface*) override;
        void                                 DeleteCustomAnimHierarchyInterface(CAnimBlendHierarchySAInterface*) override;
        std::unique_ptr<CAnimBlendSequence>  GetCustomAnimBlendSequenceAt(CAnimBlendSequenceSAInterface*, DWORD) override;
        bool                                 isGateWayAnimationHierarchy(CAnimBlendHierarchySAInterface*) override;
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
        int                                  GetAnimationBlockIndex(const char*) override;
        const char*                          GetGateWayBlockName() const override;
        const char*                          GetGateWayAnimationName() const override;

    protected:
        CAnimManagerDE(NativeAnimationManagerAccess& value, std::uint64_t generation) noexcept : native(value), ownerGeneration(generation) {}
        // Integer counters need only the same borrowed native owner, not a
        // game, a streaming provider or permission to publish block facades.
        CAnimManagerDE(NativeAnimationManagerAccess&, NativeAnimationBlockAccess&, std::uint64_t);
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
        NativeAnimationBlockIdentity          ReadCounterIdentity(int, const char*) const;
        void                                  RequireFactory(const char*) const;
        std::unique_ptr<CAnimBlock>           MakeBlock(const NativeAnimationBlockIdentity&, const char*);
        NativeAnimationManagerCounts          Read(const char*) const;
        NativeAnimationManagerAccess&         native;
        const std::uint64_t                   ownerGeneration;
        NativeAnimationBlockAccess*           blockAccess = nullptr;
        CGame*                                gameProvider = nullptr;
        NativeAnimationSequenceStorageAccess* storageAccess = nullptr;
        NativeAnimationHierarchyAccess*       hierarchyAccess = nullptr;
        NativeAnimationSequenceAccess*        sequenceAccess = nullptr;
    };
    // These factories create actual borrowed CPU facades over issued owner
    // records. Standalone sequence creation/deletion and unrelated manager
    // operations retain their pure contracts. Complete parser use also needs
    // the matching durable cleanup ticket, never destructor-owned free.
    static_assert(std::is_abstract_v<CAnimManagerDE> && std::is_base_of_v<CAnimManager, CAnimManagerDE>);
}

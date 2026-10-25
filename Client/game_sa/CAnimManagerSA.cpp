/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Client/game_sa/CAnimManagerSA.cpp
 *  PURPOSE:     Animation manager
 *
 *  Multi Theft Auto is available from https://multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CAnimBlendAssocGroupSA.h"
#include "CAnimBlendAssociationSA.h"
#include "CAnimBlendHierarchySA.h"
#include "CAnimBlendSequenceSA.h"
#include "CAnimSequenceStorageSA.h"
#include "CAnimBlockSA.h"
#include "CAnimManagerSA.h"
#include <game/CAnimGatewayNames.h>
#include "CGameSA.h"
#include "CKeyGenSA.h"

extern CGameSA* pGame;

// This "gateway" animation will allow us to play custom animations by simply playing this animation
// and then in AddAnimation and AddAnimationAndSync hook, we can return our custom animation in the
// hook instead of run_wuzi. This will trick GTA SA into thinking that it is playing run_wuzi from
// ped block, but in reality, it's playing our custom animation, and Of course, we can return run_wuzi
// animation within the hook if we want to play it instead. Why run_wuzi? We can also use another animation,
// but I've tested with this one mostly, so let's stick to this.
static const char* const kGateWayAnimationName = MtaGame::CAnimGatewayNames::GetAnimationName();

namespace
{
    void AttachClassicFrames(CAnimBlendSequenceSAInterface* row, std::size_t count, bool root, bool compressed, void* frames)
    {
        // Invoke the actual classic implementation. Do not recreate its
        // original compression predicate or infer ownership from pointer reads.
        CAnimBlendSequenceSA(row).SetKeyFrames(count, root, compressed, frames);
    }
}
CAnimManagerSA::CAnimManagerSA()
    : customCleanupOwner(std::make_shared<MtaGame::CustomAnimationCleanupSA::Owner>(*this)),
      customSequenceOwner(std::make_shared<MtaGame::CustomAnimationSequenceSA::Owner>(*this, &AttachClassicFrames))
{
}

CAnimManagerSA::~CAnimManagerSA()
{
    // Core must check HasPending before destroying the Game/manager or
    // unloading game_sa. This is only a defensive post-lifetime fence.
    customCleanupOwner->liveOrigin = nullptr;
    customSequenceOwner->manager = nullptr;
}

void CAnimManagerSA::Initialize()
{
    DWORD dwFunc = FUNC_CAnimManager_Initialize;
    // clang-format off
    __asm
    {
        call    dwFunc
    }
    // clang-format on
}

void CAnimManagerSA::Shutdown()
{
    DWORD dwFunc = FUNC_CAnimManager_Shutdown;
    // clang-format off
    __asm
    {
        call    dwFunc
    }
    // clang-format on
}

int CAnimManagerSA::GetNumAnimations()
{
    return *(int*)(VAR_CAnimManager_NumAnimations);
}

int CAnimManagerSA::GetNumAnimBlocks()
{
    return *(int*)(VAR_CAnimManager_NumAnimBlocks);
}

int CAnimManagerSA::GetNumAnimAssocDefinitions()
{
    return *(int*)(VAR_CAnimManager_NumAnimAssocDefinitions);
}

std::unique_ptr<CAnimBlendHierarchy> CAnimManagerSA::GetAnimation(int ID)
{
    CAnimBlendHierarchySAInterface* pInterface = nullptr;
    DWORD                           dwFunc = FUNC_CAnimManager_GetAnimation_int;
    // clang-format off
    __asm
    {
        push    ID
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x4
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendHierarchySA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendHierarchy> CAnimManagerSA::GetAnimation(const char* szName, std::unique_ptr<CAnimBlock>& pBlock)
{
    CAnimBlendHierarchySAInterface* pInterface = nullptr;
    DWORD                           dwFunc = FUNC_CAnimManager_GetAnimation_str_block;
    CAnimBlockSAInterface*          pBlockInterface = pBlock->GetInterface();
    // clang-format off
    __asm
    {
        push    pBlockInterface
        push    szName
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x8
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendHierarchySA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendHierarchy> CAnimManagerSA::GetAnimation(unsigned int uiIndex, std::unique_ptr<CAnimBlock>& pBlock)
{
    CAnimBlendHierarchySAInterface* pInterface = nullptr;
    ;
    DWORD                  dwFunc = FUNC_CAnimManager_GetAnimation_int_block;
    CAnimBlockSAInterface* pBlockInterface = pBlock->GetInterface();
    // clang-format off
    __asm
    {
        push    pBlockInterface
        push    uiIndex
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x8
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendHierarchySA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlock> CAnimManagerSA::GetAnimationBlock(int ID)
{
    CAnimBlockSAInterface* pInterface = nullptr;
    DWORD                  dwFunc = FUNC_CAnimManager_GetAnimationBlock_int;
    // clang-format off
    __asm
    {
        push    ID
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x4
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlockSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlock> CAnimManagerSA::GetAnimationBlock(const char* szName)
{
    CAnimBlockSAInterface* pInterface = nullptr;
    DWORD                  dwFunc = FUNC_CAnimManager_GetAnimationBlock_str;
    // clang-format off
    __asm
    {
        push    szName
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x4
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlockSA>(pInterface);
    }
    return nullptr;
}

int CAnimManagerSA::GetAnimationBlockIndex(const char* szName)
{
    int   iReturn;
    DWORD dwFunc = FUNC_CAnimManager_GetAnimationBlockIndex;
    // clang-format off
    __asm
    {
        push    szName
        call    dwFunc
        mov     iReturn, eax
        add     esp, 0x4
    }
    // clang-format on
    return iReturn;
}

int CAnimManagerSA::RegisterAnimBlock(const char* szName)
{
    int   iReturn;
    DWORD dwFunc = FUNC_CAnimManager_RegisterAnimBlock;
    // clang-format off
    __asm
    {
        push    szName
        call    dwFunc
        mov     iReturn, eax
        add     esp, 0x4
    }
    // clang-format on
    return iReturn;
}

std::unique_ptr<CAnimBlendAssocGroup> CAnimManagerSA::GetAnimBlendAssoc(AssocGroupId groupID) const
{
    CAnimBlendAssocGroupSAInterface* pInterface = nullptr;
    DWORD                            dwFunc = FUNC_CAnimManager_GetAnimBlendAssoc;
    // clang-format off
    __asm
    {
        push    groupID
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x4
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssocGroupSA>(pInterface);
    }
    return nullptr;
}

AssocGroupId CAnimManagerSA::GetFirstAssocGroup(const char* szName)
{
    AssocGroupId groupReturn;
    DWORD        dwFunc = FUNC_CAnimManager_GetFirstAssocGroup;
    // clang-format off
    __asm
    {
        push    szName
        call    dwFunc
        mov     groupReturn, eax
        add     esp, 0x4
    }
    // clang-format on
    return groupReturn;
}

const char* CAnimManagerSA::GetAnimGroupName(AssocGroupId groupID)
{
    const char* szReturn;
    DWORD       dwFunc = FUNC_CAnimManager_GetAnimGroupName;
    // clang-format off
    __asm
    {
        push    groupID
        call    dwFunc
        mov     szReturn, eax
        add     esp, 0x4
    }
    // clang-format on
    return szReturn;
}

const char* CAnimManagerSA::GetAnimBlockName(AssocGroupId groupID)
{
    const char* szReturn;
    DWORD       dwFunc = FUNC_CAnimManager_GetAnimBlockName;
    // clang-format off
    __asm
    {
        push    groupID
        call    dwFunc
        mov     szReturn, eax
        add     esp, 0x4
    }
    // clang-format on
    return szReturn;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::CreateAnimAssociation(AssocGroupId animGroup, AnimationId animID)
{
    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_CAnimManager_CreateAnimAssociation;
    // clang-format off
    __asm
    {
        push    animID
        push    animGroup
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x8
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

CAnimManagerSA::StaticAssocIntface_type CAnimManagerSA::GetAnimStaticAssociation(eAnimGroup animGroup, eAnimID animID) const
{
    // We check the validity of the group, avoid crashes due to an invalid group
    if (!IsValidGroup(static_cast<std::uint32_t>(animGroup)))
        return nullptr;

    CAnimBlendStaticAssociationSAInterface* pInterface = nullptr;
    DWORD                                   dwFunc = FUNC_CAnimManager_GetAnimAssociation;
    // clang-format off
    __asm
    {
        push    animID
        push    animGroup
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x8
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendStaticAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::GetAnimAssociation(AssocGroupId animGroup, const char* szAnimName)
{
    // We check the validity of the group, avoid crashes due to an invalid group
    if (!IsValidGroup(animGroup))
        return nullptr;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_CAnimManager_GetAnimAssociation_str;
    // clang-format off
    __asm
    {
        push    szAnimName
        push    animGroup
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x8
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::AddAnimation(RpClump* pClump, AssocGroupId animGroup, AnimationId animID)
{
    if (!pClump)
        return NULL;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_CAnimManager_AddAnimation;
    // clang-format off
    __asm
    {
        push    animID
        push    animGroup
        push    pClump
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0xC
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::AddAnimation(RpClump* pClump, CAnimBlendHierarchy* pHierarchy, int ID)
{
    if (!pClump)
        return NULL;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_CAnimManager_AddAnimation_hier;
    CAnimBlendHierarchySAInterface*   pHierarchyInterface = pHierarchy->GetInterface();
    // clang-format off
    __asm
    {
        push    ID
        push    pHierarchyInterface
        push    pClump
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0xC
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::AddAnimationAndSync(RpClump* pClump, CAnimBlendAssociation* pAssociation, AssocGroupId animGroup,
                                                                           AnimationId animID)
{
    if (!pClump)
        return nullptr;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_CAnimManager_AddAnimationAndSync;
    CAnimBlendAssociationSAInterface* pAssociationInterface = pAssociation->GetInterface();
    // clang-format off
    __asm
    {
        push    animID
        push    animGroup
        push    pAssociationInterface
        push    pClump
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x10
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::BlendAnimation(RpClump* pClump, AssocGroupId animGroup, AnimationId animID, float fBlendDelta)
{
    if (!pClump || !IsValidAnim(animGroup, animID))
        return nullptr;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_CAnimManager_BlendAnimation;
    // clang-format off
    __asm
    {
        push    fBlendDelta
        push    animID
        push    animGroup
        push    pClump
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x10
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::BlendAnimation(RpClump* pClump, CAnimBlendHierarchy* pHierarchy, int ID, float fBlendDelta)
{
    if (!pClump)
        return NULL;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_CAnimManager_BlendAnimation_hier;
    CAnimBlendHierarchySAInterface*   pHierarchyInterface = pHierarchy->GetInterface();
    // clang-format off
    __asm
    {
        push    fBlendDelta
        push    ID
        push    pHierarchyInterface
        push    pClump
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x10
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

void CAnimManagerSA::AddAnimBlockRef(int ID)
{
    DWORD dwFunc = FUNC_CAnimManager_AddAnimBlockRef;
    // clang-format off
    __asm
    {
        push    ID
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
}

void CAnimManagerSA::RemoveAnimBlockRef(int ID)
{
    DWORD dwFunc = FUNC_CAnimManager_RemoveAnimBlockRef;
    // clang-format off
    __asm
    {
        push    ID
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
}

void CAnimManagerSA::RemoveAnimBlockRefWithoutDelete(int ID)
{
    DWORD dwFunc = FUNC_CAnimManager_RemoveAnimBlockRefWithoutDelete;
    // clang-format off
    __asm
    {
        push    ID
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
}

int CAnimManagerSA::GetNumRefsToAnimBlock(int ID)
{
    int   iReturn;
    DWORD dwFunc = FUNC_CAnimManager_GetNumRefsToAnimBlock;
    // clang-format off
    __asm
    {
        push    ID
        call    dwFunc
        mov     iReturn, eax
        add     esp, 0x4
    }
    // clang-format on
    return iReturn;
}

void CAnimManagerSA::RemoveAnimBlock(int ID)
{
    DWORD dwFunc = FUNC_CAnimManager_RemoveAnimBlock;
    // clang-format off
    __asm
    {
        push    ID
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
}

AnimAssocDefinition* CAnimManagerSA::AddAnimAssocDefinition(const char* szBlockName, const char* szAnimName, AssocGroupId animGroup, AnimationId animID,
                                                            AnimDescriptor* pDescriptor)
{
    if (!IsValidAnim(animGroup, animID))
        return nullptr;

    AnimAssocDefinition* pReturn{};
    DWORD                dwFunc = FUNC_CAnimManager_AddAnimAssocDefinition;
    // clang-format off
    __asm
    {
        push    pDescriptor
        push    animID
        push    animGroup
        push    szAnimName
        push    szBlockName
        call    dwFunc
        mov     pReturn, eax
        add     esp, 0x14
    }
    // clang-format on
    return pReturn;
}

void CAnimManagerSA::ReadAnimAssociationDefinitions()
{
    DWORD dwFunc = FUNC_CAnimManager_ReadAnimAssociationDefinitions;
    // clang-format off
    __asm
    {
        call    dwFunc
    }
    // clang-format on
}

void CAnimManagerSA::CreateAnimAssocGroups()
{
    DWORD dwFunc = FUNC_CAnimManager_CreateAnimAssocGroups;
    // clang-format off
    __asm
    {
        call    dwFunc
    }
    // clang-format on
}

void CAnimManagerSA::UncompressAnimation(CAnimBlendHierarchy* pHierarchy)
{
    DWORD                           dwFunc = FUNC_CAnimManager_UncompressAnimation;
    CAnimBlendHierarchySAInterface* pHierarchyInterface = pHierarchy->GetInterface();
    // clang-format off
    __asm
    {
        push    pHierarchyInterface
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
}

void CAnimManagerSA::RemoveFromUncompressedCache(CAnimBlendHierarchy* pHierarchy)
{
    DWORD                           dwFunc = FUNC_CAnimManager_RemoveFromUncompressedCache;
    CAnimBlendHierarchySAInterface* pHierarchyInterface = pHierarchy->GetInterface();
    // clang-format off
    __asm
    {
        push    pHierarchyInterface
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
}

void CAnimManagerSA::RemoveFromUncompressedCache(CAnimBlendHierarchySAInterface* pHierarchyInterface)
{
    DWORD dwFunc = FUNC_CAnimManager_RemoveFromUncompressedCache;
    // clang-format off
    __asm
    {
        push    pHierarchyInterface
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
}

void CAnimManagerSA::LoadAnimFile(const char* szFile)
{
    DWORD dwFunc = FUNC_CAnimManager_LoadAnimFile;
    // clang-format off
    __asm
    {
        push    szFile
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
}

void CAnimManagerSA::LoadAnimFile(RwStream* pStream, bool b1, const char* sz1)
{
    DWORD dwFunc = FUNC_CAnimManager_LoadAnimFile_stream;
    // clang-format off
    __asm
    {
        push    sz1
        push    b1
        push    pStream
        call    dwFunc
        add     esp, 0xC
    }
    // clang-format on
}

void CAnimManagerSA::LoadAnimFiles()
{
    DWORD dwFunc = FUNC_CAnimManager_LoadAnimFiles;
    // clang-format off
    __asm
    {
        call    dwFunc
    }
    // clang-format on
}

void CAnimManagerSA::RemoveLastAnimFile()
{
    DWORD dwFunc = FUNC_CAnimManager_RemoveLastAnimFile;
    // clang-format off
    __asm
    {
        call    dwFunc
    }
    // clang-format on
}

BYTE* CAnimManagerSA::AllocateKeyFramesMemory(uint32_t u32BytesToAllocate)
{
    MtaGame::CustomAnimationSequenceSA::CallGuard call(*customSequenceOwner);
    const auto                                    issued = MtaGame::CustomAnimationSequenceSA::ReserveFrame(*customSequenceOwner);
    issued->entered = true;
    BYTE* pKeyFrames = nullptr;
    DWORD dwFunc = FUNC_CAnimManager_AllocateKeyFramesMemory;
    // clang-format off
    __asm
    {
        push    u32BytesToAllocate
        call    dwFunc
        add     esp, 0x4
        mov     pKeyFrames, eax
    }
    // clang-format on
    issued->address = pKeyFrames;
    issued->returned = true;
    return pKeyFrames;
}

void CAnimManagerSA::FreeKeyFramesMemory(void* pKeyFrames)
{
    MtaGame::CustomAnimationSequenceSA::CallGuard              call(*customSequenceOwner, true);
    std::shared_ptr<MtaGame::CustomAnimationSequenceSA::Frame> issued;
    for (auto it = customSequenceOwner->frames.rbegin(); it != customSequenceOwner->frames.rend(); ++it)
        if ((*it)->address == pKeyFrames && (*it)->returned)
        {
            issued = *it;
            break;
        }
    if (issued)
    {
        if (issued->release == MtaGame::CustomAnimationSequenceSA::Progress::Returned)
            return;  // Saved per-original ACK, not absence or address guessing.
        if (issued->fault || !issued->participants.empty() || issued->release == MtaGame::CustomAnimationSequenceSA::Progress::Entered)
            throw std::runtime_error("Classic original frame has a live claim or unknown release");
        issued->release = MtaGame::CustomAnimationSequenceSA::Progress::Entered;
    }
    DWORD dwFunc = FUNC_CAnimManager_FreeKeyFramesMemory;
    // clang-format off
    __asm
    {
        push    pKeyFrames
        call    dwFunc
        add     esp, 0x4
    }
    // clang-format on
    if (issued) issued->release = MtaGame::CustomAnimationSequenceSA::Progress::Returned;
    MtaGame::CustomAnimationSequenceSA::Pump(*customSequenceOwner);
}

bool CAnimManagerSA::HasAnimGroupLoaded(AssocGroupId groupID)
{
    bool  bReturn;
    DWORD dwFunc = FUNC_HasAnimGroupLoaded;
    // clang-format off
    __asm
    {
        push    groupID
        call    dwFunc
        mov     bReturn, al
        add     esp, 0x4
    }
    // clang-format on
    return bReturn;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::RpAnimBlendClumpGetFirstAssociation(RpClump* pClump)
{
    if (!pClump)
        return NULL;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_RpAnimBlendClumpGetFirstAssociation;
    // clang-format off
    __asm
    {
        push    pClump
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x4
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::RpAnimBlendClumpGetAssociation(RpClump* pClump, const char* szAnimName)
{
    if (!pClump)
        return NULL;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_RpAnimBlendClumpGetAssociation_str;
    // clang-format off
    __asm
    {
        push    szAnimName
        push    pClump
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x8
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::RpAnimBlendClumpGetAssociation(RpClump* pClump, AnimationId animID)
{
    if (!pClump)
        return NULL;

    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_RpAnimBlendClumpGetAssociation_int;
    // clang-format off
    __asm
    {
        push    animID
        push    pClump
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x8
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::RpAnimBlendGetNextAssociation(std::unique_ptr<CAnimBlendAssociation>& pAssociation)
{
    CAnimBlendAssociationSAInterface* pInterface = nullptr;
    DWORD                             dwFunc = FUNC_RpAnimBlendGetNextAssociation;
    CAnimBlendAssociationSAInterface* pAssociationInterface = pAssociation->GetInterface();
    // clang-format off
    __asm
    {
        push    pAssociationInterface
        call    dwFunc
        mov     pInterface, eax
        add     esp, 0x4
    }
    // clang-format on
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

int CAnimManagerSA::RpAnimBlendClumpGetNumAssociations(RpClump* pClump)
{
    if (!pClump)
        return 0;

    int   iReturn;
    DWORD dwFunc = FUNC_RpAnimBlendClumpGetNumAssociations;
    // clang-format off
    __asm
    {
        push    pClump
        call    dwFunc
        mov     iReturn, eax
        add     esp, 0x4
    }
    // clang-format on
    return iReturn;
}

void CAnimManagerSA::RpAnimBlendClumpUpdateAnimations(RpClump* pClump, float f1, bool b1)
{
    if (!pClump)
        return;

    DWORD dwFunc = FUNC_RpAnimBlendClumpUpdateAnimations;
    // clang-format off
    __asm
    {
        push    b1
        push    f1
        push    pClump
        call    dwFunc
        add     esp, 0xC
    }
    // clang-format on
}

std::unique_ptr<CAnimBlendAssociation> CAnimManagerSA::GetAnimBlendAssociation(CAnimBlendAssociationSAInterface* pInterface)
{
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendAssocGroup> CAnimManagerSA::GetAnimBlendAssocGroup(CAnimBlendAssocGroupSAInterface* pInterface)
{
    if (pInterface)
    {
        return std::make_unique<CAnimBlendAssocGroupSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlock> CAnimManagerSA::GetAnimBlock(CAnimBlockSAInterface* pInterface)
{
    if (pInterface)
    {
        return std::make_unique<CAnimBlockSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendHierarchy> CAnimManagerSA::GetAnimBlendHierarchy(CAnimBlendHierarchySAInterface* pInterface)
{
    if (pInterface)
    {
        return std::make_unique<CAnimBlendHierarchySA>(pInterface);
    }
    return nullptr;
}

CAnimManagerSA::StaticAssocIntface_type CAnimManagerSA::GetAnimStaticAssociation(CAnimBlendStaticAssociationSAInterface* pInterface)
{
    if (pInterface)
    {
        return std::make_unique<CAnimBlendStaticAssociationSA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendHierarchy> CAnimManagerSA::GetCustomAnimBlendHierarchy(CAnimBlendHierarchySAInterface* pInterface)
{
    if (pInterface)
    {
        return std::make_unique<CAnimBlendHierarchySA>(pInterface);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendSequence> CAnimManagerSA::GetCustomAnimBlendSequence(CAnimBlendSequenceSAInterface* pInterface)
{
    if (pInterface)
    {
        auto row = MtaGame::CustomAnimationSequenceSA::FindRow(*customSequenceOwner, pInterface);
        return std::make_unique<CAnimBlendSequenceSA>(pInterface, row ? customSequenceOwner.get() : nullptr);
    }
    return nullptr;
}

std::unique_ptr<CAnimBlendHierarchy> CAnimManagerSA::GetCustomAnimBlendHierarchy()
{
    std::unique_ptr<CAnimBlendHierarchySAInterface> pInterface(new CAnimBlendHierarchySAInterface);
    auto                                            reservation = MtaGame::CustomAnimationCleanupSA::Reserve(*customCleanupOwner, pInterface.get());
    try
    {
        auto facade = std::make_unique<CAnimBlendHierarchySA>(pInterface.get());
        pInterface.release();
        return facade;
    }
    catch (...)
    {
        MtaGame::CustomAnimationCleanupSA::CancelUnexposed(*customCleanupOwner, reservation);
        throw;
    }
}

std::unique_ptr<CAnimBlendSequence> CAnimManagerSA::GetCustomAnimBlendSequence()
{
    MtaGame::CustomAnimationSequenceSA::CallGuard  call(*customSequenceOwner);
    std::unique_ptr<CAnimBlendSequenceSAInterface> pInterface(new CAnimBlendSequenceSAInterface);
    const auto                                     row = MtaGame::CustomAnimationSequenceSA::RegisterStandalone(*customSequenceOwner, pInterface.get());
    try
    {
        auto facade = std::make_unique<CAnimBlendSequenceSA>(pInterface.get(), customSequenceOwner.get());
        pInterface.release();
        return facade;
    }
    catch (...)
    {
        row->release = MtaGame::CustomAnimationSequenceSA::Progress::Returned;
        throw;
    }
}

void CAnimManagerSA::DeleteCustomAnimHierarchyInterface(CAnimBlendHierarchySAInterface* pInterface)
{
    std::shared_ptr<MtaGame::CustomAnimationCleanupSA::Record> reserved;
    for (const auto& record : customCleanupOwner->records)
        if (record->hierarchy == pInterface && !record->complete)
        {
            reserved = record;
            break;
        }
    if (reserved)
    {
        if (reserved->begun && (!reserved->active || reserved->outer.progress != MtaGame::CustomAnimationCleanupSA::Progress::Entered))
            throw std::runtime_error("Classic hierarchy release belongs to its cleanup ticket");
        if (!reserved->begun && reserved->outer.progress != MtaGame::CustomAnimationCleanupSA::Progress::Waiting)
            throw std::runtime_error("Classic hierarchy release has no returned acknowledgement");
        reserved->outer.progress = MtaGame::CustomAnimationCleanupSA::Progress::Entered;
    }
    // Preserve the actual source operation: plain outer-interface deletion,
    // without adopting children or invoking an engine hierarchy destructor.
    delete pInterface;
    if (reserved)
    {
        reserved->outer.progress = MtaGame::CustomAnimationCleanupSA::Progress::Returned;
        if (!reserved->begun)
            reserved->complete = true;
    }
}

void CAnimManagerSA::DeleteCustomAnimSequenceInterface(CAnimBlendSequenceSAInterface* pInterface)
{
    MtaGame::CustomAnimationSequenceSA::CallGuard call(*customSequenceOwner, true);
    auto                                          row = MtaGame::CustomAnimationSequenceSA::FindRow(*customSequenceOwner, pInterface);
    if (row)
    {
        if (!row->standalone || row->fault || row->release == MtaGame::CustomAnimationSequenceSA::Progress::Entered)
            throw std::runtime_error("Classic interface release requires its exact standalone issuance");
        if (const auto pending = row->pending.lock(); pending && !pending->terminal && !pending->active)
            throw std::runtime_error("Classic interface has an accepted owner command");
        row->release = MtaGame::CustomAnimationSequenceSA::Progress::Entered;
    }
    // Preserve actual SA raw-delete semantics: frame storage is independent.
    delete pInterface;
    if (row)
    {
        row->release = MtaGame::CustomAnimationSequenceSA::Progress::Returned;
        for (const auto& frame : row->historical)
            frame->participants.erase(row.get());
    }
}

bool CAnimManagerSA::isGateWayAnimationHierarchy(CAnimBlendHierarchySAInterface* pInterface)
{
    return pGame->GetKeyGen()->GetUppercaseKey(kGateWayAnimationName) == pInterface->uiHashKey;
}

const char* CAnimManagerSA::GetGateWayBlockName() const
{
    return MtaGame::CAnimGatewayNames::GetBlockName();
}

const char* CAnimManagerSA::GetGateWayAnimationName() const
{
    return MtaGame::CAnimGatewayNames::GetAnimationName();
}

bool CAnimManagerSA::IsValidGroup(std::uint32_t uiAnimGroup) const
{
    if ((eAnimGroup)uiAnimGroup <= eAnimGroup::ANIM_GROUP_NONE || (eAnimGroup)uiAnimGroup >= eAnimGroup::ANIM_TOTAL_GROUPS)
        return false;

    const auto pGroup = GetAnimBlendAssoc(uiAnimGroup);
    return pGroup && pGroup->IsCreated();
}

bool CAnimManagerSA::IsValidAnim(std::uint32_t uiAnimGroup, std::uint32_t uiAnimID) const
{
    if ((eAnimID)uiAnimID <= eAnimID::ANIM_ID_UNDEFINED || (eAnimID)uiAnimID >= eAnimID::ANIM_ID_MAX)
        return false;

    // We get an animation for the checks
    const auto pAnim = GetAnimStaticAssociation((eAnimGroup)uiAnimGroup, (eAnimID)uiAnimID);
    if (!pAnim)
        return false;

    // We check the interface and sAnimID, if AnimID is not in GTA:SA, it will differ from our indicators in sAnimID
    const auto pInterface = pAnim->GetInterface();
    return pInterface && pInterface->sAnimID == uiAnimID;
}

CAnimBlendSequenceSAInterface* CAnimManagerSA::AllocateCustomAnimSequences(size_t capacity)
{
    MtaGame::CustomAnimationSequenceSA::CallGuard call(*customSequenceOwner);
    auto*                                         sequences = MtaGame::CAnimSequenceStorageSA::Allocate(capacity);
    try
    {
        MtaGame::CustomAnimationSequenceSA::RegisterArray(*customSequenceOwner, sequences, capacity);
    }
    catch (...)
    {
        MtaGame::CAnimSequenceStorageSA::Free(sequences);
        throw;
    }
    return sequences;
}

void CAnimManagerSA::FreeCustomAnimSequences(CAnimBlendSequenceSAInterface* sequences)
{
    MtaGame::CustomAnimationSequenceSA::CallGuard call(*customSequenceOwner, true);
    const auto                                    array = MtaGame::CustomAnimationSequenceSA::FindArray(*customSequenceOwner, sequences);
    if (array && !array->closing)
    {
        for (const auto& row : customSequenceOwner->rows)
            if (row->array == array)
                if (const auto pending = row->pending.lock(); pending && !pending->terminal)
                    throw std::runtime_error("Classic array has an accepted construction command");
        // Storage-only release drops only the rows' own claims. Original
        // frames remain independent live allocations until explicit release.
        MtaGame::CustomAnimationSequenceSA::CloseArrayClaims(*customSequenceOwner, sequences);
    }
    MtaGame::CAnimSequenceStorageSA::Free(sequences);
    if (array)
        array->released = true;
    MtaGame::CustomAnimationSequenceSA::Pump(*customSequenceOwner);
}

std::unique_ptr<CAnimBlendSequence> CAnimManagerSA::GetCustomAnimBlendSequenceAt(CAnimBlendSequenceSAInterface* originalArray, DWORD index)
{
    static_assert(sizeof(index) == sizeof(std::uint32_t));
    auto* row = MtaGame::CAnimSequenceStorageSA::At(originalArray, static_cast<std::uint32_t>(index));
    if (row)
        if (const auto array = MtaGame::CustomAnimationSequenceSA::FindArray(*customSequenceOwner, originalArray))
            MtaGame::CustomAnimationSequenceSA::RegisterBorrowedRow(*customSequenceOwner, array, row);
    return GetCustomAnimBlendSequence(row);
}

std::unique_ptr<CCustomAnimationCleanup> CAnimManagerSA::BeginCustomAnimationCleanup(const SCustomAnimationCleanupRequest& request)
{
    if (request.origin != this)
        throw std::invalid_argument("Classic cleanup requires the creating manager");
    CAnimBlendHierarchySAInterface* hierarchy = nullptr;
    if (request.hierarchy)
    {
        auto* classic = dynamic_cast<CAnimBlendHierarchySA*>(request.hierarchy);
        if (!classic)
            throw std::invalid_argument("Classic cleanup requires its actual backend hierarchy facade");
        // The source custom-storage contract already owns this plain outer
        // interface. Copy it during Begin; never retain the CPU facade.
        hierarchy = classic->GetInterface();
    }
    auto ticket = MtaGame::CustomAnimationCleanupSA::Begin(customCleanupOwner, request, hierarchy);
    for (const auto& record : customCleanupOwner->records)
        if (record->hierarchy == hierarchy && record->sequences == request.originalArray && !record->complete)
        {
            if (!record->gathered)
            {
                record->frameContext = customSequenceOwner.get();
                record->identifyFrame = &MtaGame::CustomAnimationSequenceSA::IdentifyFrame;
                record->releaseFrame = &MtaGame::CustomAnimationSequenceSA::ReleaseFrame;
                record->extraFrames = MtaGame::CustomAnimationSequenceSA::SnapshotArrayFrames(*customSequenceOwner, request.originalArray);
                MtaGame::CustomAnimationCleanupSA::Gather(*record);
                // The exact original identities are now immutable in the H
                // ticket, before another related owner can release/reuse one.
                MtaGame::CustomAnimationSequenceSA::CloseArrayClaims(*customSequenceOwner, request.originalArray);
            }
            break;
        }
    return ticket;
}

bool CAnimManagerSA::HasPendingCustomAnimationCleanup() const noexcept
{
    return !customCleanupOwner || !customSequenceOwner || customCleanupOwner->HasPending() || customSequenceOwner->HasPending();
}

std::unique_ptr<CCustomAnimationSequenceOperation> CAnimManagerSA::BeginCustomAnimationSequenceCleanup(const SCustomAnimationSequenceCleanupRequest& request)
{
    const auto copied = request;
    if (copied.origin != this)
        throw std::invalid_argument("Classic sequence cleanup requires the creating manager");
    auto* source = dynamic_cast<CAnimBlendSequenceSA*>(copied.source);
    if (!source)
        throw std::invalid_argument("Classic cleanup requires its actual sequence facade");
    return MtaGame::CustomAnimationSequenceSA::Begin(customSequenceOwner, MtaGame::CustomAnimationSequenceSA::Kind::Cleanup, source->GetInterface());
}
std::unique_ptr<CCustomAnimationSequenceOperation> CAnimManagerSA::BeginCustomAnimationSequenceTransfer(const SCustomAnimationSequenceTransferRequest& request)
{
    const auto copied = request;
    if (copied.origin != this)
        throw std::invalid_argument("Classic sequence transfer requires the creating manager");
    auto* source = dynamic_cast<CAnimBlendSequenceSA*>(copied.source);
    auto* target = dynamic_cast<CAnimBlendSequenceSA*>(copied.destination);
    if (!source || !target)
        throw std::invalid_argument("Classic transfer requires actual source and target facades");
    return MtaGame::CustomAnimationSequenceSA::Begin(customSequenceOwner, MtaGame::CustomAnimationSequenceSA::Kind::Transfer, source->GetInterface(),
                                                     target->GetInterface());
}
std::unique_ptr<CCustomAnimationSequenceOperation> CAnimManagerSA::BeginCustomAnimationFrameAttachment(const SCustomAnimationFrameAttachmentRequest& request)
{
    const auto copied = request;
    if (copied.origin != this)
        throw std::invalid_argument("Classic frame assignment requires the creating manager");
    auto* target = dynamic_cast<CAnimBlendSequenceSA*>(copied.target);
    if (!target)
        throw std::invalid_argument("Classic assignment requires its actual target facade");
    return MtaGame::CustomAnimationSequenceSA::Begin(customSequenceOwner, MtaGame::CustomAnimationSequenceSA::Kind::Attachment, target->GetInterface(), nullptr,
                                                     copied.count, copied.root, copied.compressed, copied.originalFrames);
}

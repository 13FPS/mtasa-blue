/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CAnimBlockSA.cpp
 *  PURPOSE:     Animation block
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CAnimBlendHierarchySA.h"
#include "CAnimBlockSA.h"
#include "CAnimManagerSA.h"
#include "CGameSA.h"
#include <game/CAnimBlockRequestPolicy.h>

extern CGameSA* pGame;

int CAnimBlockSAInterface::GetIndex()
{
    return (((DWORD)this - ARRAY_CAnimManager_AnimBlocks) / 32);
}

void CAnimBlockSA::Request(EModelRequestType requestType, bool bAllowBlockingFail)
{
    MtaGame::CAnimBlockRequestPolicy::Request(
        static_cast<std::uint32_t>(requestType), bAllowBlockingFail, [this] { return IsLoaded(); }, [this] { return GetIndex(); },
        [] { return pGame->GetBaseIDforIFP(); }, [](std::uint32_t model) { return pGame->GetModelInfo(model); },
        [](std::uint32_t model, std::uint32_t flags) { pGame->GetStreaming()->RequestModel(model, flags); }, [this] { AddRef(); }, [](bool onlyPriority)
        { pGame->GetStreaming()->LoadAllRequestedModels(onlyPriority); }, [](std::uint32_t model) { return pGame->GetStreaming()->HasModelLoaded(model); });
}

CAnimBlendHierarchySAInterface* CAnimBlockSA::GetAnimationHierarchyInterface(size_t iAnimation)
{
    if (!IsLoaded())
        return nullptr;

    iAnimation += m_pInterface->idOffset;
    BYTE* arrAnimations = reinterpret_cast<BYTE*>(ARRAY_CAnimManager_Animations);
    return reinterpret_cast<CAnimBlendHierarchySAInterface*>(arrAnimations + sizeof(CAnimBlendHierarchySAInterface) * iAnimation);
}

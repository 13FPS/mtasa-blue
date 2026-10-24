/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Client/mods/deathmatch/logic/CIFPAnimations.cpp
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include <StdInc.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendSequence.h>
#include <game/CAnimManager.h>

#include "CIFPAnimationStorage.h"

CIFPAnimations::~CIFPAnimations()
{
    DeleteAnimations();
}

void CIFPAnimations::DeleteAnimations()
{
    // Storage capacity is valid even when parsing stopped before publishing
    // the final logical count. Unvisited vector entries and null rows are safe.
    for (auto& animation : vecAnimations)
        MtaGame::IfpStorage::DestroyAnimation(manager, animation);
    vecAnimations.clear();
}

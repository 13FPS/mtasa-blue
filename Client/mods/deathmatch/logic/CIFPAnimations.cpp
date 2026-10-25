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
    // A pending accepted ticket leaves the copied request/resources with the
    // original backend owner. Destruction drops only CPU facades, not debt.
    DeleteAnimations();
}

bool CIFPAnimations::DeleteAnimations()
{
    // Completed entries stay empty and stable while another entry waits for its
    // actual backend. The durable ticket, never vector/map absence, owns debt.
    bool complete = true;
    for (auto& animation : vecAnimations)
        if (!MtaGame::IfpStorage::DestroyAnimation(manager, animation))
            complete = false;
    if (complete)
        vecAnimations.clear();
    return complete;
}

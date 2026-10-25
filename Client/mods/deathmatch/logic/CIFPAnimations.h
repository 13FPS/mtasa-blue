/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Client/mods/deathmatch/logic/CIFPAnimations.h
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <bitset>
#include <cstddef>
#include <memory>
#include <vector>
#include <windows.h>
#include <SString.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CCustomAnimationCleanup.h>
class CAnimManager;

class CIFPAnimations
{
public:
    struct SAnimation
    {
        SString                              Name;
        unsigned int                         uiNameHash = 0;
        std::unique_ptr<CAnimBlendHierarchy> pHierarchy;
        CAnimBlendSequenceSAInterface*       pSequences = nullptr;
        std::size_t                          sequenceCapacity = 0;
        bool                                 sequencesAttached = false;
        // The facade is CPU-only; its same backend keeps incomplete native
        // ownership and acknowledgements after the final shared owner drops.
        std::unique_ptr<CCustomAnimationCleanup> cleanupTicket;

        // Bit i is set when bone i (see CClientIFP::m_karruBoneIds) is actually driven by this
        // animation's own IFP data, as opposed to being padded out with a fixed pose because the
        // file doesn't define that bone.
        std::bitset<64> AnimatedBonesMask;
    };

    std::vector<SAnimation> vecAnimations;

public:
    explicit CIFPAnimations(CAnimManager& manager) noexcept : manager(manager) {}
    ~CIFPAnimations();
    bool DeleteAnimations();

private:
    // Cleanup uses the creating backend; its pending-record/ticket gate keeps
    // that manager and module alive rather than reselecting a global provider.
    CAnimManager& manager;
};

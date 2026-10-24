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

        // Bit i is set when bone i (see CClientIFP::m_karruBoneIds) is actually driven by this
        // animation's own IFP data, as opposed to being padded out with a fixed pose because the
        // file doesn't define that bone.
        std::bitset<64> AnimatedBonesMask;
    };

    std::vector<SAnimation> vecAnimations;

public:
    explicit CIFPAnimations(CAnimManager& manager) noexcept : manager(manager) {}
    ~CIFPAnimations();
    void DeleteAnimations();

private:
    // The existing backend outlives its IFP owners; never reselect a different
    // global manager while destroying transferred native allocations.
    CAnimManager& manager;
};

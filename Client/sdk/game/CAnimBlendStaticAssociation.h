/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/CAnimBlendStaticAssocation.h
 *  PURPOSE:     Animation blend static association interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

class CAnimBlendHierarchySAInterface;
class CAnimBlendStaticAssociationSAInterface;
struct RpClump;

class CAnimBlendStaticAssociation
{
public:
    virtual void Initialize(RpClump* pClump, CAnimBlendHierarchySAInterface* pAnimBlendHierarchyInterface) = 0;
    virtual void SetNumBlendNodes(unsigned short nNumBlendNodes) = 0;
    virtual void SetAnimID(short sAnimID) = 0;
    virtual void SetAnimGroup(short sAnimGroup) = 0;
    virtual void SetFlags(short sFlags) = 0;

    virtual unsigned short                          GetNumBlendNodes() = 0;
    virtual short                                   GetAnimID() = 0;
    virtual short                                   GetAnimGroup() = 0;
    virtual short                                   GetFlags() = 0;
    virtual CAnimBlendHierarchySAInterface*         GetAnimHierarchyInterface() = 0;
    virtual CAnimBlendStaticAssociationSAInterface* GetInterface() = 0;
    // Both manager factories return unique_ptr<Base> CPU facades. Destruction
    // owns only that wrapper; the engine group's row/node/bank lifetime stays
    // with its genuine native owner. Rebuild classic/DE providers and consumers
    // together for this deliberate shared C++ ABI change.
    virtual ~CAnimBlendStaticAssociation() = default;
};

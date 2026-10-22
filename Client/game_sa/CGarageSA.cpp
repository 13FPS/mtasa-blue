/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CGarageSA.cpp
 *  PURPOSE:     Garage handling
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CGarageSA.h"
#include <game/CGarageDoorStatePolicy.h>

CGarageSA::CGarageSA(CGarageSAInterface* pInterface)
{
    internalInterface = pInterface;
}

void CGarageSA::SetType(BYTE type)
{
    internalInterface->Type = type;
}

void CGarageSA::GetPosition(CVector& vecPosition) const
{
    const CVector& internalPosition = internalInterface->vecPosition;
    vecPosition = internalPosition;
}

void CGarageSA::GetDirection(float& x1, float& y1, float& x2, float& y2) const
{
    x1 = internalInterface->DirectionA_X;
    y1 = internalInterface->DirectionA_Y;
    x2 = internalInterface->DirectionB_X;
    y2 = internalInterface->DirectionB_Y;
}

void CGarageSA::GetSize(float& height, float& width, float& depth) const
{
    height = internalInterface->Height;
    width = internalInterface->Width;
    depth = internalInterface->Depth;
}

void CGarageSA::GetBoundingBox(float& left, float& right, float& front, float& back) const
{
    left = internalInterface->Left;
    right = internalInterface->Right;
    front = internalInterface->Front;
    back = internalInterface->Back;
}

void CGarageSA::SetOpen(bool bOpen)
{
    // Reuse the actual SDK byte policy across backends; native script door
    // transitions may deliberately report a different notion of open.
    internalInterface->DoorState = MtaGame::CGarageDoorStatePolicy::StateForOpen(bOpen);
}

bool CGarageSA::IsOpen() const
{
    return MtaGame::CGarageDoorStatePolicy::IsOpen(internalInterface->DoorState);
}

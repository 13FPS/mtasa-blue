/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CGaragesSA.cpp
 *  PURPOSE:     Garage manager
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CGaragesSA.h"
#include <game/CGarageInitializationPolicy.h>

CGaragesSA::CGaragesSA(CGaragesSAInterface* pInterface)
{
    internalInterface = pInterface;

    for (int i = 0; i < MAX_GARAGES; i++)
    {
        Garages[i] = new CGarageSA((CGarageSAInterface*)(CLASS_CGarages + 216 * i));
    }
}

CGaragesSA::~CGaragesSA()
{
    for (int i = 0; i < MAX_GARAGES; i++)
    {
        delete Garages[i];
        Garages[i] = NULL;
    }
}

void CGaragesSA::Initialize()
{
    // Keep MTA's exact IDs/order while this backend retains its native array
    // ownership and direct SetType behavior, including exception propagation.
    MtaGame::CGarageInitializationPolicy::Initialize([this](std::uint32_t index, std::uint8_t type) { Garages[index]->SetType(type); });
}

CGarageSA* CGaragesSA::GetGarage(DWORD dwID)
{
    if (dwID >= 0 && dwID < MAX_GARAGES)
    {
        return Garages[dwID];
    }
    return NULL;
}

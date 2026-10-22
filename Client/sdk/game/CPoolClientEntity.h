/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Existing SDK pool wrapper/client pair without native pool bindings
 *****************************************************************************/
#pragma once

class CClientEntity;

// Keep the actual SDK pair independent of classic PCH/native pool headers so
// the shared registry policy can be tested without fabricating a game object.
// Public pool registries instantiate T with the common SDK interface.
// Actual backend construction performs the ordinary upcast once when
// registering; callers mutate this same pair, not a cached converted view.
template <class T>
struct SClientEntity
{
    T*             pEntity;
    CClientEntity* pClientEntity;
};

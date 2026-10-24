/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Resolve an existing targeted ped through its common pool registry
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <SharedUtil.Defines.h>
// Keep the existing SDK's legacy forward bone enum warning local to that
// include; normal client PCH consumers and standalone consumers use one SDK.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CPed.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <game/CPools.h>

namespace MtaClient
{
    template <class GetPools>
    CClientEntity* ResolveTargetClient(CEntity* target, GetPools&& getPools)
    {
        // Preserve the actual client's order: a missing target never queries
        // the game service, and each present target selects the pools once.
        if (!target)
            return nullptr;
        CPools* const pools = getPools();

        // Actual player targeting already resolves the same pool's canonical
        // wrapper. Read that row's current client association directly, so a
        // DE ped need not expose a classic native interface. StoredPointer is
        // separate mutable bookkeeping and cannot replace this association.
        if (auto* ped = dynamic_cast<CPed*>(target))
            return pools->GetPedClientEntity(ped);

        return pools->GetClientEntity(reinterpret_cast<DWORD*>(target->GetInterface()));
    }
}

/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve explicit null clear and admit only controlled ped targets
 *****************************************************************************/
#include "CWorldIgnoreEntityDE.h"
// Keep the shared SDK's legacy bone-enum forward declaration local, as in the
// existing CPed layers, without weakening diagnostics for this implementation.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CPed.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    namespace
    {
        void RequireIgnore(ClockResult result)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error("DE world owner refused IgnoreEntity: " + std::to_string(static_cast<unsigned>(result)));
        }
    }

    void CWorldIgnoreEntityDE::IgnoreEntity(CEntity* entity)
    {
        // Null is an explicit world clear and needs no ped registry lookup.
        // Other targets must already be controlled SDK peds; silently clearing
        // for an unsupported native wrapper would claim its missing support.
        if (!entity)
        {
            RequireIgnore(ignoreOwner.ClearWorldIgnoredEntity());
            return;
        }
        auto* ped = dynamic_cast<CPed*>(entity);
        if (!ped)
        {
            RequireIgnore(ClockResult::InvalidArgument);
            return;
        }
        NativePedReferenceToken selected;
        RequireIgnore(pools.ReadBorrowedPedReference(ped, selected));
        // This CPU copy adds no reference registration or deferred lifetime
        // lease. The sole native owner revalidates it and owns clear/end debt.
        RequireIgnore(ignoreOwner.SetWorldIgnoredPed(selected));
    }
}

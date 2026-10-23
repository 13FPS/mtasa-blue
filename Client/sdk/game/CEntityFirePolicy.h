/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the actual engine-independent nonconst entity fallback
 ****************************************************************************/
#pragma once
namespace MtaGame
{
    struct CEntityFirePolicy
    {
        // Classic CEntitySA supplies this nonconst fallback. CPed's distinct
        // const overload observes its native fire association instead; this
        // policy must never replace that query or imply native owner readiness.
        static constexpr bool IsOnFire() noexcept { return false; }
    };
}

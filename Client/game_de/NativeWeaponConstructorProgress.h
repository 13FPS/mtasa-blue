/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Value-only progress of the retained native constructor weapon phase
 *****************************************************************************/
#pragma once
#include <cstdint>

namespace mta::game_de
{
    // This receipt names neither engine storage nor an SDK service. Complete
    // describes the existing manager's weapon phase, never full CGame startup.
    struct NativeWeaponConstructorProgress
    {
        std::uint32_t completedRows = 0, registeredRows = 0;
        bool          complete = false, failed = false;
    };
}

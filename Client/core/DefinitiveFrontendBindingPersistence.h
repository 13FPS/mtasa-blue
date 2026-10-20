/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit opt-in for retained MTA movement-binding persistence
 *****************************************************************************/
#pragma once
#include "DefinitiveFrontend.h"

namespace DefinitiveFrontendBindingPersistence
{
    inline constexpr std::uint32_t ApiVersion = 1;
    using Status = DefinitiveFrontend::Status;
    // Handles are existing owner-thread frontend handles, and only views which
    // borrow retained runtime services are admitted. Nothing opts in implicitly.
    using Enable = Status (*)(void*, std::uint32_t);
    // Explicit owned paths let offline hosts avoid the actual user data folder.
    // The path is absolute, has no embedded NUL, and chars excludes a terminator.
    using EnableAtPath = Status (*)(void*, std::uint32_t, const wchar_t*, std::uint32_t);
}

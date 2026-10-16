/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Copied completed-Core lifecycle evidence for the script owner
 *****************************************************************************/
#pragma once
#include "../core/DefinitiveFrontendStartup.h"
#include <array>
#include <cstdint>

namespace deonline::FrontendStartupReport
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t Begin = 0;
    inline constexpr std::uint32_t AfterView = DefinitiveFrontendStartup::FieldCount;
    inline constexpr std::uint32_t Stopped = 2 * DefinitiveFrontendStartup::FieldCount;
    inline constexpr std::uint32_t FieldCount = 3 * DefinitiveFrontendStartup::FieldCount;
    static_assert(FieldCount == 96);
    using Snapshot = std::array<std::uint32_t, FieldCount>;
}

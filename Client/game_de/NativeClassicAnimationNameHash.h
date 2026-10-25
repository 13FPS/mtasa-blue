/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-exact classic animation name key policy without a row bound
 ****************************************************************************/
#pragma once
#include <cstdint>
namespace mta::game_de::NativeClassicAnimationNamePolicy
{
    // Hierarchies store only the source key. Unlike sequence names, their
    // original NUL-terminated input has no native 64-byte storage boundary.
    std::uint32_t Hash(const char* name);
}

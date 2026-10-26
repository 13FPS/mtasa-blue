/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Plain already-observed native weak origin, never a GC lifetime lease
 *****************************************************************************/
#pragma once
#include <cstdint>
namespace mta::game_de
{
    struct NativeModelReferenceOrigin
    {
        std::uintptr_t root = 0, map = 0;
        std::int32_t   rootIndex = -1;
        std::uint32_t  rootSerial = 0;
    };
}

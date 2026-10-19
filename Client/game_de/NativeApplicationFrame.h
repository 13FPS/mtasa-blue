/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the genuine DE application callback before startup writes
 *****************************************************************************/
#pragma once

#include <cstdint>

namespace mta::game_de
{
    // Raw engine state is deliberately separate from MTA's SystemState enum.
    // Observing a natural callback grants neither a UObject lifetime lease nor
    // permission to invoke lifecycle methods on the opaque native receiver.
    struct NativeApplicationFrame
    {
        std::uint64_t generation = 0;
        std::uint32_t nativeState = 0;
        float         deltaSeconds = 0;
    };

    using ApplicationFrameHandler = void (*)(const NativeApplicationFrame&, void*);
}

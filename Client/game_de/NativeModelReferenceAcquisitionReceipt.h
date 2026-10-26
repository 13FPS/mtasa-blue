/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native acquisition facts without importing the x64 call ABI
 *****************************************************************************/
#pragma once
#include <cstdint>
#include "NativeModelReferenceOrigin.h"

namespace mta::game_de
{
    // This is the existing acquisition receipt, not a second counter policy.
    // Keep the native ReservedRow ABI out of cross-width SDK-only transports.
    struct NativeModelReferenceAcquisitionReceipt
    {
        std::uintptr_t             root = 0, map = 0, row = 0, heap = 0;
        std::uint32_t              modelIndex = 0, before = 0, target = 0, observed = 0, exceptionCode = 0;
        std::int32_t               reservedIndex = -1, publishedIndex = -1;
        bool                       entered = false, reserved = false, published = false, accepted = false;
        NativeModelReferenceOrigin origin{};
    };
}

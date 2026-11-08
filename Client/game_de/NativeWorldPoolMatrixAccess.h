/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep real matrix detach/static restoration with the current owner
 ****************************************************************************/
#pragma once
#include "NativeWorldPoolMatrixMemory.h"

namespace mta::game_de
{
    class NativeWorldPoolMatrixAccess
    {
    public:
        using Kind = NativeWorldPoolMatrixMemory::Kind;
        using Transform = NativeWorldPoolMatrixMemory::Transform;
        virtual ~NativeWorldPoolMatrixAccess() = default;
        virtual ClockResult ReadWorldPoolMatrix(Kind, std::uint32_t, Transform&, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult DetachWorldPoolMatrix(Kind, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RestoreWorldPoolMatrix(Kind, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RestoreAllWorldPoolMatrices() noexcept { return ClockResult::NotInitialized; }
    };
}

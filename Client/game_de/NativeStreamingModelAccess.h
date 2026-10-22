/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA streaming intents with the existing admitted engine owner
 ****************************************************************************/
#pragma once
#include "NativeModelLoadedReader.h"

namespace mta::game_de
{
    class NativeStreamingModelAccess
    {
    public:
        virtual ~NativeStreamingModelAccess() = default;
        // Older/read-only owners cannot turn a request into a successful no-op.
        // Native entry, partial effects and cleanup remain the runtime's concern.
        virtual ClockResult RequestStreamingModel(std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadStreamingModelLoaded(std::uint32_t, NativeModelLoadedReader::Value&) noexcept { return ClockResult::NotInitialized; }
    };
}

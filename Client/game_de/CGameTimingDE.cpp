/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Supply checked DE timing through existing MTA CGame method contracts
 *****************************************************************************/
#include "CGameTimingDE.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    NativeGameTimingValues CGameTimingDE::RequireTiming(const NativeGameTimingReader& reader)
    {
        NativeGameTimingValues values;
        const auto             result = reader.Read(values);
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame timing unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return values;
    }

    DWORD CGameTimingDE::GetSystemTime()
    {
        return RequireTiming(timing).clock.nativeTimer;
    }

    int CGameTimingDE::GetSystemFrameCounter() const
    {
        const auto values = RequireTiming(timing);
        static_assert(sizeof(int) == sizeof(values.frameCounter));
        int result = 0;
        // Preserve the native counter's bits at wraparound; the existing SDK
        // returns int even though native storage/increment is a 32-bit word.
        std::memcpy(&result, &values.frameCounter, sizeof(result));
        return result;
    }

    float CGameTimingDE::GetTimeStep()
    {
        return RequireTiming(timing).timeStep;
    }

    float CGameTimingDE::GetOldTimeStep()
    {
        return RequireTiming(timing).oldTimeStep;
    }

    float CGameTimingDE::GetTimeScale()
    {
        return RequireTiming(timing).timeScale;
    }

    unsigned long CGameTimingDE::GetMinuteDuration()
    {
        return RequireTiming(timing).clock.millisecondsPerGameMinute;
    }
}

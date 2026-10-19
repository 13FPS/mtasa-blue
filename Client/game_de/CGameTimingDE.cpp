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
    namespace
    {
        void RequireTimingWrite(ClockResult result, const char* operation)
        {
            // CGame's void setters cannot return a native refusal. Match the
            // getters' explicit failure policy so a caller cannot mistake an
            // unqualified producer thread for an applied engine mutation.
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE CGame ") + operation + " unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) +
                                         ")");
        }
    }

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

    float CGameTimingDE::RequireFPS(const NativeGameTimingReader& reader)
    {
        float      value = 0;
        const auto result = reader.ReadFPS(value);
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame FPS unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return value;
    }

    float CGameTimingDE::GetFPS()
    {
        return RequireFPS(timing);
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

    float CGameTimingDE::GetGameSpeed()
    {
        // MTA's two SDK names read the same native time-scale scalar. Keep
        // their freshness and refusal behavior identical through one path.
        return GetTimeScale();
    }

    unsigned long CGameTimingDE::GetMinuteDuration()
    {
        return RequireTiming(timing).clock.millisecondsPerGameMinute;
    }

    void CGameTimingDE::SetTimeScale(float value)
    {
        RequireTimingWrite(timingWriter.SetTimeScale(value), "SetTimeScale");
    }

    void CGameTimingDE::SetGameSpeed(float value)
    {
        // Both names address the same native scale, just as their SDK getters.
        SetTimeScale(value);
    }

    void CGameTimingDE::SetMinuteDuration(unsigned long value)
    {
        static_assert(sizeof(value) == sizeof(std::uint32_t));
        RequireTimingWrite(timingWriter.SetMinuteDuration(value), "SetMinuteDuration");
    }

    void CGameTimingDE::Pause(bool value)
    {
        // The clock/weather callback stops while paused. Its native owner must
        // also retain a timer callback that can unpause and retire this write.
        RequireTimingWrite(timingWriter.Pause(value), "Pause");
    }
}

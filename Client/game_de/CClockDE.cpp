/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA clock semantics while preserving checked native failures
 *****************************************************************************/
#include "CClockDE.h"

namespace mta::game_de
{
    ClockResult CClockDE::Record(ClockResult result) const noexcept
    {
        // Foreign-thread refusals can race an owner's diagnostic observation;
        // only the result is shared. Native time is never cached here.
        lastResult.store(result);
        return result;
    }

    ClockResult CClockDE::TrySet(BYTE hour, BYTE minute) noexcept
    {
        // CClockSA accepts minute 60. Preserve that exact SDK behavior instead
        // of calling DE's setter, which normalizes it and changes other fields.
        if (hour > 23 || minute > 60)
            return Record(ClockResult::InvalidArgument);
        return Record(access.Set(hour, minute));
    }

    ClockResult CClockDE::TryGet(BYTE* hour, BYTE* minute) noexcept
    {
        if (!hour || !minute)
            return Record(ClockResult::InvalidArgument);
        ClockValues values;
        const auto  result = access.Read(values);
        if (result == ClockResult::Ok)
        {
            // Publish a coherent successful read only. The classic minute-first
            // ordering also preserves its behavior when both outputs alias.
            *minute = values.minute;
            *hour = values.hour;
        }
        return Record(result);
    }

    ClockResult CClockDE::TrySetTimeFrozen(bool value) noexcept
    {
        return Record(access.Freeze(value));
    }

    ClockResult CClockDE::TryIsTimeFrozen(bool& output) const noexcept
    {
        bool       value = false;
        const auto result = access.Frozen(value);
        if (result == ClockResult::Ok)
            output = value;
        return Record(result);
    }

    ClockResult CClockDE::TryResetTimeFrozen() noexcept
    {
        // Reset means unfreeze in the MTA interface. Restoring a trial's prior
        // native state is a separate runtime ownership obligation.
        return Record(access.Freeze(false));
    }

    void CClockDE::Set(BYTE hour, BYTE minute) noexcept
    {
        TrySet(hour, minute);
    }

    void CClockDE::Get(BYTE* hour, BYTE* minute) noexcept
    {
        TryGet(hour, minute);
    }

    bool CClockDE::SetTimeFrozen(bool value) noexcept
    {
        return TrySetTimeFrozen(value) == ClockResult::Ok;
    }

    bool CClockDE::IsTimeFrozen() const noexcept
    {
        // The SDK cannot represent an error here. Checked callers use Try and
        // LastResult; a failed read never reuses an earlier successful value.
        bool value = false;
        TryIsTimeFrozen(value);
        return value;
    }

    bool CClockDE::ResetTimeFrozen() noexcept
    {
        return TryResetTimeFrozen() == ClockResult::Ok;
    }
}

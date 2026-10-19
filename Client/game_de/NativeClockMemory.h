/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared native clock reads for SDK clock and startup time consumers
 *****************************************************************************/
#pragma once

#include "NativeClockProfile.h"
#include "NativeImageAccess.h"

namespace mta::game_de::NativeClockMemory
{
    template <typename Values>
    bool Read(std::uintptr_t image, Values& output) noexcept
    {
        const auto    read = [image](std::uintptr_t rva, auto& value) { return NativeImageAccess::ReadImage(image, image + rva, &value, sizeof(value), true); };
        Values        value;
        std::uint32_t after = 0;
        if (!read(NativeClockProfile::nativeTimerRva, value.nativeTimer) || !read(NativeClockProfile::hoursRva, value.hour) ||
            !read(NativeClockProfile::minutesRva, value.minute) || !read(NativeClockProfile::secondsRva, value.seconds) ||
            !read(NativeClockProfile::lastChangeRva, value.lastMinuteChange) || !read(NativeClockProfile::minuteDurationRva, value.millisecondsPerGameMinute) ||
            !read(NativeClockProfile::nativeTimerRva, after) || value.nativeTimer != after || value.hour > 23 || value.minute > 60 ||
            !value.millisecondsPerGameMinute || value.millisecondsPerGameMinute > 3600000)
            return false;
        // Preserve the clock service's minute-60 behavior and checked staging.
        // The startup reader adds a second complete equal sample below this
        // shared primitive because it has no qualified engine callback owner.
        output = value;
        return true;
    }

    template <typename Values>
    bool Equal(const Values& left, const Values& right) noexcept
    {
        return left.hour == right.hour && left.minute == right.minute && left.seconds == right.seconds && left.nativeTimer == right.nativeTimer &&
               left.lastMinuteChange == right.lastMinuteChange && left.millisecondsPerGameMinute == right.millisecondsPerGameMinute;
    }
}

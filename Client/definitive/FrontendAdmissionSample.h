/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional value-only admission observation without altering old reports
 ****************************************************************************/
#pragma once
#include <array>
#include <cstdint>
namespace deonline::FrontendAdmissionSample
{
    inline constexpr std::uint32_t ApiVersion = 1, FreshnessMs = 500, PollMs = 100;
    enum Field : std::uint32_t
    {
        Version,
        RuntimeGeneration,
        WorkerThread,
        Status,
        Admission,
        SerialLow,
        SerialHigh,
        TickLow,
        TickHigh,
        Reserved9,
        Reserved10,
        Reserved11,
        FieldCount
    };
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    inline std::uint64_t Pair(const Snapshot& value, Field low) noexcept
    {
        return std::uint64_t{value[low]} | (std::uint64_t{value[static_cast<std::uint32_t>(low) + 1]} << 32);
    }
    // Failed samples carry their real failed status and zero admission kind.
    // An observation never grants a native lease; the real Show rechecks it.
    static_assert(FieldCount == 12);
}

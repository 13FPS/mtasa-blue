/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native game-time access through the retained MTA Core owner
 *****************************************************************************/
#pragma once

#include "DefinitiveFrontendStartup.h"

namespace DefinitiveStartupGameTime
{
    inline constexpr std::uint32_t ApiVersion = 1;
    using Status = DefinitiveFrontendStartup::Status;
    enum Field : std::uint32_t
    {
        Version,
        OwnerThread,
        CoreRetained,
        BackendStarted,
        SampleAvailable,
        NativeStatus,
        NativeProducerThread,
        Hour,
        Minute,
        SecondsRaw,
        NativeTimer,
        LastMinuteChange,
        MillisecondsPerGameMinute,
        Reserved13,
        Reserved14,
        Reserved15,
        FieldCount
    };
    static_assert(FieldCount == 16);
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using Read = Status (*)(void*, std::uint32_t*, std::uint32_t);

    // Explicit reads preserve startup's engine-independent Begin. Each call
    // samples afresh; unavailable native time is never replaced by wall time
    // or a previously successful sample. This does not expose a partial CGame.
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    Status ReadOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept;
#endif
}

/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Core-owned startup facts using the existing application-frame transaction
 *****************************************************************************/

#include "DefinitiveStartupObservationReference.h"
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include "CGameAdapterModule.h"
    #include <windows.h>

namespace DefinitiveStartupObservationReference
{
    Snapshot PublishNotEntered(const DefinitiveInputReference::Request& request, bool adapterStopped) noexcept
    {
        Snapshot values{};
        values[Version] = ApiVersion;
        values[Phase] = adapterStopped ? 2u : 3u;
        values[Sequence] = 1;
        values[Reason] = static_cast<std::uint32_t>(StopReason::PreflightRejected);
        values[CleanupPending] = adapterStopped ? 0u : 1u;
        values[CleanupComplete] = adapterStopped ? 1u : 0u;
        values[NotEntered] = 1;
        values[AdapterStopped] = adapterStopped ? 1u : 0u;
        // Preflight refusal is independent of an application observation. It
        // can certify adapter shutdown without inventing native restoration.
        if (request.publish)
            try
            {
                request.publish(values.data(), FieldCount);
            }
            catch (...)
            {
                values[Reason] = static_cast<std::uint32_t>(StopReason::Exception);
            }
        return values;
    }

    Snapshot Run(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept
    {
        struct Owner
        {
            CGameAdapterModule& adapter;
            bool                Begin(const mta::game_de::ClockServiceRequestV1& value, mta::game_de::ClockServiceStateV1& state)
            {
                return adapter.BeginNativeStartupObservation(value, state);
            }
            bool Renew(std::uint64_t generation) { return adapter.RenewNativeClockService(generation); }
            bool Read(mta::game_de::ClockServiceStateV1& state) { return adapter.ReadNativeClockService(state); }
            bool ReadObservation(std::uint64_t generation, mta::game_de::StartupObservationStateV1& state)
            {
                return adapter.ReadNativeStartupObservation(generation, state);
            }
            bool End(std::uint64_t generation) { return adapter.EndNativeClockService(generation); }
            bool Reset() { return adapter.Reset(); }
            bool Focused() const noexcept
            {
                DWORD      process = 0;
                const auto foreground = GetForegroundWindow();
                return foreground && GetWindowThreadProcessId(foreground, &process) && process == GetCurrentProcessId();
            }
        } owner{adapter};
        struct Clock
        {
            std::uint64_t Now() const noexcept { return GetTickCount64(); }
            void          Wait(std::uint32_t milliseconds) const noexcept { Sleep(milliseconds); }
        } clock;
        // Every module pointer and diagnostic buffer is prepared by the
        // existing Core consumer before native Begin. The shared lifecycle
        // thereafter uses only fixed values and atomic bridge callbacks.
        const Request reference{request.durationMs, request.cancelled, request.publish};
        return DefinitiveStartupObservationReference::RunLifecycle(owner, clock, reference);
    }
}
#endif

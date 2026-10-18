/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

// A controlled DLL supplies loader-lock delay and thread-affinity observations.
// It contains no MTA/Game DE implementation and never touches game memory.
#include <algorithm>
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include "../../Client/core/DefinitiveInputReference.h"
#include "../../Client/core/DefinitiveMovementReference.h"
#include "../../Client/core/DefinitiveClockReference.h"
#include "../../Client/core/DefinitiveClockSetReference.h"
#include "../../Client/core/DefinitiveRenderReference.h"
#include "../../Client/core/DefinitiveGuiStartupReference.h"
#include "../../Client/core/DefinitiveGuiSurfaceReference.h"
#include <filesystem>
#include <stdexcept>

namespace
{
    HANDLE                            mapping = nullptr;
    volatile LONG*                    counters = nullptr;
    bool                              mtaSet = false, gtaSet = false;
    DefinitiveInputReference::Request inputRequest;
    bool                              guiRequest = false, guiSurfaceRequest = false, clockRequest = false, clockSetRequest = false;

    DWORD Setting(const wchar_t* name)
    {
        wchar_t text[256]{};
        GetEnvironmentVariableW(name, text, 256);
        return static_cast<DWORD>(std::wcstoul(text, nullptr, 10));
    }
    void Call(unsigned counter)
    {
        if (!counters)
            return;
        if (static_cast<DWORD>(counters[0]) != GetCurrentThreadId())
            InterlockedIncrement(counters + 2);
        InterlockedIncrement(counters + counter);
    }
    HANDLE Event(const wchar_t* variable, DWORD access)
    {
        wchar_t name[256]{};
        if (!GetEnvironmentVariableW(variable, name, 256))
            return nullptr;
        return OpenEventW(access, FALSE, name);
    }
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEInputReferenceApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_INPUT_MODE") == 3 ? 2u : 1u;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEInputReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                             DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveInputReference::MaximumDurationMs || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish};
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEMovementReferenceApiVersion()
{
    Call(7);
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEClockReferenceApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_CLOCK_MODE") == 2 ? 2u : DefinitiveClockReference::ApiVersion;
}

#ifndef CORE_FIXTURE_MISSING_CLOCK_EXPORT
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEClockReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                             DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveClockReference::MaximumDurationMs || !cancelled || !publish || Setting(L"MTA_CORE_FIXTURE_CLOCK_MODE") == 3)
        return 0;
    inputRequest = {duration, cancelled, publish};
    clockRequest = true;
    return 1;
}
#endif

extern "C" __declspec(dllexport) std::uint32_t MTA_DEClockSetReferenceApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_CLOCK_SET_MODE") == 2 ? 2u : DefinitiveClockSetReference::ApiVersion;
}

#ifndef CORE_FIXTURE_MISSING_CLOCK_SET_EXPORT
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEClockSetReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                                DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveClockSetReference::MaximumDurationMs || !cancelled || !publish || Setting(L"MTA_CORE_FIXTURE_CLOCK_SET_MODE") == 3)
        return 0;
    inputRequest = {duration, cancelled, publish};
    clockSetRequest = true;
    return 1;
}
#endif

void ControlledClock()
{
    using namespace DefinitiveClockReference;
    const auto mode = Setting(L"MTA_CORE_FIXTURE_CLOCK_MODE");
    Snapshot   sample{};
    sample[Version] = ApiVersion;
    sample[Phase] = 1;
    sample[BeginAttempted] = sample[BeginSucceeded] = sample[ReadyObserved] = sample[ReadOnlyVerified] = sample[NativeStateValid] = 1;
    sample[GenerationLow] = 17;
    sample[EngineThread] = GetCurrentThreadId() + 1;
    sample[ProducerThread] = GetCurrentThreadId();
    sample[NativeBound] = sample[NativeHooked] = sample[NativeActive] = sample[NativeReady] = 1;
    sample[Hour] = 12;
    sample[Minute] = 34;
    sample[Seconds] = 27;
    sample[MillisecondsPerGameMinute] = 1000;
    sample[ReadSubmitted] = sample[ReadAcknowledged] = sample[ReadsLow] = 3;
    sample[CallbacksLow] = sample[QualifiedCallbacksLow] = 10;
    sample[NativeTimerProgressSamples] = 2;
    // Load/PIN the genuine backend only to exercise the worker's independent
    // module accounting. This controlled Core never calls its native factory.
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    const auto backendPath = std::filesystem::path(path).parent_path() / L"game_de.dll";
    const auto backend = LoadLibraryExW(backendPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE    retained = nullptr;
    const auto factory = backend ? GetProcAddress(backend, "CreateGameAdapter") : nullptr;
    sample[BackendPinnedExpected] =
        factory && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(factory), &retained)
            ? 1u
            : 0u;
    if (backend)
        FreeLibrary(backend);
    const auto emit = [&]
    {
        ++sample[Sequence];
        sample[NativeTimer] = 10000 + sample[Sequence];
        sample[LastMinuteChange] = 9900 + sample[Sequence];
        Call(9);
        inputRequest.publish(sample.data(), FieldCount);
    };
    emit();
    const auto release = Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE);
    while (release && WaitForSingleObject(release, 5) == WAIT_TIMEOUT && !inputRequest.cancelled())
        emit();
    if (release)
        CloseHandle(release);
    sample[Phase] = 3;
    sample[CleanupPending] = sample[RetainedPeerThreads] = sample[CleanupAttempts] = 1;
    sample[NativeActive] = sample[NativeReady] = 0;
    emit();
    Call(21);
    // A protocol fault that returns pending cannot be repaired by another
    // thread's publication. The worker must quarantine its Core and TLS owner.
    if (mode == 7 || mode == 8 || mode == 12)
        return;
    const auto cleanup = Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE);
    while (cleanup && WaitForSingleObject(cleanup, 5) == WAIT_TIMEOUT)
    {
        ++sample[CleanupAttempts];
        emit();
    }
    if (cleanup)
        CloseHandle(cleanup);
    sample[Phase] = 2;
    sample[Reason] = static_cast<std::uint32_t>(inputRequest.cancelled() ? StopReason::Cancelled : StopReason::Completed);
    sample[CleanupComplete] = sample[EndSucceeded] = sample[NativeRestored] = sample[WorkloadComplete] = 1;
    sample[CleanupPending] = sample[NativeBound] = sample[NativeHooked] = sample[RetainedPeerThreads] = 0;
    sample[ElapsedMs] = sample[WorkloadElapsedMs] = MaximumDurationMs;
    if (mode == 11)
        sample[WorkloadComplete] = 0;
    if (mode == 9)
        sample[Version] = ApiVersion + 1;
    if (mode == 10)
        sample[RetainedPeerThreads] = 1;
    emit();
}

void ControlledClockSet()
{
    using namespace DefinitiveClockSetReference;
    const auto mode = Setting(L"MTA_CORE_FIXTURE_CLOCK_SET_MODE");
    Snapshot   sample{};
    if (mode == 14)
    {
        // This is explicit non-entry cleanup, never a successful time write.
        sample[Version] = ApiVersion;
        sample[Phase] = 2;
        sample[Sequence] = sample[CleanupComplete] = sample[NotEntered] = sample[AdapterStopped] = 1;
        sample[Reason] = static_cast<std::uint32_t>(StopReason::PreflightRejected);
        Call(9);
        inputRequest.publish(sample.data(), FieldCount);
        return;
    }
    sample[Version] = ApiVersion;
    sample[Phase] = 1;
    sample[BeginAttempted] = sample[BeginSucceeded] = sample[ReadyObserved] = sample[NativeStateValid] = 1;
    sample[GenerationLow] = 17;
    sample[EngineThread] = GetCurrentThreadId() + 1;
    sample[ProducerThread] = GetCurrentThreadId();
    sample[NativeBound] = sample[NativeHooked] = sample[NativeActive] = sample[NativeReady] = 1;
    sample[Hour] = 12;
    sample[Minute] = 34;
    sample[Seconds] = 27;
    sample[MillisecondsPerGameMinute] = 1000;
    sample[ReadSubmitted] = sample[ReadAcknowledged] = sample[ReadsLow] = 3;
    sample[CallbacksLow] = sample[QualifiedCallbacksLow] = 10;
    sample[NativeTimerProgressSamples] = 2;
    sample[SetsLow] = sample[InitialReadAcknowledged] = sample[SetSubmitted] = sample[SetAcknowledged] = 1;
    sample[InitialHour] = 12;
    sample[InitialMinute] = 34;
    sample[InitialSeconds] = 27;
    sample[InitialNativeTimer] = 10000;
    sample[InitialLastMinuteChange] = 9900;
    sample[InitialMillisecondsPerGameMinute] = 1000;
    sample[TargetHour] = sample[SetAcknowledgementHour] = 6;
    sample[TargetMinute] = sample[SetAcknowledgementMinute] = 15;
    sample[SetSequenceLow] = 2;
    sample[SetAcknowledgementNativeTimer] = 10001;
    sample[SetAcknowledgementSeconds] = 27;
    sample[CoherentSetReadback] = sample[SubsequentReadbackVerified] = 1;
    sample[SubsequentReadAcknowledged] = 2;
    sample[SetAcknowledgementElapsedMs] = 50;
    sample[LastReadbackNativeTimer] = 10002;
    sample[LastReadbackHour] = 6;
    sample[LastReadbackMinute] = 15;
    sample[LastReadbackSeconds] = 27;
    sample[ProviderCapturedSnapshotNotCompared] = sample[ExactlyOneSetVerified] = 1;
    sample[InitialReadSequenceLow] = sample[TargetProgressSamples] = 1;
    // Load/PIN the genuine backend only to exercise the worker's independent
    // module accounting. This controlled Core never calls its native factory.
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    const auto backendPath = std::filesystem::path(path).parent_path() / L"game_de.dll";
    const auto backend = LoadLibraryExW(backendPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE    retained = nullptr;
    const auto factory = backend ? GetProcAddress(backend, "CreateGameAdapter") : nullptr;
    sample[BackendPinnedExpected] =
        factory && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(factory), &retained)
            ? 1u
            : 0u;
    if (backend)
        FreeLibrary(backend);
    const auto emit = [&]
    {
        ++sample[Sequence];
        sample[NativeTimer] = 10000 + sample[Sequence];
        sample[LastMinuteChange] = 9900 + sample[Sequence];
        Call(9);
        inputRequest.publish(sample.data(), mode == 16 ? FieldCount - 1 : FieldCount);
    };
    emit();
    const auto release = Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE);
    while (release && WaitForSingleObject(release, 5) == WAIT_TIMEOUT && !inputRequest.cancelled())
        emit();
    if (release)
        CloseHandle(release);
    sample[Phase] = 3;
    sample[CleanupPending] = sample[RetainedPeerThreads] = sample[CleanupAttempts] = 1;
    sample[NativeActive] = sample[NativeReady] = 0;
    emit();
    Call(21);
    // A protocol fault that returns pending cannot be repaired by another
    // thread's publication. The worker must quarantine its Core and TLS owner.
    if (mode == 7 || mode == 8 || mode == 12)
        return;
    const auto cleanup = Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE);
    while (cleanup && WaitForSingleObject(cleanup, 5) == WAIT_TIMEOUT)
    {
        ++sample[CleanupAttempts];
        emit();
    }
    if (cleanup)
        CloseHandle(cleanup);
    sample[Phase] = 2;
    sample[Reason] = static_cast<std::uint32_t>(inputRequest.cancelled() ? StopReason::Cancelled : StopReason::Completed);
    sample[CleanupComplete] = sample[EndSucceeded] = sample[NativeRestored] = sample[WorkloadComplete] = 1;
    sample[CleanupPending] = sample[NativeBound] = sample[NativeHooked] = sample[RetainedPeerThreads] = 0;
    sample[ElapsedMs] = sample[WorkloadElapsedMs] = MaximumDurationMs;
    sample[ProviderRestorationVerified] = 1;
    if (mode == 11)
        sample[WorkloadComplete] = 0;
    if (mode == 9)
        sample[Version] = ApiVersion + 1;
    if (mode == 10)
        sample[RetainedPeerThreads] = 1;
    if (mode == 13)
        sample[SetSubmitted] = 2;
    if (mode == 15)
        sample[122] = 1;
    emit();
}

extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEMovementReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                                DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveMovementReference::MaximumDurationMs || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish, true};
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DERenderReferenceApiVersion()
{
    Call(7);
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDERenderReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                              DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveRenderReference::MaximumDurationMs || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish, false, true};
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DERenderReferenceApiVersionV2()
{
    Call(7);
    return 2;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDERenderReferenceV2(std::uint32_t duration, std::uint32_t route,
                                                                                DefinitiveInputReference::CancelCallback  cancelled,
                                                                                DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveRenderReference::MaximumDurationMs || (route != 1 && route != 2) || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish, false, true, route, 2};
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DERenderReferenceApiVersionV3()
{
    Call(7);
    return 3;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDERenderReferenceV3(std::uint32_t duration, std::uint32_t route,
                                                                                DefinitiveInputReference::CancelCallback  cancelled,
                                                                                DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveRenderReference::MaximumDurationMs || route != 2 || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish, false, true, route, 3};
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DERenderReferenceApiVersionV4()
{
    Call(7);
    return 4;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDERenderReferenceV4(std::uint32_t duration, std::uint32_t route,
                                                                                DefinitiveInputReference::CancelCallback  cancelled,
                                                                                DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveRenderReference::MaximumDurationMs || route != 2 || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish, false, true, route, 4};
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DERenderReferenceApiVersionV5()
{
    Call(7);
    return 5;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDERenderReferenceV5(std::uint32_t duration, std::uint32_t route,
                                                                                DefinitiveInputReference::CancelCallback  cancelled,
                                                                                DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveRenderReference::MaximumDurationMs || route != 2 || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish, false, true, route, 5};
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DERenderReferenceApiVersionV6()
{
    Call(7);
    return 6;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDERenderReferenceV6(std::uint32_t duration, std::uint32_t route,
                                                                                DefinitiveInputReference::CancelCallback  cancelled,
                                                                                DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveRenderReference::MaximumDurationMs || route != 2 || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish, false, true, route, 6};
    return 1;
}

#ifndef CORE_FIXTURE_MISSING_GUI_EXPORT
extern "C" __declspec(dllexport) std::uint32_t MTA_DEGuiStartupReferenceApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_GUI_MODE") == 3 ? 2u : DefinitiveGuiStartupReference::ApiVersion;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEGuiStartupReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                                  DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveGuiStartupReference::MaximumDurationMs || !cancelled || !publish || Setting(L"MTA_CORE_FIXTURE_GUI_MODE") == 4)
        return 0;
    guiRequest = true;
    inputRequest = {duration, cancelled, publish};
    return 1;
}
#endif

void ControlledGuiStartup()
{
    using namespace DefinitiveGuiStartupReference;
    const auto mode = Setting(L"MTA_CORE_FIXTURE_GUI_MODE");
    Snapshot   sample{};
    sample[Version] = ApiVersion;
    sample[Phase] = sample[Sequence] = 1;
    sample[ObserverStarted] = sample[DeviceAcquired] = sample[GuiPrepared] = 1;
    sample[LeaseGenerationLow] = 3;
    sample[LeaseGenerationHigh] = 2;
    sample[AcceptedSnapshotsLow] = sample[DispatchEntriesLow] = 7;
    sample[AcceptedSnapshotsHigh] = sample[DispatchEntriesHigh] = 1;
    sample[SourceProfile] = 2;
    sample[LeaseMetadataValid] = 1;
    sample[GuiStarted] = sample[QueueCreated] = sample[TargetCreated] = 1;
    sample[TargetFormat] = 24;
    sample[Width] = 64;
    sample[Height] = 32;
    // Controlled value-only transport evidence is not a GUI renderer. The
    // real backend is PINned only to exercise the explicit worker unload bit.
    if (mode != 11)
    {
        wchar_t path[32768]{};
        GetModuleFileNameW(nullptr, path, 32768);
        const auto backend = LoadLibraryExW((std::filesystem::path(path).parent_path() / L"game_de.dll").c_str(), nullptr,
                                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        HMODULE    retained = nullptr;
        const auto factory = backend ? GetProcAddress(backend, "CreateGameAdapter") : nullptr;
        sample[BackendPinnedExpected] =
            factory && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(factory), &retained)
                ? 1u
                : 0u;
        if (backend)
            FreeLibrary(backend);
    }
    Call(9);
    inputRequest.publish(sample.data(), FieldCount);
    if (mode == 8)
    {
        DefinitiveRenderReference::SnapshotV6 other{};
        other[DefinitiveRenderReference::Version] = 6;
        other[DefinitiveRenderReference::Phase] = 2;
        inputRequest.publish(other.data(), DefinitiveRenderReference::FieldCountV6);
    }
    const auto release = Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE);
    while (release && WaitForSingleObject(release, 5) == WAIT_TIMEOUT && !inputRequest.cancelled())
        Call(10);
    if (release)
        CloseHandle(release);
    sample[Phase] = 3;
    ++sample[Sequence];
    sample[CleanupPending] = sample[ObserverPendingCleanup] = sample[ModuleRetained] = sample[SessionRetained] = sample[DeviceRetained] = 1;
    sample[CleanupAttempts] = 1;
    inputRequest.publish(sample.data(), FieldCount);
    Call(21);
    const auto cleanupRelease = Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE);
    const auto cleanupAt = GetTickCount64();
    while (cleanupRelease && WaitForSingleObject(cleanupRelease, 50) == WAIT_TIMEOUT)
    {
        if (mode == 7 && GetTickCount64() - cleanupAt >= CleanupDeadlineMs)
            sample[CleanupDeadlineExceeded] = 1;
        ++sample[Sequence];
        ++sample[CleanupAttempts];
        inputRequest.publish(sample.data(), FieldCount);
    }
    if (cleanupRelease)
        CloseHandle(cleanupRelease);
    sample[Phase] = 2;
    ++sample[Sequence];
    sample[ElapsedMs] = MaximumDurationMs;
    sample[CleanupPending] = sample[ObserverPendingCleanup] = sample[ModuleRetained] = sample[SessionRetained] = sample[DeviceRetained] = 0;
    sample[CleanupComplete] = mode == 9 ? 0u : 1u;
    sample[ObserverEnded] = sample[GuiCleanupComplete] = 1;
    sample[WorkloadComplete] = 1;
    sample[WorkloadSucceeded] = mode == 1 ? 0u : 1u;
    sample[WorkloadHresult] = mode == 1 ? static_cast<std::uint32_t>(E_FAIL) : 0u;
    sample[Reason] = static_cast<unsigned>(inputRequest.cancelled() ? StopReason::Cancelled : mode == 1 ? StopReason::GpuError : StopReason::Completed);
    if (mode == 5)
        sample[Version] = 2;
    if (mode == 10)
        sample[ModuleRetained] = 1;
    // A final value-only sample cannot authorize worker success after an
    // unknown device Release effect, even if its other cleanup bits are zero.
    if (mode == 12)
        sample[DeviceReleaseFault] = 1;
    inputRequest.publish(sample.data(), FieldCount);
}

#ifndef CORE_FIXTURE_MISSING_SURFACE_EXPORT
extern "C" __declspec(dllexport) std::uint32_t MTA_DEGuiSurfaceReferenceApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_SURFACE_MODE") == 3 ? 2u : DefinitiveGuiSurfaceReference::ApiVersion;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEGuiSurfaceReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                                  DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveGuiSurfaceReference::MaximumDurationMs || !cancelled || !publish || Setting(L"MTA_CORE_FIXTURE_SURFACE_MODE") == 4)
        return 0;
    guiSurfaceRequest = true;
    inputRequest = {duration, cancelled, publish};
    return 1;
}
#endif

void ControlledGuiSurface()
{
    using namespace DefinitiveGuiStartupReference;
    const auto                              mode = Setting(L"MTA_CORE_FIXTURE_SURFACE_MODE");
    DefinitiveGuiSurfaceReference::Snapshot sample{};
    sample[Version] = ApiVersion;
    sample[Phase] = sample[Sequence] = 1;
    sample[ObserverStarted] = sample[DeviceAcquired] = sample[GuiPrepared] = 1;
    sample[LeaseGenerationLow] = 3;
    sample[LeaseGenerationHigh] = 2;
    sample[AcceptedSnapshotsLow] = sample[DispatchEntriesLow] = 7;
    sample[AcceptedSnapshotsHigh] = sample[DispatchEntriesHigh] = 1;
    sample[SourceProfile] = 2;
    sample[LeaseMetadataValid] = 1;
    sample[GuiStarted] = sample[QueueCreated] = sample[TargetCreated] = 1;
    sample[TargetFormat] = 87;
    sample[Width] = 64;
    sample[Height] = 32;
    sample[DefinitiveGuiSurfaceReference::ParentValidated] = sample[DefinitiveGuiSurfaceReference::ParentWindowed] = 1;
    sample[DefinitiveGuiSurfaceReference::ChildCreated] = sample[DefinitiveGuiSurfaceReference::CompositionCreated] = 1;
    sample[DefinitiveGuiSurfaceReference::PrivateSurface] = 1;
    // Controlled value-only transport evidence is not a GUI renderer. The
    // real backend is PINned only to exercise the explicit worker unload bit.
    if (mode != 11)
    {
        wchar_t path[32768]{};
        GetModuleFileNameW(nullptr, path, 32768);
        const auto backend = LoadLibraryExW((std::filesystem::path(path).parent_path() / L"game_de.dll").c_str(), nullptr,
                                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        HMODULE    retained = nullptr;
        const auto factory = backend ? GetProcAddress(backend, "CreateGameAdapter") : nullptr;
        sample[BackendPinnedExpected] =
            factory && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(factory), &retained)
                ? 1u
                : 0u;
        if (backend)
            FreeLibrary(backend);
    }
    Call(9);
    inputRequest.publish(sample.data(), DefinitiveGuiSurfaceReference::FieldCount);
    if (mode == 8)
    {
        DefinitiveGuiStartupReference::Snapshot other{};
        other[Version] = 1;
        other[Phase] = 2;
        inputRequest.publish(other.data(), DefinitiveGuiStartupReference::FieldCount);
    }
    const auto release = Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE);
    while (release && WaitForSingleObject(release, 5) == WAIT_TIMEOUT && !inputRequest.cancelled())
        Call(10);
    if (release)
        CloseHandle(release);
    sample[Phase] = 3;
    ++sample[Sequence];
    sample[CleanupPending] = sample[ObserverPendingCleanup] = sample[ModuleRetained] = sample[SessionRetained] = sample[DeviceRetained] = 1;
    sample[DefinitiveGuiSurfaceReference::ChildRetained] = sample[DefinitiveGuiSurfaceReference::QueueRetained] =
        sample[DefinitiveGuiSurfaceReference::SurfaceRetained] = 1;
    sample[CleanupAttempts] = 1;
    inputRequest.publish(sample.data(), DefinitiveGuiSurfaceReference::FieldCount);
    Call(21);
    const auto cleanupRelease = Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE);
    const auto cleanupAt = GetTickCount64();
    while (cleanupRelease && WaitForSingleObject(cleanupRelease, 50) == WAIT_TIMEOUT)
    {
        if (mode == 7 && GetTickCount64() - cleanupAt >= CleanupDeadlineMs)
            sample[CleanupDeadlineExceeded] = 1;
        ++sample[Sequence];
        ++sample[CleanupAttempts];
        inputRequest.publish(sample.data(), DefinitiveGuiSurfaceReference::FieldCount);
    }
    if (cleanupRelease)
        CloseHandle(cleanupRelease);
    sample[Phase] = 2;
    ++sample[Sequence];
    sample[ElapsedMs] = MaximumDurationMs;
    sample[CleanupPending] = sample[ObserverPendingCleanup] = sample[ModuleRetained] = sample[SessionRetained] = sample[DeviceRetained] = 0;
    sample[DefinitiveGuiSurfaceReference::ChildRetained] = mode == 13 ? 1u : 0u;
    sample[DefinitiveGuiSurfaceReference::QueueRetained] = mode == 14 ? 1u : 0u;
    sample[DefinitiveGuiSurfaceReference::SurfaceRetained] = mode == 15 ? 1u : 0u;
    sample[DefinitiveGuiSurfaceReference::WindowCleanupComplete] = mode == 16 ? 0u : 1u;
    sample[DefinitiveGuiSurfaceReference::CompositionCleanupComplete] = mode == 17 ? 0u : 1u;
    sample[DefinitiveGuiSurfaceReference::ClassUnregistered] = mode == 18 ? 0u : 1u;
    sample[DefinitiveGuiSurfaceReference::ResizePending] = mode == 22 ? 1u : 0u;
    sample[CleanupComplete] = mode == 9 ? 0u : 1u;
    sample[ObserverEnded] = mode == 19 ? 0u : 1u;
    sample[GuiCleanupComplete] = mode == 20 ? 0u : 1u;
    sample[GuiShutdownStatus] = mode == 21 ? 1u : 0u;
    sample[WorkloadComplete] = 1;
    sample[WorkloadSucceeded] = mode == 1 ? 0u : 1u;
    sample[WorkloadHresult] = mode == 1 ? static_cast<std::uint32_t>(E_FAIL) : 0u;
    sample[Reason] = static_cast<unsigned>(inputRequest.cancelled() ? StopReason::Cancelled : mode == 1 ? StopReason::GpuError : StopReason::Completed);
    if (mode == 5)
        sample[Version] = 2;
    if (mode == 10)
        sample[ModuleRetained] = 1;
    // A final value-only sample cannot authorize worker success after an
    // unknown device Release effect, even if its other cleanup bits are zero.
    if (mode == 12)
        sample[DeviceReleaseFault] = 1;
    inputRequest.publish(sample.data(), DefinitiveGuiSurfaceReference::FieldCount);
}

template <typename SnapshotType>
void ControlledRender()
{
    using namespace DefinitiveRenderReference;
    constexpr auto count = static_cast<std::uint32_t>(std::tuple_size<SnapshotType>::value);
    SnapshotType   sample{};
    sample[Version] = count == FieldCountV6   ? 6u
                      : count == FieldCountV5 ? 5u
                      : count == FieldCountV4 ? 4u
                      : count == FieldCountV3 ? 3u
                      : count == FieldCountV2 ? 2u
                                              : 1u;
    sample[Phase] = sample[Sequence] = sample[SampleCount] = 1;
    sample[ObserverStarted] = sample[Bound] = sample[Active] = 1;
    sample[PresentCallbacksLow] = sample[AcceptedSnapshotsLow] = 10;
    sample[GenerationHigh] = 2;
    sample[Width] = 1280;
    sample[Height] = 720;
    sample[NativeOriginalReturned] = 1;
    sample[NativePresence] = 15;
    if constexpr (count >= FieldCountV2)
    {
        // Warmup is intentionally unobserved. A route request alone cannot
        // make an RHI callback, queue, or presence field known.
        sample[RequestedRoute] = inputRequest.renderRoute;
        sample[PresentCallbacksLow] = sample[AcceptedSnapshotsLow] = 0;
        sample[NativeOriginalReturned] = sample[NativePresence] = sample[Width] = sample[Height] = 0;
    }
    // PIN the real backend without creating an observer or running engine code.
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    const auto backend = LoadLibraryExW((std::filesystem::path(path).parent_path() / L"game_de.dll").c_str(), nullptr,
                                        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE    retained = nullptr;
    const auto factory = backend ? GetProcAddress(backend, "CreateGameAdapter") : nullptr;
    sample[ModulePinned] = sample[BackendPinnedExpected] =
        factory && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(factory), &retained)
            ? 1u
            : 0u;
    if (backend)
        FreeLibrary(backend);
    Call(9);
    inputRequest.publish(sample.data(), count);
    const auto release = Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE);
    if (release)
    {
        WaitForSingleObject(release, 10000);
        CloseHandle(release);
    }
    sample[Phase] = 3;
    if constexpr (count >= FieldCountV2)
    {
        if (Setting(L"MTA_CORE_FIXTURE_RENDER_MODE") != 1)
        {
            sample[ObservedRoute] = inputRequest.renderRoute;
            sample[PresenceKnownMask] = inputRequest.renderRoute == 2 ? 19u : 15u;
            sample[NativePresence] = inputRequest.renderRoute == 2 ? 3u : 15u;
            sample[QueuePresent] = inputRequest.renderRoute == 2 ? 1u : 0u;
            sample[PresentCallbacksLow] = sample[AcceptedSnapshotsLow] = 10;
            sample[NativeOriginalReturned] = 1;
            sample[25] = GetCurrentThreadId();
            sample[Width] = 1280;
            sample[Height] = 720;
        }
    }
    if constexpr (count >= FieldCountV3)
    {
        // These are controlled worker transport values, not native evidence.
        // The actual COM provider is exercised by the independent WARP fixture.
        if (sample[ObservedRoute])
        {
            sample[ProbeAttemptsLow] = 10;
            const bool failed = Setting(L"MTA_CORE_FIXTURE_RENDER_MODE") == 5;
            sample[ProbeAcceptedLow] = failed ? 0u : 10u;
            sample[ProbeRejectedLow] = failed ? 10u : 0u;
            sample[QueryStage] = failed ? 8u : 19u;
            sample[QueryHresult] = failed ? static_cast<std::uint32_t>(E_NOINTERFACE) : 0u;
            sample[MetadataValid] = sample[SupportPredicate] = failed ? 0u : 1u;
            sample[DeviceNodeCount] = sample[NativeExplicitGPUCount] = sample[NativeAFRCount] = 1;
            sample[CanonicalIdentityFlags] = failed ? 1u : 7u;
            sample[SwapchainBufferCount] = 2;
            sample[SwapchainWidth] = sample[BackbufferWidthLow] = 1280;
            sample[SwapchainHeight] = sample[BackbufferHeight] = 720;
            sample[SwapchainFormat] = sample[BackbufferFormat] = 87;
            sample[BackbufferSampleCount] = 1;
            sample[QueryStatus] = failed ? 3u : 1u;
            if constexpr (count >= FieldCountV4)
            {
                // Classification is transported independently of COM success
                // and cannot turn the custom-present GPU exclusion into support.
                const bool unknown = Setting(L"MTA_CORE_FIXTURE_RENDER_MODE") == 6;
                sample[ProbeAcceptedLow] = sample[SupportPredicate] = 0;
                sample[ProbeRejectedLow] = 10;
                sample[CustomPresentPresent] = 1;
                sample[QueryStatus] = failed ? 3u : 2u;
                sample[80] = unknown ? 3u : 2u;
                sample[81] = unknown ? 0u : 1u;
                sample[82] = unknown ? 0u : 127u;
                sample[83] = unknown ? 0u : 15u;
                if constexpr (count >= FieldCountV5)
                {
                    // Controlled scalars prove transport and retained teardown,
                    // not the actual system Present or engine buffer association.
                    sample[84] = unknown ? 10u : failed ? 5u : 2u;
                    sample[85] = 1;
                    sample[86] = unknown ? 0u : failed ? 1u : 255u;
                    sample[88] = sample[89] = unknown ? 0u : 1u;
                    sample[91] = failed || unknown ? 0u : 1u;
                    sample[92] = 10;
                    sample[93] = 2;
                    sample[95] = 1;
                    sample[94] = failed ? 10u : 0u;
                    sample[104] = unknown ? 0u : sample[25];
                    sample[108] = unknown ? 0u : failed ? 3u : 9u;
                    sample[109] = failed ? static_cast<std::uint32_t>(E_NOINTERFACE) : 0u;
                    if constexpr (count == FieldCountV6)
                    {
                        // Distinct tails let the worker prove a dispatch result
                        // survives while the older standard endpoint refuses.
                        std::copy_n(sample.begin() + 84, 26, sample.begin() + 110);
                        sample[111] = 2;
                        std::fill(sample.begin() + 84, sample.begin() + 110, 0u);
                        sample[84] = 1;
                    }
                }
            }
        }
    }
    sample[CleanupPending] = sample[NativeCleanupPending] = sample[CleanupAttempts] = 1;
    ++sample[Sequence];
    inputRequest.publish(sample.data(), count);
    // The host holds the actual worker cache lock across this publication.
    // Progress here proves the production callback returned without waiting.
    if constexpr (count == FieldCountV6)
        Call(20);
    const auto cleanupRelease = Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE);
    const auto cleanupAt = GetTickCount64();
    while (cleanupRelease && WaitForSingleObject(cleanupRelease, 50) == WAIT_TIMEOUT)
    {
        if (GetTickCount64() - cleanupAt >= CleanupDeadlineMs)
        {
            sample[CleanupDeadlineExceeded] = 1;
            sample[Reason] = static_cast<std::uint32_t>(StopReason::CleanupError);
        }
        ++sample[Sequence];
        ++sample[CleanupAttempts];
        inputRequest.publish(sample.data(), count);
    }
    if (cleanupRelease)
        CloseHandle(cleanupRelease);
    sample[Phase] = 2;
    ++sample[Sequence];
    sample[ElapsedMs] = MaximumDurationMs;
    sample[Active] = sample[CleanupPending] = sample[NativeCleanupPending] = 0;
    sample[CleanupComplete] = 1;
    if (!sample[CleanupDeadlineExceeded])
        sample[Reason] = static_cast<std::uint32_t>(inputRequest.cancelled() ? StopReason::Cancelled : StopReason::TimeBudget);
    if constexpr (count >= FieldCountV2)
    {
        const auto malformed = Setting(L"MTA_CORE_FIXTURE_RENDER_MODE");
        if (malformed == 2)
            sample[ObservedRoute] = inputRequest.renderRoute == 1 ? 2u : 1u;
        else if (malformed == 3)
            sample[Version] = 1;
        else if (malformed == 4)
            sample[PresenceKnownMask] = inputRequest.renderRoute == 2 ? 15u : 19u;
    }
    inputRequest.publish(sample.data(), count);
}

void ControlledMovement()
{
    using namespace DefinitiveInputReference;
    DefinitiveMovementReference::Snapshot sample{};
    sample[Version] = 1;
    sample[Phase] = sample[Sequence] = 1;
    sample[Flags] = 3;
    sample[SupportedBindings] = sample[EligibleKeys] = 6;
    sample[Rearms] = sample[SampleCount] = 1;
    sample[DefinitiveMovementReference::ContextKnownMask] = sample[DefinitiveMovementReference::ContextFlags] = 15;
    sample[DefinitiveMovementReference::Eligible] = sample[DefinitiveMovementReference::WriterStarted] = 1;
    sample[DefinitiveMovementReference::NativeMetrics] = sample[DefinitiveMovementReference::NativeMetrics + 2] = 1;
    sample[DefinitiveMovementReference::NativeMetrics + 4] = 120;
    sample[DefinitiveMovementReference::NativeMetrics + 6] = 100;
    sample[DefinitiveMovementReference::NativeMetrics + 12] = 99;
    // PIN the real backend without calling its factory or native API. This
    // controlled Core is not MTA and proves only the worker's unload policy.
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    const auto backendPath = std::filesystem::path(path).parent_path() / L"game_de.dll";
    const auto backend = LoadLibraryExW(backendPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE    retained = nullptr;
    const auto factory = backend ? GetProcAddress(backend, "CreateGameAdapter") : nullptr;
    sample[DefinitiveMovementReference::BackendPinnedExpected] =
        factory && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(factory), &retained)
            ? 1u
            : 0u;
    if (backend)
        FreeLibrary(backend);
    Call(9);
    inputRequest.publish(sample.data(), DefinitiveMovementReference::FieldCount);
    const auto release = Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE);
    if (release)
    {
        WaitForSingleObject(release, 10000);
        CloseHandle(release);
    }
    sample[Phase] = 3;
    ++sample[Sequence];
    sample[DefinitiveMovementReference::NativeMetrics + 2] = 0;
    sample[DefinitiveMovementReference::CleanupPending] = sample[DefinitiveMovementReference::CleanupAttempts] = 1;
    inputRequest.publish(sample.data(), DefinitiveMovementReference::FieldCount);
    const auto cleanupRelease = Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE);
    if (cleanupRelease)
    {
        WaitForSingleObject(cleanupRelease, 10000);
        CloseHandle(cleanupRelease);
    }
    const bool cancelled = inputRequest.cancelled();
    sample[Phase] = 2;
    ++sample[Sequence];
    sample[ElapsedMs] = cancelled ? 50u : DefinitiveMovementReference::MaximumDurationMs;
    sample[Flags] = 0;
    sample[Reason] = cancelled ? 2u : 1u;
    sample[InputCleanupComplete] = 1;
    sample[DefinitiveMovementReference::CleanupPending] = sample[DefinitiveMovementReference::NativeMetrics] = 0;
    sample[DefinitiveMovementReference::CleanupAttempts] = 2;
    sample[DefinitiveMovementReference::NativeMetrics + 12] = 100;
    inputRequest.publish(sample.data(), DefinitiveMovementReference::FieldCount);
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEStartupApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_BAD_VERSION") ? 2u : 1u;
}
#ifndef CORE_FIXTURE_MISSING_EXPORT
extern "C" __declspec(dllexport) std::uint32_t MTA_ReadDEStartupState(std::uint32_t field)
{
    Call(6);
    if ((clockRequest && Setting(L"MTA_CORE_FIXTURE_CLOCK_MODE") == 12) || (clockSetRequest && Setting(L"MTA_CORE_FIXTURE_CLOCK_SET_MODE") == 12))
        throw std::runtime_error("controlled pending clock diagnostic exception");
    const std::array<std::uint32_t, 8> values{1, 5, 6, Setting(L"MTA_CORE_FIXTURE_BAD_STATE") ? 33u : 97u, 0, 0, 0, 1};
    return field < values.size() ? values[field] : 0xffffffffu;
}
#endif
extern "C" __declspec(dllexport) void SetMTADirectory(const wchar_t* directory, std::size_t size)
{
    Call(4);
    mtaSet = directory && size && directory[size] == L'\0';
}
extern "C" __declspec(dllexport) void SetGTADirectory(const wchar_t* directory, std::size_t size)
{
    Call(5);
    gtaSet = directory && size && directory[size] == L'\0';
}
extern "C" __declspec(dllexport) int InitializeCore()
{
    Call(3);
    if (Setting(L"MTA_CORE_FIXTURE_BAD_INITIALIZE"))
        return 0;
    if (mtaSet && gtaSet && clockSetRequest)
    {
        ControlledClockSet();
        const auto mode = Setting(L"MTA_CORE_FIXTURE_CLOCK_SET_MODE");
        Call(24);
        if (mode == 8 || mode == 12)
            Call(25);
        if (mode == 5 || mode == 8)
            throw std::runtime_error("controlled Set Initialize exception");
        return mode == 4 || mode == 7 ? 0 : 5;
    }
    if (mtaSet && gtaSet && clockRequest)
    {
        ControlledClock();
        const auto mode = Setting(L"MTA_CORE_FIXTURE_CLOCK_MODE");
        Call(24);
        if (mode == 8 || mode == 12)
            Call(25);
        if (mode == 5 || mode == 8)
            throw std::runtime_error("controlled clock Initialize exception");
        return mode == 4 || mode == 7 ? 0 : 5;
    }
    if (mtaSet && gtaSet && guiSurfaceRequest)
    {
        ControlledGuiSurface();
        return 5;
    }
    if (mtaSet && gtaSet && guiRequest)
    {
        ControlledGuiStartup();
        return 5;
    }
    if (mtaSet && gtaSet && inputRequest.renderObservation)
    {
        if (inputRequest.renderApiVersion == 6)
            ControlledRender<DefinitiveRenderReference::SnapshotV6>();
        else if (inputRequest.renderApiVersion == 5)
            ControlledRender<DefinitiveRenderReference::SnapshotV5>();
        else if (inputRequest.renderApiVersion == 4)
            ControlledRender<DefinitiveRenderReference::SnapshotV4>();
        else if (inputRequest.renderApiVersion == 3)
            ControlledRender<DefinitiveRenderReference::SnapshotV3>();
        else if (inputRequest.renderRoute)
            ControlledRender<DefinitiveRenderReference::SnapshotV2>();
        else
            ControlledRender<DefinitiveRenderReference::Snapshot>();
        return 5;
    }
    if (mtaSet && gtaSet && inputRequest.nativeMovement)
    {
        ControlledMovement();
        return 5;
    }
    if (mtaSet && gtaSet && inputRequest.publish)
    {
        using namespace DefinitiveInputReference;
        Snapshot sample{};
        sample[Version] = 1;
        sample[Phase] = 1;
        sample[Sequence] = 1;
        sample[Flags] = 7;
        sample[SupportedBindings] = sample[EligibleKeys] = 5;
        sample[Rearms] = sample[SampleCount] = 1;
        sample[HeldActionsLow] = 1u << 4;
        sample[Current + 1] = 0xffffff80u;
        if (Setting(L"MTA_CORE_FIXTURE_INPUT_MODE") != 2)
        {
            Call(9);
            inputRequest.publish(sample.data(), FieldCount);
            const auto release = Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE);
            if (release)
            {
                while (WaitForSingleObject(release, 5) == WAIT_TIMEOUT && !inputRequest.cancelled())
                    Call(10);
                CloseHandle(release);
            }
        }
        const bool cancelled = inputRequest.cancelled();
        sample[Phase] = sample[Sequence] = 2;
        sample[ElapsedMs] = cancelled ? 50u : MaximumDurationMs;
        sample[Flags] = 4;
        sample[HeldActionsLow] = 0;
        sample[Reason] = cancelled ? 2u : 1u;
        sample[InputCleanupComplete] = Setting(L"MTA_CORE_FIXTURE_INPUT_MODE") == 2 ? 0u : 1u;
        sample[Current + 1] = 0;
        Call(9);
        inputRequest.publish(sample.data(), FieldCount);
    }
    return !mtaSet ? 3 : !gtaSet ? 4 : 5;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        wchar_t name[256]{};
        if (!GetEnvironmentVariableW(L"MTA_CORE_FIXTURE_MAP", name, 256))
            return FALSE;
        mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
        if (!mapping)
            return FALSE;
        counters = static_cast<volatile LONG*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 64 * sizeof(LONG)));
        if (!counters)
        {
            CloseHandle(mapping);
            mapping = nullptr;
            return FALSE;
        }
        InterlockedExchange(counters, static_cast<LONG>(GetCurrentThreadId()));
        const auto entered = Event(L"MTA_CORE_FIXTURE_ENTERED", EVENT_MODIFY_STATE);
        const auto release = Event(L"MTA_CORE_FIXTURE_RELEASE", SYNCHRONIZE);
        if (entered)
        {
            SetEvent(entered);
            CloseHandle(entered);
        }
        // Holding DllMain deliberately exposes any hidden loader API on Poll or
        // Cancel. The host releases this gate only after responsive calls pass.
        Sleep(Setting(L"MTA_CORE_FIXTURE_DELAY_MS"));
        if (release)
        {
            WaitForSingleObject(release, 10000);
            CloseHandle(release);
        }
        if (Setting(L"MTA_CORE_FIXTURE_FAIL_ATTACH"))
            return FALSE;
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        if (counters)
        {
            InterlockedExchange(counters + 1, static_cast<LONG>(GetCurrentThreadId()));
            UnmapViewOfFile(const_cast<LONG*>(counters));
            counters = nullptr;
        }
        if (mapping)
        {
            CloseHandle(mapping);
            mapping = nullptr;
        }
    }
    return TRUE;
}

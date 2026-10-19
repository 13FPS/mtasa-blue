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
#include "../../Client/core/DefinitiveApplicationFrameReference.h"
#include "../../Client/core/DefinitiveStartupObservationReference.h"
#include "../../Client/core/DefinitiveRenderReference.h"
#include "../../Client/core/DefinitiveGuiStartupReference.h"
#include "../../Client/core/DefinitiveGuiSurfaceReference.h"
#include "../../Client/core/DefinitiveGuiFrontendInputReference.h"
#include "../../Client/core/DefinitiveFrontendClockReference.h"
#include "../../Client/core/DefinitiveFrontendStartup.h"
#include "../../Client/core/DefinitiveFrontendRuntime.h"
#include "../../Client/core/DefinitiveLocalInputRuntime.h"
#include <filesystem>
#include <stdexcept>

namespace
{
    HANDLE                                mapping = nullptr;
    volatile LONG*                        counters = nullptr;
    bool                                  mtaSet = false, gtaSet = false;
    DefinitiveInputReference::Request     inputRequest;
    bool                                  guiRequest = false, guiSurfaceRequest = false, clockRequest = false, clockSetRequest = false;
    bool                                  applicationFrameRequest = false;
    bool                                  startupObservationRequest = false;
    DefinitiveFrontendStartup::Snapshot   frontendOwner{};
    HMODULE                               frontendBackend = nullptr;
    bool                                  frontendClockReturned = false, frontendOwnerReadThrew = false;
    DefinitiveFrontendRuntime::Snapshot   runtimeOwner{};
    bool                                  runtimeViewActive = false;
    DefinitiveLocalInputRuntime::Snapshot localRuntime{};

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

extern "C" __declspec(dllexport) std::uint32_t MTA_DEApplicationFrameReferenceApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_APPLICATION_FRAME_MODE") == 2 ? 2u : DefinitiveApplicationFrameReference::ApiVersion;
}

#ifndef CORE_FIXTURE_MISSING_APPLICATION_FRAME_EXPORT
extern "C"
    __declspec(dllexport) std::uint32_t MTA_ConfigureDEApplicationFrameReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                                 DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveApplicationFrameReference::MaximumDurationMs || !cancelled || !publish ||
        Setting(L"MTA_CORE_FIXTURE_APPLICATION_FRAME_MODE") == 3)
        return 0;
    inputRequest = {duration, cancelled, publish};
    applicationFrameRequest = true;
    return 1;
}
#endif

void ControlledApplicationFrame()
{
    using namespace DefinitiveApplicationFrameReference;
    const auto mode = Setting(L"MTA_CORE_FIXTURE_APPLICATION_FRAME_MODE");
    Snapshot   sample{};
    sample[Version] = ApiVersion;
    if (mode == 14)
    {
        // A never-entered native scope authorizes unload, not a frame claim.
        sample[Phase] = 2;
        sample[Sequence] = sample[CleanupComplete] = sample[NotEntered] = sample[AdapterStopped] = 1;
        sample[Reason] = static_cast<std::uint32_t>(StopReason::PreflightRejected);
        Call(9);
        inputRequest.publish(sample.data(), FieldCount);
        return;
    }
    sample[Phase] = 1;
    sample[BeginAttempted] = sample[NativeStateValid] = 1;
    if (mode != 13)
    {
        sample[BeginSucceeded] = sample[ReadyObserved] = sample[ReadOnlyVerified] = 1;
        sample[GenerationLow] = 17;
        sample[EngineThread] = GetCurrentThreadId() + 1;
        sample[ProducerThread] = GetCurrentThreadId();
        sample[NativeBound] = sample[NativeHooked] = sample[NativeActive] = sample[NativeReady] = 1;
        sample[CallbacksLow] = sample[QualifiedCallbacksLow] = 10;
        sample[ObservationReads] = 3;
        sample[ObservationValid] = sample[DictionaryVerified] = 1;
        sample[ObservationProgressSamples] = 2;
        sample[RawNativeState] = 9;
        sample[FrontendFramesLow] = 40;
        sample[LoadingLookupsLow] = sample[LoadingKeyPresentLow] = sample[LoadingNonNullLow] = 8;
        sample[SimpleFadeLookupsLow] = sample[SimpleFadeKeyPresentLow] = sample[SimpleFadeNonNullLow] = 7;
        sample[LastLookupMask] = 63;
    }
    else
        sample[RefusedNoGeneration] = 1;
    // PIN accounting exercises the actual bridge unload predicate without
    // invoking the genuine backend's native factory from this owned host.
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
        if (mode != 13)
        {
            sample[FramesLow] = 100 + sample[Sequence];
            sample[PlayingFramesLow] = 60 + sample[Sequence];
        }
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
    // Deliberate faulty providers return before retirement to prove that the
    // real worker retains Core, TLS callbacks and its heap-backed paths.
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
    sample[Reason] = static_cast<std::uint32_t>(inputRequest.cancelled() ? StopReason::Cancelled : mode == 13 ? StopReason::BeginError : StopReason::Completed);
    sample[CleanupComplete] = 1;
    sample[CleanupPending] = sample[NativeBound] = sample[NativeHooked] = sample[RetainedPeerThreads] = 0;
    if (mode == 13)
        sample[ResetSucceeded] = 1;
    else
    {
        sample[EndSucceeded] = sample[NativeRestored] = sample[WorkloadComplete] = 1;
        sample[ElapsedMs] = sample[WorkloadElapsedMs] = MaximumDurationMs;
    }
    if (mode == 11)
        sample[WorkloadComplete] = 0;
    if (mode == 9)
        sample[Version] = ApiVersion + 1;
    if (mode == 10)
        sample[RetainedPeerThreads] = 1;
    emit();
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEStartupObservationReferenceApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_STARTUP_OBSERVATION_MODE") == 2 ? 2u : DefinitiveStartupObservationReference::ApiVersion;
}

#ifndef CORE_FIXTURE_MISSING_STARTUP_OBSERVATION_EXPORT
extern "C"
    __declspec(dllexport) std::uint32_t MTA_ConfigureDEStartupObservationReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                                   DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveStartupObservationReference::MaximumDurationMs || !cancelled || !publish ||
        Setting(L"MTA_CORE_FIXTURE_STARTUP_OBSERVATION_MODE") == 3)
        return 0;
    inputRequest = {duration, cancelled, publish};
    startupObservationRequest = true;
    return 1;
}
#endif

void ControlledStartupObservation()
{
    namespace Startup = DefinitiveStartupObservationReference;
    using namespace DefinitiveStartupObservationReference;
    // The extended protocol owns its width and constants; its imported frame
    // namespace must not select the older 128-word snapshot accidentally.
    const auto        mode = Setting(L"MTA_CORE_FIXTURE_STARTUP_OBSERVATION_MODE");
    Startup::Snapshot sample{};
    sample[Version] = Startup::ApiVersion;
    if (mode == 14)
    {
        // A never-entered native scope authorizes unload, not a frame claim.
        sample[Phase] = 2;
        sample[Sequence] = sample[CleanupComplete] = sample[NotEntered] = sample[AdapterStopped] = 1;
        sample[Reason] = static_cast<std::uint32_t>(StopReason::PreflightRejected);
        Call(9);
        inputRequest.publish(sample.data(), Startup::FieldCount);
        return;
    }
    sample[Phase] = 1;
    sample[BeginAttempted] = sample[NativeStateValid] = 1;
    if (mode != 13)
    {
        sample[BeginSucceeded] = sample[ReadyObserved] = sample[ReadOnlyVerified] = 1;
        sample[GenerationLow] = 17;
        sample[EngineThread] = GetCurrentThreadId() + 1;
        sample[ProducerThread] = GetCurrentThreadId();
        sample[NativeBound] = sample[NativeHooked] = sample[NativeActive] = sample[NativeReady] = 1;
        sample[CallbacksLow] = sample[QualifiedCallbacksLow] = 10;
        sample[ObservationReads] = 3;
        sample[ObservationValid] = sample[DictionaryVerified] = 1;
        sample[ObservationProgressSamples] = 2;
        sample[RawNativeState] = 9;
        sample[FrontendFramesLow] = 40;
        sample[LoadingLookupsLow] = sample[LoadingKeyPresentLow] = sample[LoadingNonNullLow] = 8;
        sample[SimpleFadeLookupsLow] = sample[SimpleFadeKeyPresentLow] = sample[SimpleFadeNonNullLow] = 7;
        sample[LastLookupMask] = 63;
    }
    else
        sample[RefusedNoGeneration] = 1;
    // PIN accounting exercises the actual bridge unload predicate without
    // invoking the genuine backend's native factory from this owned host.
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
        if (mode != 13)
        {
            sample[FramesLow] = 100 + sample[Sequence];
            sample[PlayingFramesLow] = 60 + sample[Sequence];
            // Extended words are distinct from the inherited frame128 proof.
            sample[StartupReads] = 3;
            sample[StartupStateValid] = 1;
            sample[ObservationsLow] = sample[FramesLow];
            sample[LoadingTypeObservedLow] = sample[LoadingCompatibleLow] = sample[FramesLow];
            sample[SimpleFadeTypeObservedLow] = sample[SimpleFadeCompatibleLow] = sample[FramesLow];
            sample[OwnerObservedLow] = sample[WorldObservedLow] = sample[FramesLow];
            sample[LastFacts] = sample[Phase] == 2 && mode == 17 ? 64u : 63u;
            if (sample[Phase] == 2 && mode == 18)
                sample[150] = 1;
        }
        Call(9);
        inputRequest.publish(sample.data(), mode == 16 ? Startup::FieldCount - 1 : Startup::FieldCount);
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
    // Deliberate faulty providers return before retirement to prove that the
    // real worker retains Core, TLS callbacks and its heap-backed paths.
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
    sample[Reason] = static_cast<std::uint32_t>(inputRequest.cancelled() ? StopReason::Cancelled : mode == 13 ? StopReason::BeginError : StopReason::Completed);
    sample[CleanupComplete] = 1;
    sample[CleanupPending] = sample[NativeBound] = sample[NativeHooked] = sample[RetainedPeerThreads] = 0;
    if (mode == 13)
        sample[ResetSucceeded] = 1;
    else
    {
        sample[EndSucceeded] = sample[NativeRestored] = sample[WorkloadComplete] = 1;
        sample[ElapsedMs] = sample[WorkloadElapsedMs] = Startup::MaximumDurationMs;
    }
    if (mode == 11)
        sample[WorkloadComplete] = 0;
    if (mode == 9)
        sample[Version] = Startup::ApiVersion + 1;
    if (mode == 10)
        sample[RetainedPeerThreads] = 1;
    emit();
}

void ControlledClock(bool completedOwner = false)
{
    using namespace DefinitiveClockReference;
    const auto frontendMode = completedOwner ? Setting(L"MTA_CORE_FIXTURE_FRONTEND_CLOCK_MODE") : 0u;
    const auto mode = completedOwner ? (frontendMode == 4                         ? 11u
                                        : frontendMode >= 7 && frontendMode <= 10 ? frontendMode
                                                                                  : 0u)
                                     : Setting(L"MTA_CORE_FIXTURE_CLOCK_MODE");
    Snapshot   sample{};
    sample[Version] = ApiVersion;
    sample[Phase] = 1;
    sample[BeginAttempted] = sample[BeginSucceeded] = sample[ReadyObserved] = sample[ReadOnlyVerified] = sample[NativeStateValid] = 1;
    sample[GenerationLow] = 17;
    sample[EngineThread] = GetCurrentThreadId() + 1;
    sample[ProducerThread] = GetCurrentThreadId();
    if (completedOwner && frontendMode == 17)
        ++sample[ProducerThread];
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
        inputRequest.publish(sample.data(), completedOwner && frontendMode == 18 ? FieldCount - 1 : FieldCount);
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
    sample[ElapsedMs] = sample[WorkloadElapsedMs] = completedOwner ? (frontendMode == 5 ? 1000u : 2000u) : MaximumDurationMs;
    if (mode == 11)
        sample[WorkloadComplete] = 0;
    if (mode == 9)
        sample[Version] = ApiVersion + 1;
    if (mode == 10)
        sample[RetainedPeerThreads] = 1;
    if (completedOwner && frontendMode == 20)
        sample[88] = 1;
    emit();
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEFrontendStartupApiVersion()
{
    Call(32);
    return DefinitiveFrontendStartup::ApiVersion;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEFrontendRuntimeApiVersion()
{
    return Setting(L"MTA_CORE_FIXTURE_RUNTIME_MODE") == 8 ? 2u : 1u;
}
extern "C" __declspec(dllexport) void* MTA_BeginDEFrontendRuntime(std::uint32_t version)
{
    namespace C = DefinitiveFrontendRuntime;
    namespace S = DefinitiveFrontendStartup;
    Call(44);
    if (version != 1 || !mtaSet || !gtaSet)
        return nullptr;
    runtimeOwner = {};
    runtimeOwner[S::Version] = runtimeOwner[C::RuntimeVersion] = 1;
    runtimeOwner[S::Phase] = runtimeOwner[C::RuntimePhase] = 2;
    runtimeOwner[S::OwnerThread] = runtimeOwner[C::OwnerThread] = GetCurrentThreadId();
    for (const auto field : {S::CoreRetained, S::BackendStartAttempted, S::BackendStarted, S::Pulses, S::LastViewCleanupComplete})
        runtimeOwner[field] = 1;
    runtimeOwner[S::CapabilitiesLow] = 33;
    for (std::uint32_t field = C::SessionGenerationLow; field <= C::BindingLoads; ++field)
        runtimeOwner[field] = field == C::SessionGenerationHigh ? 0u : 1u;
    runtimeOwner[C::HiddenNeutralVerified] = runtimeOwner[C::ConfigurationReadOnly] = 1;
    runtimeOwner[C::BindCount] = 87;
    localRuntime = {};
    localRuntime[DefinitiveLocalInputRuntime::Version] = 1;
    localRuntime[DefinitiveLocalInputRuntime::Phase] = static_cast<std::uint32_t>(DefinitiveLocalInputRuntime::State::Disabled);
    localRuntime[DefinitiveLocalInputRuntime::OwnerThread] = GetCurrentThreadId();
    localRuntime[DefinitiveLocalInputRuntime::CleanupComplete] = 1;
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    frontendBackend = LoadLibraryExW((std::filesystem::path(path).parent_path() / L"game_de.dll").c_str(), nullptr,
                                     LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    return frontendBackend ? &runtimeOwner : nullptr;
}
extern "C" __declspec(dllexport) DefinitiveFrontendRuntime::Status MTA_ReadDEFrontendRuntime(void* handle, std::uint32_t* output, std::uint32_t count)
{
    Call(45);
    if (runtimeViewActive)
        Call(52);
    if (handle != &runtimeOwner || !output || count != DefinitiveFrontendRuntime::FieldCount)
        return DefinitiveFrontend::Status::InvalidArgument;
    std::copy(runtimeOwner.begin(), runtimeOwner.end(), output);
    return DefinitiveFrontend::Status::Ok;
}
extern "C" __declspec(dllexport) DefinitiveFrontendRuntime::Status MTA_PulseDEFrontendRuntime(void* handle)
{
    Call(46);
    if (handle != &runtimeOwner)
        return DefinitiveFrontend::Status::InvalidArgument;
    ++runtimeOwner[DefinitiveFrontendStartup::Pulses];
    return DefinitiveFrontend::Status::Ok;
}

namespace
{
    void LocalRetired(bool disabled)
    {
        namespace L = DefinitiveLocalInputRuntime;
        if (localRuntime[L::NativeAttempted])
            ++localRuntime[L::NativeEndAttempts];
        localRuntime[L::Phase] = static_cast<std::uint32_t>(disabled ? L::State::Disabled : L::State::Suspended);
        for (const auto field : {L::NativeAttempted, L::NativeActive, L::NativeBound, L::CleanupPending, L::HeldActionsLow, L::HeldActionsHigh})
            localRuntime[field] = 0;
        localRuntime[L::NativeRevoked] = localRuntime[L::NativeGenerationLow] ? 1u : 0u;
        if (localRuntime[L::NativeGenerationLow] && !localRuntime[L::NativeReason])
            localRuntime[L::NativeReason] = 5;
        localRuntime[L::CanonicalNeutral] = localRuntime[L::CleanupComplete] = 1;
        localRuntime[L::Enabled] = 0;
        if (disabled)
            localRuntime[L::Configured] = localRuntime[L::BindingsBorrowed] = 0;
    }
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DELocalInputRuntimeApiVersion()
{
    Call(53);
    return Setting(L"MTA_CORE_FIXTURE_LOCAL_RUNTIME_MODE") == 6 ? 2u : 1u;
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ConfigureDELocalInputRuntime(void*                                         handle,
                                                                                             const DefinitiveLocalInputRuntime::RequestV1* request)
{
    namespace L = DefinitiveLocalInputRuntime;
    Call(54);
    if (handle != &runtimeOwner || !request || request->size != sizeof(*request) || request->version != 1 || request->reserved[0] || request->reserved[1] ||
        runtimeViewActive || !runtimeOwner[DefinitiveFrontendRuntime::ViewsRetired])
    {
        Call(59);
        return DefinitiveFrontend::Status::InvalidArgument;
    }
    // The controlled provider transports an opaque HWND only. Its copied
    // values exercise worker ownership; it never admits real native input.
    if (Setting(L"MTA_CORE_FIXTURE_LOCAL_RUNTIME_MODE") == 7)
        return DefinitiveFrontend::Status::Failed;
    localRuntime[L::Phase] = static_cast<std::uint32_t>(L::State::Arming);
    for (const auto field :
         {L::Configured, L::Enabled, L::CanonicalNeutral, L::BindingsBorrowed, L::CleanupComplete, L::PhysicalRearmReady, L::Focused, L::ContextEligible})
        localRuntime[field] = 1;
    localRuntime[L::ContextKnownMask] = localRuntime[L::ContextFlags] = 15;
    localRuntime[L::SupportedBindings] = localRuntime[L::EligibleKeys] = 6;
    localRuntime[L::LastFailure] = 0;
    return DefinitiveFrontend::Status::Ok;
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_PulseDELocalInputRuntime(void* handle)
{
    namespace L = DefinitiveLocalInputRuntime;
    Call(55);
    if (handle != &runtimeOwner || runtimeViewActive)
    {
        Call(59);
        return DefinitiveFrontend::Status::InvalidArgument;
    }
    if (!localRuntime[L::Configured] || !localRuntime[L::Enabled])
        return DefinitiveFrontend::Status::Ok;
    const auto focusGate = Setting(L"MTA_CORE_FIXTURE_LOCAL_RUNTIME_MODE") == 4 ? Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE) : nullptr;
    const bool focusLost = focusGate && WaitForSingleObject(focusGate, 0) == WAIT_OBJECT_0;
    if (focusGate)
        CloseHandle(focusGate);
    if (focusLost)
    {
        LocalRetired(false);
        localRuntime[L::Focused] = 0;
        localRuntime[L::NativeReason] = 3;
        localRuntime[L::LastFailure] = static_cast<std::uint32_t>(L::Failure::FocusLost);
        return DefinitiveFrontend::Status::Ok;
    }
    localRuntime[L::Phase] = static_cast<std::uint32_t>(L::State::Active);
    for (const auto field : {L::NativeAttempted, L::NativeActive, L::NativeBound})
        localRuntime[field] = 1;
    localRuntime[L::NativeGenerationLow] = static_cast<std::uint32_t>(counters ? counters[54] : 1) + 100;
    localRuntime[L::NativeRevoked] = localRuntime[L::NativeReason] = 0;
    localRuntime[L::NativeLifetimeMs] = 500;
    localRuntime[L::CleanupComplete] = 0;
    localRuntime[L::CanonicalNeutral] = 0;
    localRuntime[L::HeldActionsLow] = 1;
    ++localRuntime[L::RenewalsLow];
    return DefinitiveFrontend::Status::Ok;
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDELocalInputRuntime(void* handle, std::uint32_t* output, std::uint32_t count)
{
    Call(56);
    if (handle != &runtimeOwner || !output || count != DefinitiveLocalInputRuntime::FieldCount || runtimeViewActive)
    {
        Call(59);
        return DefinitiveFrontend::Status::InvalidArgument;
    }
    std::copy(localRuntime.begin(), localRuntime.end(), output);
    return DefinitiveFrontend::Status::Ok;
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_SuspendDELocalInputRuntime(void* handle)
{
    namespace L = DefinitiveLocalInputRuntime;
    Call(57);
    if (handle != &runtimeOwner || runtimeViewActive)
    {
        Call(59);
        return DefinitiveFrontend::Status::InvalidArgument;
    }
    const auto mode = Setting(L"MTA_CORE_FIXTURE_LOCAL_RUNTIME_MODE");
    const auto gate = localRuntime[L::NativeAttempted] && (mode == 2 || mode == 3) ? Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE) : nullptr;
    const bool pending = gate && WaitForSingleObject(gate, 0) == WAIT_TIMEOUT;
    if (gate)
        CloseHandle(gate);
    if (pending)
    {
        Call(60);
        localRuntime[L::Phase] = static_cast<std::uint32_t>(L::State::RetainedCleanup);
        localRuntime[L::NativeActive] = 0;
        localRuntime[L::CleanupPending] = 1;
        localRuntime[L::CleanupComplete] = 0;
        return DefinitiveFrontend::Status::Busy;
    }
    if (localRuntime[L::Configured])
        LocalRetired(false);
    return DefinitiveFrontend::Status::Ok;
}
#ifndef CORE_FIXTURE_MISSING_LOCAL_RUNTIME_EXPORT
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_DisableDELocalInputRuntime(void* handle)
{
    namespace L = DefinitiveLocalInputRuntime;
    Call(58);
    if (handle != &runtimeOwner || runtimeViewActive)
    {
        Call(59);
        return DefinitiveFrontend::Status::InvalidArgument;
    }
    const auto mode = Setting(L"MTA_CORE_FIXTURE_LOCAL_RUNTIME_MODE");
    const auto gate = localRuntime[L::NativeAttempted] && (mode == 2 || mode == 3) ? Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE) : nullptr;
    const bool pending = gate && WaitForSingleObject(gate, 0) == WAIT_TIMEOUT;
    if (gate)
        CloseHandle(gate);
    if (pending)
    {
        Call(60);
        localRuntime[L::Phase] = static_cast<std::uint32_t>(L::State::RetainedCleanup);
        localRuntime[L::NativeActive] = 0;
        localRuntime[L::CleanupPending] = 1;
        localRuntime[L::CleanupComplete] = 0;
        return DefinitiveFrontend::Status::Busy;
    }
    LocalRetired(true);
    return DefinitiveFrontend::Status::Ok;
}
#endif

extern "C"
    __declspec(dllexport) DefinitiveFrontendRuntime::Status MTA_RunDEFrontendRuntimeView(void* handle, const DefinitivePersistentFrontend::RequestV1* request)
{
    namespace C = DefinitiveFrontendRuntime;
    namespace S = DefinitiveFrontendStartup;
    namespace P = DefinitivePersistentFrontend;
    Call(47);
    if (localRuntime[DefinitiveLocalInputRuntime::NativeAttempted] || localRuntime[DefinitiveLocalInputRuntime::NativeBound] ||
        localRuntime[DefinitiveLocalInputRuntime::NativeActive] || localRuntime[DefinitiveLocalInputRuntime::CleanupPending] ||
        (!localRuntime[DefinitiveLocalInputRuntime::CanonicalNeutral] && localRuntime[DefinitiveLocalInputRuntime::Configured]))
    {
        Call(59);
        return DefinitiveFrontend::Status::Failed;
    }
    const auto mode = Setting(L"MTA_CORE_FIXTURE_RUNTIME_MODE");
    if (handle != &runtimeOwner || !request || !request->cancelled || !request->publish || !request->heartbeat ||
        request->generation != runtimeOwner[C::ViewGenerationLow] + 1)
        return DefinitiveFrontend::Status::InvalidArgument;
    if (mode == 9)
    {
        runtimeOwner[S::LastFailure] = static_cast<std::uint32_t>(S::Failure::InputBind);
        return DefinitiveFrontend::Status::Failed;
    }
    if (mode == 16)
        return DefinitiveFrontend::Status::Busy;
    if (mode == 17)
    {
        // Rejected publication is not absence of publication. An unchanged
        // InputBind refusal cannot erase this uncertain callback lifetime.
        P::Snapshot malformed{};
        malformed[0] = malformed[P::PersistentVersion] = 1;
        malformed[P::SessionGenerationLow] = static_cast<std::uint32_t>(request->generation) + 1;
        Call(50);
        request->publish(malformed.data(), P::FieldCount);
        runtimeOwner[S::LastFailure] = static_cast<std::uint32_t>(S::Failure::InputBind);
        return DefinitiveFrontend::Status::Failed;
    }
    runtimeViewActive = true;
    ++runtimeOwner[C::ViewsAttached];
    ++runtimeOwner[S::ViewRuns];
    runtimeOwner[C::ViewGenerationLow] = static_cast<std::uint32_t>(request->generation);
    runtimeOwner[C::RuntimePhase] = 3;
    runtimeOwner[C::ViewBorrowed] = 1;
    runtimeOwner[C::HiddenNeutralVerified] = runtimeOwner[S::LastViewCleanupComplete] = 0;
    runtimeOwner[S::ExactImageBound] = 1;
    runtimeOwner[S::CapabilitiesLow] = 97;
    HMODULE    retained = nullptr;
    const auto factory = frontendBackend ? GetProcAddress(frontendBackend, "CreateGameAdapter") : nullptr;
    const bool pinned =
        factory && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(factory), &retained);
    P::Snapshot sample{};
    sample[0] = sample[1] = 1;
    // Match the real GUI lifecycle: a live view owns eventual cleanup debt
    // even after its short native render observer has already retired.
    sample[5] = 1;
    sample[57] = sample[232] = pinned ? 1u : 0u;
    sample[68] = sample[70] = 1;
    sample[96] = 1;
    sample[97] = 2;
    sample[99] = sample[100] = 1;
    sample[128] = sample[131] = sample[160] = sample[161] = sample[162] = sample[176] = 1;
    sample[208] = sample[210] = sample[212] = 1;
    sample[214] = sample[216] = sample[220] = 2;
    sample[236] = sample[237] = sample[243] = 1;
    sample[P::PersistentVersion] = 1;
    sample[P::SessionGenerationLow] = sample[P::NativeLeaseGenerationLow] = static_cast<std::uint32_t>(request->generation);
    sample[P::HostLeaseFresh] = sample[P::NativeLeaseActive] = 1;
    sample[P::HostHeartbeatBudgetMs] = 1000;
    sample[P::NativeHeartbeatBudgetMs] = 500;
    const auto publish = [&]
    {
        ++sample[2];
        ++sample[3];
        Call(50);
        request->publish(sample.data(), P::FieldCount);
    };
    // NativeVerified alone must not acknowledge a partial cursor Begin/renew
    // failure: these controlled reports lack actual focused GUI admission.
    if (mode == 2 || mode == 11 || mode == 12)
    {
        sample[162] = 0;
        sample[68] = sample[70] = 0;
    }
    if (mode == 11 || mode == 12)
        sample[172] = 1;
    if (mode == 18)
        sample[5] = 0;
    if (mode == 19)
        sample[6] = 1;
    publish();
    const auto started = GetTickCount64();
    auto       reason = P::Stop::Cancelled;
    // Hold these otherwise fully admitted samples until the test has observed
    // their sole invalid ownership word. A timer could skip the actual guard
    // boundary on a busy host and test only the later terminal publication.
    const auto ownershipRelease = mode == 18 || mode == 19 ? Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE) : nullptr;
    for (;;)
    {
        if (!request->heartbeat())
        {
            reason = P::Stop::HostLeaseExpired;
            break;
        }
        if (request->cancelled())
            break;
        if (ownershipRelease && WaitForSingleObject(ownershipRelease, 0) == WAIT_OBJECT_0)
        {
            reason = P::Stop::Fault;
            break;
        }
        if (mode == 6 || mode == 7 || mode == 10 || mode == 11 || mode == 12)
        {
            if (GetTickCount64() - started >= 100)
            {
                reason = mode == 6 ? P::Stop::FocusLost : mode == 7 ? P::Stop::GuiClosed : P::Stop::Fault;
                break;
            }
        }
        Sleep(10);
        publish();
    }
    if (ownershipRelease)
        CloseHandle(ownershipRelease);
    if (mode == 13)
    {
        sample[1] = 3;
        sample[5] = 1;
        publish();
        auto gate = Event(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", SYNCHRONIZE);
        if (gate)
        {
            WaitForSingleObject(gate, 15000);
            CloseHandle(gate);
        }
    }
    sample[1] = 2;
    sample[5] = 0;
    sample[6] = sample[9] = sample[39] = 1;
    sample[78] = sample[79] = sample[80] = 1;
    sample[97] = 4;
    sample[100] = 0;
    sample[113] = 1;
    sample[129] = sample[177] = 4;
    sample[149] = sample[190] = 1;
    sample[162] = sample[208] = sample[210] = sample[243] = 0;
    sample[171] = sample[238] = 1;
    sample[P::NativeLeaseActive] = 0;
    sample[P::NativeLeaseRevoked] = 1;
    sample[P::NativeLeaseReason] = 5;
    sample[P::StopReason] = static_cast<std::uint32_t>(reason);
    publish();
    Call(51);
    runtimeViewActive = false;
    runtimeOwner[C::RuntimePhase] = 2;
    runtimeOwner[C::ViewBorrowed] = 0;
    runtimeOwner[C::HiddenNeutralVerified] = runtimeOwner[S::LastViewCleanupComplete] = 1;
    ++runtimeOwner[C::ViewsRetired];
    ++runtimeOwner[S::Pulses];
    return reason == P::Stop::Fault ? DefinitiveFrontend::Status::Failed : DefinitiveFrontend::Status::Stopped;
}
extern "C" __declspec(dllexport) DefinitiveFrontendRuntime::Status MTA_ShutdownDEFrontendRuntime(void* handle)
{
    namespace C = DefinitiveFrontendRuntime;
    namespace S = DefinitiveFrontendStartup;
    Call(48);
    if (handle != &runtimeOwner || runtimeViewActive)
        return DefinitiveFrontend::Status::Busy;
    if (Setting(L"MTA_CORE_FIXTURE_LOCAL_RUNTIME_MODE") &&
        (localRuntime[DefinitiveLocalInputRuntime::NativeAttempted] || localRuntime[DefinitiveLocalInputRuntime::NativeBound] ||
         localRuntime[DefinitiveLocalInputRuntime::NativeActive] || localRuntime[DefinitiveLocalInputRuntime::CleanupPending] ||
         localRuntime[DefinitiveLocalInputRuntime::Configured] || localRuntime[DefinitiveLocalInputRuntime::BindingsBorrowed] ||
         !localRuntime[DefinitiveLocalInputRuntime::CleanupComplete] || !localRuntime[DefinitiveLocalInputRuntime::CanonicalNeutral] || !counters[58]))
    {
        Call(59);
        return DefinitiveFrontend::Status::Busy;
    }
    if (Setting(L"MTA_CORE_FIXTURE_RUNTIME_MODE") == 14 && counters && counters[48] < 4)
        return DefinitiveFrontend::Status::Busy;
    runtimeOwner[S::Phase] = 4;
    runtimeOwner[S::BackendStarted] = runtimeOwner[S::ExactImageBound] = runtimeOwner[S::CapabilitiesLow] = 0;
    runtimeOwner[S::CleanupComplete] = 1;
    runtimeOwner[C::RuntimePhase] = 5;
    runtimeOwner[C::ServicesRetained] = runtimeOwner[C::XmlRetained] = runtimeOwner[C::ConfigRetained] = runtimeOwner[C::CVarsLoaded] =
        runtimeOwner[C::BindingsRetained] = 0;
    runtimeOwner[C::CleanupComplete] = 1;
    if (frontendBackend)
    {
        FreeLibrary(frontendBackend);
        frontendBackend = nullptr;
    }
    return DefinitiveFrontend::Status::Ok;
}
extern "C" __declspec(dllexport) DefinitiveFrontendRuntime::Status MTA_ReleaseDEFrontendRuntime(void* handle)
{
    Call(49);
    return handle == &runtimeOwner ? DefinitiveFrontend::Status::Ok : DefinitiveFrontend::Status::InvalidArgument;
}

extern "C" __declspec(dllexport) void* MTA_BeginDEFrontendStartup(std::uint32_t version)
{
    namespace Startup = DefinitiveFrontendStartup;
    Call(33);
    if (version != Startup::ApiVersion || !mtaSet || !gtaSet)
        return nullptr;
    frontendOwner = {};
    frontendOwner[Startup::Version] = Startup::ApiVersion;
    frontendOwner[Startup::Phase] = 2;
    frontendOwner[Startup::OwnerThread] = GetCurrentThreadId();
    frontendOwner[Startup::CoreRetained] = frontendOwner[Startup::BackendStartAttempted] = frontendOwner[Startup::BackendStarted] = 1;
    frontendOwner[Startup::Pulses] = frontendOwner[Startup::LastViewCleanupComplete] = 1;
    frontendOwner[Startup::CapabilitiesLow] = 33;
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    frontendBackend = LoadLibraryExW((std::filesystem::path(path).parent_path() / L"game_de.dll").c_str(), nullptr,
                                     LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    return frontendBackend ? &frontendOwner : nullptr;
}

extern "C" __declspec(dllexport) DefinitiveFrontendStartup::Status MTA_ReadDEFrontendStartup(void* handle, std::uint32_t* output, std::uint32_t count)
{
    namespace Startup = DefinitiveFrontendStartup;
    Call(36);
    if (handle != &frontendOwner || !output || count != Startup::FieldCount)
        return Startup::Status::InvalidArgument;
    if (Setting(L"MTA_CORE_FIXTURE_FRONTEND_CLOCK_MODE") == 22 && frontendClockReturned && !frontendOwnerReadThrew)
    {
        frontendOwnerReadThrew = true;
        throw std::runtime_error("controlled completed owner read exception after clock retirement");
    }
    if (counters && counters[34] && !frontendClockReturned)
        Call(40);
    std::copy(frontendOwner.begin(), frontendOwner.end(), output);
    return Startup::Status::Ok;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEFrontendClockReferenceApiVersion()
{
    return Setting(L"MTA_CORE_FIXTURE_FRONTEND_CLOCK_MODE") == 2 ? 2u : DefinitiveFrontendClockReference::ApiVersion;
}

#ifndef CORE_FIXTURE_MISSING_FRONTEND_CLOCK_EXPORT
extern "C" __declspec(dllexport) DefinitiveFrontendClockReference::Status MTA_RunDEFrontendClockReference(void* handle, std::uint32_t duration,
                                                                                                          DefinitiveInputReference::CancelCallback  cancelled,
                                                                                                          DefinitiveInputReference::PublishCallback publish)
{
    namespace Startup = DefinitiveFrontendStartup;
    using Status = Startup::Status;
    const auto mode = Setting(L"MTA_CORE_FIXTURE_FRONTEND_CLOCK_MODE");
    Call(34);
    if (handle != &frontendOwner || duration != 2000 || !cancelled || !publish)
        return Status::InvalidArgument;
    if (mode == 3)
    {
        frontendOwner[Startup::LastFailure] = static_cast<std::uint32_t>(Startup::Failure::InputBind);
        frontendClockReturned = true;
        return Status::Failed;
    }
    if (mode == 14 || mode == 15 || mode == 16)
    {
        Call(39);
        Call(25);
        if (mode == 16)
            throw std::runtime_error("controlled missing completed clock report exception");
        return mode == 14 ? Status::Busy : Status::Failed;
    }
    frontendOwner[Startup::ExactImageBound] = 1;
    frontendOwner[Startup::CapabilitiesLow] |= static_cast<std::uint32_t>(mta::game_de::NativePadBindingCapabilities);
    ++frontendOwner[Startup::Pulses];
    inputRequest = {duration, cancelled, publish};
    if (mode == 19)
    {
        namespace Clock = DefinitiveClockReference;
        Clock::Snapshot sample{};
        sample[Clock::Version] = Clock::ApiVersion;
        sample[Clock::Phase] = 2;
        sample[Clock::Sequence] = sample[Clock::CleanupComplete] = 1;
        sample[Clock::Reason] = static_cast<std::uint32_t>(Clock::StopReason::Cancelled);
        publish(sample.data(), Clock::FieldCount);
        frontendClockReturned = true;
        return Status::Stopped;
    }
    ControlledClock(true);
    Call(39);
    if (mode == 7 || mode == 8 || mode == 9 || mode == 10 || mode == 18)
    {
        Call(25);
        if (mode == 8)
            throw std::runtime_error("controlled completed clock returned pending exception");
        return Status::Ok;
    }
    frontendClockReturned = true;
    if (mode == 6)
        frontendOwner[Startup::CapabilitiesLow] |= static_cast<std::uint32_t>(mta::game_de::AdapterCapability::NativeDeviceStateCandidateRead);
    if (mode == 23)
        frontendOwner[Startup::CapabilitiesLow] &= ~static_cast<std::uint32_t>(mta::game_de::NativePadBindingCapabilities);
    if (mode == 12)
        throw std::runtime_error("controlled completed clock exception after retirement");
    return cancelled() ? Status::Stopped : Status::Ok;
}
#endif

extern "C" __declspec(dllexport) DefinitiveFrontendStartup::Status MTA_RunDEFrontendStartupView(void* handle, std::uint32_t duration,
                                                                                                DefinitiveInputReference::CancelCallback  cancelled,
                                                                                                DefinitiveInputReference::PublishCallback publish)
{
    namespace Startup = DefinitiveFrontendStartup;
    namespace Gui = DefinitiveGuiStartupReference;
    namespace Input = DefinitiveGuiFrontendInputReference;
    namespace Surface = DefinitiveGuiSurfaceReference;
    Call(35);
    if (handle != &frontendOwner || duration != Input::MaximumDurationMs || !cancelled || !publish || !frontendClockReturned)
        return Startup::Status::InvalidArgument;
    Input::Snapshot sample{};
    sample[Gui::Version] = Input::ApiVersion;
    sample[Gui::Phase] = 2;
    sample[Gui::Sequence] = 1;
    sample[Gui::CleanupComplete] = sample[Gui::ObserverEnded] = sample[Gui::GuiCleanupComplete] = 1;
    sample[Surface::WindowCleanupComplete] = sample[Surface::CompositionCleanupComplete] = sample[Surface::ClassUnregistered] = 1;
    sample[Input::NativeEnded] = 1;
    sample[Gui::BackendPinnedExpected] = 1;
    sample[Gui::WorkloadComplete] = sample[Gui::WorkloadSucceeded] = Setting(L"MTA_CORE_FIXTURE_FRONTEND_CLOCK_MODE") == 21 ? 0u : 1u;
    publish(sample.data(), Input::FieldCount);
    frontendOwner[Startup::ViewRuns] = frontendOwner[Startup::LastViewCleanupComplete] = 1;
    if (Setting(L"MTA_CORE_FIXTURE_FRONTEND_CLOCK_MODE") == 24)
        frontendOwner[Startup::CapabilitiesLow] |= static_cast<std::uint32_t>(mta::game_de::AdapterCapability::NativeDeviceStateCandidateRead);
    ++frontendOwner[Startup::Pulses];
    return sample[Gui::WorkloadSucceeded] ? Startup::Status::Ok : Startup::Status::Failed;
}

extern "C" __declspec(dllexport) DefinitiveFrontendStartup::Status MTA_ShutdownDEFrontendStartup(void* handle)
{
    namespace Startup = DefinitiveFrontendStartup;
    Call(37);
    if (handle != &frontendOwner)
        return Startup::Status::InvalidArgument;
    frontendOwner[Startup::Phase] = 4;
    frontendOwner[Startup::BackendStarted] = frontendOwner[Startup::ExactImageBound] = 0;
    frontendOwner[Startup::CapabilitiesLow] = frontendOwner[Startup::CapabilitiesHigh] = 0;
    frontendOwner[Startup::CleanupComplete] = 1;
    if (frontendBackend)
    {
        FreeLibrary(frontendBackend);
        frontendBackend = nullptr;
    }
    return Startup::Status::Ok;
}

extern "C" __declspec(dllexport) DefinitiveFrontendStartup::Status MTA_ReleaseDEFrontendStartup(void* handle)
{
    Call(38);
    return handle == &frontendOwner ? DefinitiveFrontendStartup::Status::Ok : DefinitiveFrontendStartup::Status::InvalidArgument;
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
    if ((clockRequest && Setting(L"MTA_CORE_FIXTURE_CLOCK_MODE") == 12) || (clockSetRequest && Setting(L"MTA_CORE_FIXTURE_CLOCK_SET_MODE") == 12) ||
        (applicationFrameRequest && Setting(L"MTA_CORE_FIXTURE_APPLICATION_FRAME_MODE") == 12) ||
        (startupObservationRequest && Setting(L"MTA_CORE_FIXTURE_STARTUP_OBSERVATION_MODE") == 12))
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
    if (mtaSet && gtaSet && startupObservationRequest)
    {
        ControlledStartupObservation();
        const auto mode = Setting(L"MTA_CORE_FIXTURE_STARTUP_OBSERVATION_MODE");
        Call(24);
        if (mode == 8 || mode == 12)
            Call(25);
        if (mode == 5 || mode == 8)
            throw std::runtime_error("controlled startup-observation Initialize exception");
        return mode == 4 || mode == 7 ? 0 : 5;
    }
    if (mtaSet && gtaSet && applicationFrameRequest)
    {
        ControlledApplicationFrame();
        const auto mode = Setting(L"MTA_CORE_FIXTURE_APPLICATION_FRAME_MODE");
        Call(24);
        if (mode == 8 || mode == 12)
            Call(25);
        if (mode == 5 || mode == 8)
            throw std::runtime_error("controlled application-frame Initialize exception");
        return mode == 4 || mode == 7 ? 0 : 5;
    }
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

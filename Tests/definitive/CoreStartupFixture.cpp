/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

// A controlled DLL supplies loader-lock delay and thread-affinity observations.
// It contains no MTA/Game DE implementation and never touches game memory.
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include "../../Client/core/DefinitiveInputReference.h"
#include "../../Client/core/DefinitiveMovementReference.h"
#include "../../Client/core/DefinitiveRenderReference.h"
#include <filesystem>

namespace
{
    HANDLE                            mapping = nullptr;
    volatile LONG*                    counters = nullptr;
    bool                              mtaSet = false, gtaSet = false;
    DefinitiveInputReference::Request inputRequest;

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

template <typename SnapshotType>
void ControlledRender()
{
    using namespace DefinitiveRenderReference;
    constexpr auto count = static_cast<std::uint32_t>(std::tuple_size<SnapshotType>::value);
    SnapshotType   sample{};
    sample[Version] = count == FieldCountV3 ? 3u : count == FieldCountV2 ? 2u : 1u;
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
            sample[Width] = 1280;
            sample[Height] = 720;
        }
    }
    if constexpr (count == FieldCountV3)
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
        }
    }
    sample[CleanupPending] = sample[NativeCleanupPending] = sample[CleanupAttempts] = 1;
    ++sample[Sequence];
    inputRequest.publish(sample.data(), count);
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
    if (mtaSet && gtaSet && inputRequest.renderObservation)
    {
        if (inputRequest.renderApiVersion == 3)
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

/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Controlled protocol producer; contains no MTA or engine runtime
 *****************************************************************************/
#include "../../Client/core/DefinitiveGuiFrontendReference.h"
#include "../../Client/core/DefinitiveFrontend.h"
#include "../../Client/core/DefinitiveInputReference.h"
#include "../../Client/core/DefinitiveGuiFrontendInputReference.h"
#include "../../Client/core/DefinitiveFrontendStartup.h"
#include <windows.h>
#include <algorithm>
#include <cwchar>
#include <cstdio>
#include <stdexcept>

namespace
{
    bool                                            mta = false, gta = false;
    bool                                            coreOwnedInput = false;
    DefinitiveInputReference::CancelCallback        cancel = nullptr;
    DefinitiveInputReference::PublishCallback       publish = nullptr;
    DefinitiveGuiFrontendReference::ControlCallback control = nullptr;
    DefinitiveFrontendStartup::Snapshot             startup{};
    int                                             startupToken = 0;
    unsigned                                        startupShutdowns = 0, startupReleases = 0, startupReads = 0;
    unsigned                                        Mode()
    {
        wchar_t value[32]{};
        GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_MODE", value, 32);
        return static_cast<unsigned>(std::wcstoul(value, nullptr, 10));
    }
    void ControlledFrontendInput()
    {
        using namespace DefinitiveGuiStartupReference;
        using namespace DefinitiveGuiFrontendInputReference;
        DefinitiveGuiFrontendInputReference::Snapshot sample{};
        sample[Version] = sample[Phase] = sample[Sequence] = 1;
        sample[ObserverEnded] = 1;
        sample[Frontend] = 1;
        sample[Frontend + 1] = 2;
        sample[Frontend + 19] = sample[Frontend + 20] = sample[Frontend + 21] = 1;
        sample[Console] = 1;
        sample[Console + 21] = sample[Console + 24] = 1;
        sample[Console + 29] = 255;
        sample[Bindings] = 1;
        sample[Bindings + 1] = 2;
        sample[Bindings + 3] = sample[Bindings + 4] = 1;
        sample[Bindings + 10] = sample[Bindings + 12] = sample[Bindings + 13] = sample[Bindings + 15] = sample[Bindings + 16] = 1;
        sample[Bindings + 11] = 2;
        sample[InteractionVersion] = sample[InteractionEnabled] = sample[InteractionFocused] = 1;
        sample[NativeMetrics] = sample[NativeMetrics + 2] = sample[NativeModulePinned] = sample[NativeStarted] = sample[NativeVerified] = 1;
        sample[NativeMetrics + 6] = sample[NativeMetrics + 12] = 7;
        sample[NativeMetrics + 7] = sample[NativeMetrics + 13] = 2;
        sample[NativeMetrics + 8] = 3;
        sample[NativeOwnerRetained] = 1;
        publish(sample.data(), static_cast<std::uint32_t>(sample.size()));
        wchar_t releaseName[256]{};
        GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_RELEASE", releaseName, 256);
        const auto release = OpenEventW(SYNCHRONIZE, FALSE, releaseName);
        const auto started = GetTickCount64();
        while (release && WaitForSingleObject(release, 2) == WAIT_TIMEOUT && !cancel() && GetTickCount64() - started < 10000)
        {
        }
        if (release)
            CloseHandle(release);
        sample[Phase] = 2;
        ++sample[Sequence];
        sample[CleanupComplete] = sample[GuiCleanupComplete] = 1;
        sample[DefinitiveGuiSurfaceReference::WindowCleanupComplete] = sample[DefinitiveGuiSurfaceReference::CompositionCleanupComplete] =
            sample[DefinitiveGuiSurfaceReference::ClassUnregistered] = 1;
        sample[Frontend + 1] = 4;
        sample[Frontend + 17] = 1;
        sample[Frontend + 19] = sample[Frontend + 20] = sample[Frontend + 21] = 0;
        sample[Bindings + 1] = 4;
        sample[Bindings + 10] = sample[Bindings + 15] = 0;
        sample[Bindings + 14] = 1;
        sample[InteractionFocused] = sample[NativeMetrics] = sample[NativeMetrics + 2] = sample[NativeOwnerRetained] = 0;
        sample[InputDetached] = sample[NativeEnded] = 1;
        if (Mode() >= 200)
        {
            sample[WorkloadComplete] = 1;
            sample[WorkloadSucceeded] = Mode() != 205 && Mode() != 209 && Mode() != 217 && Mode() != 218;
        }
        if (Mode() == 210)
            sample[NativeOwnerRetained] = 1;
        if (Mode() == 218)
            sample[CleanupDeadlineExceeded] = 1;
        if (Mode() == 103)
            sample[NativeEnded] = 0;
        if (Mode() == 104)
            sample[NativeMetrics + 2] = 1;
        if (Mode() == 105)
            sample[NativeMetrics + 12] = 6;
        if (Mode() == 106)
            sample[NativeMetrics + 14] = 1;
        if (Mode() == 107)
            sample[Bindings + 15] = 1;
        if (Mode() == 108)
            sample[Frontend + 19] = 1;
        if (Mode() == 109)
            sample[NativeOwnerRetained] = 1;
        if (Mode() == 110)
            sample[CleanupDeadlineExceeded] = 1;
        publish(sample.data(), static_cast<std::uint32_t>(sample.size()));
    }
    void ControlledFrontend()
    {
        using namespace DefinitiveGuiStartupReference;
        using namespace DefinitiveGuiFrontendReference;
        DefinitiveGuiFrontendReference::Snapshot sample{};
        sample[DefinitiveGuiStartupReference::Version] = 1;
        sample[Phase] = sample[Sequence] = 1;
        sample[ObserverEnded] = 1;  // Offer controls only after observation ended.
        ControlSnapshot lease{};
        const auto      update = [&](ControlAction action)
        {
            const bool result = control(static_cast<std::uint32_t>(action), lease.data(), static_cast<std::uint32_t>(lease.size()));
            if (result)
                std::copy(lease.begin(), lease.end(), sample.begin() + ControlGeneration);
            ++sample[Sequence];
            publish(sample.data(), static_cast<std::uint32_t>(sample.size()));
            return result;
        };
        if (Mode() != 3)
        {
            if (!update(ControlAction::Request))
                return;
            const auto begin = GetTickCount64();
            while (!lease[ControlField::Acknowledged] && !lease[ControlField::Revoked] && GetTickCount64() - begin < 10000)
            {
                update(ControlAction::Poll);
                Sleep(2);
            }
            if (lease[ControlField::Acknowledged])
            {
                sample[FrontendStarted] = 1;
                sample[Frontend + DefinitiveFrontend::Version] = DefinitiveFrontend::ApiVersion;
                sample[Frontend + DefinitiveFrontend::Phase] = 2;
                sample[Frontend + DefinitiveFrontend::ServicesRetained] = sample[Frontend + DefinitiveFrontend::GuiBorrowed] = 1;
                sample[DefinitiveGuiSurfaceReference::ChildCreated] = sample[InteractionEnabled] = 1;
                sample[InputEventsLow] = 7;
                sample[InputEventsHigh] = 2;
            }
            wchar_t name[256]{};
            GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_RELEASE", name, 256);
            const auto release = OpenEventW(SYNCHRONIZE, FALSE, name);
            const auto activeAt = GetTickCount64();
            while (release && WaitForSingleObject(release, 2) == WAIT_TIMEOUT && !cancel() && GetTickCount64() - activeAt < 10000)
                update(ControlAction::Poll);
            if (release)
                CloseHandle(release);
            // Quiesce active publications before the host contends on cleanup.
            // Otherwise a legitimate Phase1 publication could already be
            // waiting on that lock and never reach the detachment callback.
            wchar_t detachReadyName[256]{}, detachGateName[256]{};
            GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_DETACH_READY", detachReadyName, 256);
            GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_DETACH_GATE", detachGateName, 256);
            const auto detachReady = OpenEventW(EVENT_MODIFY_STATE, FALSE, detachReadyName);
            const auto detachGate = OpenEventW(SYNCHRONIZE, FALSE, detachGateName);
            if (detachReady)
            {
                SetEvent(detachReady);
                CloseHandle(detachReady);
            }
            if (detachGate)
            {
                WaitForSingleObject(detachGate, 10000);
                CloseHandle(detachGate);
            }
            sample[Phase] = 3;
            sample[CleanupPending] = 1;
            sample[InputDetached] = 1;
            if (sample[FrontendStarted])
            {
                sample[FrontendStopped] = 1;
                sample[Frontend + DefinitiveFrontend::Phase] = 4;
                sample[Frontend + DefinitiveFrontend::ServicesRetained] = sample[Frontend + DefinitiveFrontend::GuiBorrowed] = 0;
                sample[Frontend + DefinitiveFrontend::CleanupComplete] = 1;
            }
            update(ControlAction::InputDetached);
            const auto detachedAt = GetTickCount64();
            while (!lease[ControlField::Restored] && Mode() != 6 && GetTickCount64() - detachedAt < 10000)
            {
                update(ControlAction::Poll);
                Sleep(2);
            }
            if (Mode() == 8)
            {
                // Let the host contend precisely after this producer observed
                // restoration, so only the worker's independent final read waits.
                wchar_t readyName[256]{}, finalName[256]{};
                GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_FINAL_READY", readyName, 256);
                GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_FINAL_RELEASE", finalName, 256);
                const auto ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, readyName);
                const auto final = OpenEventW(SYNCHRONIZE, FALSE, finalName);
                if (ready)
                {
                    SetEvent(ready);
                    CloseHandle(ready);
                }
                if (final)
                {
                    WaitForSingleObject(final, 10000);
                    CloseHandle(final);
                }
            }
        }
        sample[Phase] = 2;
        sample[CleanupPending] = 0;
        sample[CleanupComplete] = sample[GuiCleanupComplete] = 1;
        sample[DefinitiveGuiSurfaceReference::WindowCleanupComplete] = 1;
        sample[DefinitiveGuiSurfaceReference::CompositionCleanupComplete] = 1;
        sample[DefinitiveGuiSurfaceReference::ClassUnregistered] = 1;
        if (Mode() == 5)
            sample[Frontend + DefinitiveFrontend::ServicesRetained] = 1;
        if (Mode() == 7)
            sample[PressedButtons] = 1;
        if (Mode() == 9)
            sample[CleanupDeadlineExceeded] = 1;  // Historical failure, ownership actually retired.
        if (Mode() == 10)
            sample[Frontend + DefinitiveFrontend::Phase] = 2;
        if (Mode() == 11)
            sample[Frontend + DefinitiveFrontend::InputRequested] = 1;
        if (Mode() == 12)
            sample[Frontend + DefinitiveFrontend::ReleaseFault] = 1;
        ++sample[Sequence];
        publish(sample.data(), static_cast<std::uint32_t>(sample.size()));
    }
    void LogStartup(char operation)
    {
        wchar_t path[32768]{};
        if (!GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_LIFECYCLE", path, 32768))
            return;
        const auto file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return;
        char       text[64]{};
        const auto count = std::snprintf(text, sizeof(text), "%c %lu\n", operation, GetCurrentThreadId());
        DWORD      written = 0;
        if (count > 0)
            WriteFile(file, text, static_cast<DWORD>(count), &written, nullptr);
        CloseHandle(file);
    }
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEStartupApiVersion()
{
    return 1;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ReadDEStartupState(std::uint32_t field)
{
    const std::uint32_t values[]{1, 5, 6, 97, 0, 0, 0, 1};
    return field < 8 ? values[field] : 0xffffffffu;
}
extern "C" __declspec(dllexport) void SetMTADirectory(const wchar_t* value, std::size_t size)
{
    mta = value && size;
}
extern "C" __declspec(dllexport) void SetGTADirectory(const wchar_t* value, std::size_t size)
{
    gta = value && size;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_DEGuiFrontendReferenceApiVersion()
{
    return Mode() == 2 ? 2u : 1u;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_DEGuiFrontendInputReferenceApiVersion()
{
    return Mode() == 101 ? 2u : 1u;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEGuiFrontendInputReference(std::uint32_t                             duration,
                                                                                        DefinitiveInputReference::CancelCallback  cancelled,
                                                                                        DefinitiveInputReference::PublishCallback publisher)
{
    if (duration != DefinitiveGuiFrontendInputReference::MaximumDurationMs || !cancelled || !publisher || Mode() == 102)
        return 0;
    cancel = cancelled;
    publish = publisher;
    coreOwnedInput = true;
    return 1;
}
extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEGuiFrontendReference(std::uint32_t version, std::uint32_t duration,
                                                                                   DefinitiveInputReference::CancelCallback        cancelled,
                                                                                   DefinitiveInputReference::PublishCallback       publisher,
                                                                                   DefinitiveGuiFrontendReference::ControlCallback controls)
{
    if (version != 1 || duration != DefinitiveGuiFrontendReference::MaximumDurationMs || !cancelled || !publisher || !controls || Mode() == 1)
        return 0;
    cancel = cancelled;
    publish = publisher;
    control = controls;
    return 1;
}
extern "C" __declspec(dllexport) int InitializeCore()
{
    if (Mode() >= 200)
        LogStartup('I');
    if (!mta)
        return 3;
    if (!gta)
        return 4;
    if (publish && coreOwnedInput)
        ControlledFrontendInput();
    else if (publish)
        ControlledFrontend();
    return 5;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEFrontendStartupApiVersion()
{
    return Mode() == 201 ? 2u : 1u;
}
// These controlled exports deliberately violate the real lifecycle's no-throw
// contract to test worker retention after unknown failures. Explicit throwing
// specifications override /EHsc's extern-C assumption only in this fixture.
extern "C" __declspec(dllexport) void* MTA_BeginDEFrontendStartup(std::uint32_t version) noexcept(false)
{
    LogStartup('B');
    if (!mta || !gta || version != 1 || Mode() == 202)
        return nullptr;
    if (Mode() == 220)
        throw std::runtime_error("controlled Begin throw with unknown ownership");
    using namespace DefinitiveFrontendStartup;
    startup = {};
    startupShutdowns = startupReleases = startupReads = 0;
    startup[Version] = 1;
    startup[Phase] = Mode() == 203 ? 5u : 2u;
    startup[OwnerThread] = GetCurrentThreadId();
    startup[CoreRetained] = startup[BackendStartAttempted] = startup[Pulses] = startup[LastViewCleanupComplete] = 1;
    startup[BackendStarted] = Mode() == 203 ? 0u : 1u;
    startup[CapabilitiesLow] = startup[BackendStarted] ? 33u : 0u;
    startup[LastFailure] = Mode() == 203 ? static_cast<std::uint32_t>(Failure::BackendStart) : 0u;
    return &startupToken;
}
extern "C" __declspec(dllexport) DefinitiveFrontendStartup::Status MTA_ReadDEFrontendStartup(void* handle, std::uint32_t* output,
                                                                                             std::uint32_t count) noexcept(false)
{
    LogStartup('R');
    using namespace DefinitiveFrontendStartup;
    if (handle != &startupToken || !output || count != FieldCount)
        return Status::InvalidArgument;
    if (startup[OwnerThread] != GetCurrentThreadId())
        return Status::WrongThread;
    ++startupReads;
    if (Mode() == 229 && startupReads == 1)
    {
        wchar_t readyName[256]{}, releaseName[256]{};
        GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_FINAL_READY", readyName, 256);
        GetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_FINAL_RELEASE", releaseName, 256);
        const auto ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, readyName);
        const auto gate = OpenEventW(SYNCHRONIZE, FALSE, releaseName);
        if (ready)
            SetEvent(ready);
        if (gate)
            WaitForSingleObject(gate, 10000);
        if (ready)
            CloseHandle(ready);
        if (gate)
            CloseHandle(gate);
    }
    if ((Mode() == 204 && startup[ViewRuns] && startup[Phase] != 4) || (Mode() == 212 && startup[Phase] == 2 && !startup[ViewRuns]))
        return Status::Failed;
    if (Mode() == 225 && startup[Phase] == 4)
        throw std::runtime_error("controlled stopped read throw");
    auto next = startup;
    if (Mode() == 211 && next[Phase] == 4)
        next[BackendStarted] = 1;
    if (Mode() == 215 && next[Phase] == 2 && next[ViewRuns])
        ++next[OwnerThread];
    if (Mode() == 219 && next[Phase] == 4)
        next[ViewRuns] = 2;
    std::copy(next.begin(), next.end(), output);
    return Status::Ok;
}
extern "C" __declspec(dllexport) DefinitiveFrontendStartup::Status MTA_RunDEFrontendStartupView(
    void* handle, std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
    DefinitiveInputReference::PublishCallback publisher) noexcept(false)
{
    LogStartup('V');
    using namespace DefinitiveFrontendStartup;
    if (handle != &startupToken || duration != DefinitiveGuiFrontendInputReference::MaximumDurationMs || !cancelled || !publisher)
        return Status::InvalidArgument;
    if (startup[OwnerThread] != GetCurrentThreadId())
        return Status::WrongThread;
    if (Mode() == 216)
    {
        startup[LastFailure] = static_cast<std::uint32_t>(Failure::InputBind);
        return Status::Failed;
    }
    if (Mode() == 224)
        throw std::runtime_error("controlled pre-view throw");
    cancel = cancelled;
    publish = publisher;
    startup[ExactImageBound] = startup[ViewRuns] = 1;
    ++startup[Pulses];
    ControlledFrontendInput();
    if (Mode() == 206)
    {
        startup[Phase] = 3;
        startup[CleanupPending] = 1;
        startup[LastViewCleanupComplete] = 0;
        return Status::Busy;
    }
    if (Mode() == 221)
        throw std::runtime_error("controlled post-view throw");
    return Mode() == 205 || Mode() == 209 || Mode() == 218 ? Status::Failed : Status::Ok;
}
extern "C" __declspec(dllexport) DefinitiveFrontendStartup::Status MTA_ShutdownDEFrontendStartup(void* handle) noexcept(false)
{
    LogStartup('S');
    using namespace DefinitiveFrontendStartup;
    if (handle != &startupToken)
        return Status::InvalidArgument;
    if (startup[OwnerThread] != GetCurrentThreadId())
        return Status::WrongThread;
    if (Mode() == 223)
        throw std::runtime_error("controlled shutdown throw");
    ++startupShutdowns;
    if (((Mode() == 206 || Mode() == 207 || Mode() == 227) && startupShutdowns < 4) || (Mode() == 226 && startupShutdowns < 44) || Mode() == 213)
    {
        startup[Phase] = 3;
        startup[CleanupPending] = 1;
        return Mode() == 213 ? Status::Failed : Status::Busy;
    }
    startup[Phase] = 4;
    startup[BackendStarted] = startup[ExactImageBound] = startup[CapabilitiesLow] = startup[CleanupPending] = 0;
    startup[CleanupComplete] = 1;
    startup[LastViewCleanupComplete] = 1;
    return Status::Ok;
}
extern "C" __declspec(dllexport) DefinitiveFrontendStartup::Status MTA_ReleaseDEFrontendStartup(void* handle) noexcept(false)
{
    LogStartup('F');
    using namespace DefinitiveFrontendStartup;
    if (handle != &startupToken || startup[OwnerThread] != GetCurrentThreadId() || startup[Phase] != 4)
        return Status::InvalidArgument;
    if (Mode() == 222)
        throw std::runtime_error("controlled release throw");
    ++startupReleases;
    return (Mode() == 208 || Mode() == 228) && startupReleases < 4 ? Status::Busy : Mode() == 214 ? Status::Failed : Status::Ok;
}

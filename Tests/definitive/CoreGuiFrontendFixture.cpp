/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Controlled protocol producer; contains no MTA or engine runtime
 *****************************************************************************/
#include "../../Client/core/DefinitiveGuiFrontendReference.h"
#include "../../Client/core/DefinitiveFrontend.h"
#include "../../Client/core/DefinitiveInputReference.h"
#include "../../Client/core/DefinitiveGuiFrontendInputReference.h"
#include <windows.h>
#include <algorithm>
#include <cwchar>

namespace
{
    bool                                            mta = false, gta = false;
    bool                                            coreOwnedInput = false;
    DefinitiveInputReference::CancelCallback        cancel = nullptr;
    DefinitiveInputReference::PublishCallback       publish = nullptr;
    DefinitiveGuiFrontendReference::ControlCallback control = nullptr;
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

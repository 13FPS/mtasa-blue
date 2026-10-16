/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual async worker against controlled, engine-free frontend leases
 *****************************************************************************/
#include "../../Client/definitive/CoreStartupReference.h"
#include <cstdio>
#include <cwchar>
#include <thread>
#include <stdexcept>
#include <string>

namespace deonline
{
    struct FrontendControlMailboxTestAccess
    {
        static SRWLOCK& Lock(FrontendControlMailbox& mailbox) { return mailbox.lock; }
    };
    struct CoreStartupReferenceTestAccess
    {
        static SRWLOCK& Lock(CoreStartupReference& worker) { return worker.inputLock; }
        static SRWLOCK& ControlLock(CoreStartupReference& worker) { return FrontendControlMailboxTestAccess::Lock(worker.frontendControl); }
    };
}
namespace
{
    unsigned checks = 0;
    int      anchor = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    template <class Condition>
    void Until(Condition condition, const char* reason)
    {
        const auto begin = GetTickCount64();
        while (!condition())
        {
            Check(GetTickCount64() - begin < 15000, reason);
            Sleep(2);
        }
    }
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        using namespace DefinitiveGuiFrontendReference;
        Check(argc == 2, "mode required");
        const auto mode = static_cast<unsigned>(std::wcstoul(argv[1], nullptr, 10));
        SetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_MODE", argv[1]);
        const auto eventName = L"Local\\MtaFrontendRelease-" + std::to_wstring(GetCurrentProcessId());
        const auto release = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
        Check(release && SetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_RELEASE", eventName.c_str()), "fixture release setup");
        const auto readyName = eventName + L"-FinalReady", finalName = eventName + L"-FinalRelease";
        const auto ready = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
        const auto final = CreateEventW(nullptr, TRUE, FALSE, finalName.c_str());
        Check(ready && final && SetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_FINAL_READY", readyName.c_str()) &&
                  SetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_FINAL_RELEASE", finalName.c_str()),
              "fixture final-read setup");
        const auto detachReadyName = eventName + L"-DetachReady", detachGateName = eventName + L"-DetachGate";
        const auto detachReady = CreateEventW(nullptr, TRUE, FALSE, detachReadyName.c_str());
        const auto detachGate = CreateEventW(nullptr, TRUE, FALSE, detachGateName.c_str());
        Check(detachReady && detachGate && SetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_DETACH_READY", detachReadyName.c_str()) &&
                  SetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_DETACH_GATE", detachGateName.c_str()),
              "fixture cleanup-publication setup");
        deonline::CoreStartupReference worker;
        Check(!worker.CaptureGuiFrontend() && !worker.CaptureGuiFrontendControl() && !worker.ReadGuiFrontend(0), "unbegun caches empty");
        std::uint32_t previous = 0;
        for (unsigned attempt = 0; attempt < (mode == 0 ? 2u : 1u); ++attempt)
        {
            ResetEvent(release);
            ResetEvent(detachReady);
            ResetEvent(detachGate);
            Check(worker.BeginGuiFrontendReference(&anchor), "frontend begin");
            Check(!worker.Begin(&anchor) && !worker.BeginGuiSurfaceReference(&anchor) && !worker.BeginGuiStartupReference(&anchor),
                  "shared owner excludes every historical protocol");
            if (mode == 1 || mode == 2 || mode == 3)
            {
                Until([&] { return worker.Poll() == 1; }, "early failure completion");
                Check(worker.Read(16) == (mode == 1 ? 14u : mode == 2 ? 13u : 0u), "early failure preserves protocol classification");
                Check(!GetModuleHandleW(L"core.dll"), "safe early failure unloads Core");
                if (mode == 3)
                    Check(worker.CaptureGuiFrontend() && worker.ReadGuiFrontend(FrontendStarted) == 0 && worker.ReadGuiFrontend(ControlRequested) == 0,
                          "no-start workload does not invent control ownership");
                break;
            }
            Until([&] { return worker.CaptureGuiFrontendControl() && worker.ReadGuiFrontendControl(ControlField::Requested); }, "mailbox request");
            const auto generation = worker.ReadGuiFrontendControl(ControlField::Generation);
            Check(generation > previous, "generations monotonic across worker attempts");
            previous = generation;
            Check(!worker.RestoreGuiFrontendControl(generation), "cannot restore before input detachment");
            bool        foreign = true;
            std::thread other(
                [&]
                {
                    foreign = worker.CaptureGuiFrontend() || worker.CaptureGuiFrontendControl() || worker.AcknowledgeGuiFrontendControl(generation) ||
                              worker.HeartbeatGuiFrontendControl(generation) || worker.RevokeGuiFrontendControl(generation) ||
                              worker.RestoreGuiFrontendControl(generation) || worker.ReadGuiFrontendControl(0) != 0;
                });
            other.join();
            Check(!foreign, "foreign thread cannot use frontend or mailbox APIs");
            Check(!worker.AcknowledgeGuiFrontendControl(generation + 1), "stale acknowledgement rejected");
            if (mode != 4)
                Check(worker.AcknowledgeGuiFrontendControl(generation) && !worker.AcknowledgeGuiFrontendControl(generation),
                      "one acknowledgement per generation");
            else
                Check(worker.RevokeGuiFrontendControl(generation), "no-write cancellation revokes unacknowledged request");
            Until(
                [&]
                {
                    return worker.CaptureGuiFrontend() && worker.ReadGuiFrontend(ControlGeneration) == generation &&
                           (mode == 4 || worker.ReadGuiFrontend(FrontendStarted));
                },
                "coherent frontend publication");
            Check(!worker.CaptureGuiSurface() && !worker.CaptureGuiStartup() && !worker.ReadGuiSurface(0) && !worker.ReadGuiStartup(0) &&
                      !worker.ReadGuiFrontend(160),
                  "old caches stay independent");
            if (mode != 4)
                Check(worker.ReadGuiFrontend(InputEventsHigh) == 2, "frontend high counter half preserved");
            SetEvent(release);
            Check(WaitForSingleObject(detachReady, 10000) == WAIT_OBJECT_0, "active publications quiesce before cleanup contention");
            const auto capturedSequence = worker.ReadGuiFrontend(DefinitiveGuiStartupReference::Sequence);
            auto&      lock = deonline::CoreStartupReferenceTestAccess::Lock(worker);
            AcquireSRWLockExclusive(&lock);
            Check(!worker.CaptureGuiFrontend() && worker.ReadGuiFrontend(DefinitiveGuiStartupReference::Sequence) >= capturedSequence,
                  "contended frontend capture preserves previous output");
            SetEvent(detachGate);
            Until([&] { return worker.CaptureGuiFrontendControl() && worker.ReadGuiFrontendControl(ControlField::Detached); },
                  "input detachment advances while publication lock held");
            Check(worker.ReadGuiFrontendControl(ControlField::Revoked) && !worker.ReadGuiFrontendControl(ControlField::Fresh),
                  "detachment revokes input permission");
            if (mode != 6)
                Check(worker.RestoreGuiFrontendControl(generation), "script proves restoration while GPU publication still busy");
            SRWLOCK* controlLock = nullptr;
            if (mode == 8)
            {
                Check(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0, "producer must observe restoration before final mailbox contention");
                controlLock = &deonline::CoreStartupReferenceTestAccess::ControlLock(worker);
                AcquireSRWLockExclusive(controlLock);
                SetEvent(final);
            }
            ReleaseSRWLockExclusive(&lock);
            if (controlLock)
            {
                Until([&] { return worker.CaptureGuiFrontend() && worker.ReadGuiFrontend(DefinitiveGuiStartupReference::Phase) == 2; },
                      "final publication remains readable while final mailbox read waits");
                Sleep(50);
                Check(worker.Poll() == 0 && GetModuleHandleW(L"core.dll"), "mailbox contention retains worker and Core instead of terminal quarantine");
                ReleaseSRWLockExclusive(controlLock);
            }
            Until([&] { return worker.Poll() == 1; }, "frontend completion");
            const bool unsafe = mode == 5 || mode == 6 || mode == 7 || mode >= 10;
            Check(worker.Read(16) == (unsafe ? 15u : 0u), "worker accepts only complete frontend ownership proof");
            Check((GetModuleHandleW(L"core.dll") != nullptr) == unsafe, "unsafe return retains Core code; safe return unloads");
            if (unsafe)
                Check(!worker.Begin(&anchor) && !worker.BeginGuiFrontendReference(&anchor) && !worker.BeginGuiSurfaceReference(&anchor),
                      "quarantine cannot be bypassed by historical attempt");
            else
                Check(worker.CaptureGuiFrontend() && worker.ReadGuiFrontend(ControlRestored) == 1 &&
                          worker.ReadGuiFrontend(DefinitiveGuiStartupReference::Phase) == 2,
                      "terminal frontend cache remains readable");
            if (mode == 9)
                Check(worker.ReadGuiFrontend(DefinitiveGuiStartupReference::CleanupDeadlineExceeded) == 1,
                      "eventual retirement preserves historical deadline failure without quarantine");
        }
        CloseHandle(release);
        CloseHandle(ready);
        CloseHandle(final);
        CloseHandle(detachReady);
        CloseHandle(detachGate);
        std::printf("{\"success\":true,\"checks\":%u,\"mode\":%u,\"fixture_is_mta\":false,\"game_launched\":false}\n", checks, mode);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        std::printf("{\"success\":false,\"checks\":%u}\n", checks);
        return 1;
    }
}

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
#include <filesystem>
#include <fstream>
#include <sstream>

namespace deonline
{
    struct FrontendControlMailboxTestAccess
    {
        static SRWLOCK& Lock(FrontendControlMailbox& mailbox) { return mailbox.lock; }
    };
    struct CoreStartupReferenceTestAccess
    {
        static SRWLOCK&                    Lock(CoreStartupReference& worker) { return worker.inputLock; }
        static SRWLOCK&                    ControlLock(CoreStartupReference& worker) { return FrontendControlMailboxTestAccess::Lock(worker.frontendControl); }
        static std::atomic<std::uint32_t>& StartupSequence(CoreStartupReference& worker) { return worker.frontendStartupSequence; }
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
        if (mode >= 200)
        {
            namespace Startup = DefinitiveFrontendStartup;
            namespace Report = deonline::FrontendStartupReport;
            namespace Input = DefinitiveGuiFrontendInputReference;
            wchar_t executable[32768]{};
            Check(GetModuleFileNameW(nullptr, executable, 32768) != 0, "owned fixture executable path");
            const auto log = std::filesystem::path(executable).parent_path() / L"lifecycle.log";
            Check(SetEnvironmentVariableW(L"MTA_FRONTEND_FIXTURE_LIFECYCLE", log.c_str()), "fixture lifecycle observation path");
            Check(!worker.CaptureGuiFrontendStartup() && !worker.ReadGuiFrontendStartup(0), "completed startup cache begins empty");
            const bool early = mode == 201 || mode == 202 || mode == 203 || mode == 212 || mode == 216 || mode == 220 || mode == 224 || mode == 229;
            Check(worker.BeginGuiFrontendStartupReference(&anchor), "completed Core mode begins separately");
            Check(!worker.Begin(&anchor) && !worker.BeginGuiFrontendInputReference(&anchor) && !worker.BeginGuiFrontendReference(&anchor),
                  "completed Core shares worker ownership without entering a historical startup path");
            if (mode == 229)
            {
                Check(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0 && worker.Cancel(), "cancel a known completed owner before view admission");
                SetEvent(final);
            }
            if (!early)
            {
                Until([&] { return worker.CaptureGuiFrontendInput() && worker.ReadGuiFrontendInput(DefinitiveGuiStartupReference::Phase) == 1; },
                      "completed owner publishes actual protocol256 through the existing cache");
                Check(worker.CaptureGuiFrontendStartup() && worker.ReadGuiFrontendStartup(Report::Begin + Startup::Phase) == 2 &&
                          worker.ReadGuiFrontendStartup(Report::Begin + Startup::BackendStarted) == 1 &&
                          worker.ReadGuiFrontendStartup(Report::Begin + Startup::CoreRetained) == 1,
                      "copied Begin32 proves a completed Core before the bounded view");
                Check(!worker.CaptureGuiFrontendControl() && !worker.CaptureGuiFrontend() && !worker.CaptureGuiSurface(),
                      "completed startup mode opens no script control mailbox or old GUI cache");
                bool        foreign = true;
                std::thread observer(
                    [&] { foreign = worker.CaptureGuiFrontendStartup() || worker.ReadGuiFrontendStartup(0) != 0 || worker.CaptureGuiFrontendInput(); });
                observer.join();
                Check(!foreign, "completed startup report and input captures reject foreign script threads");
                auto& sequence = deonline::CoreStartupReferenceTestAccess::StartupSequence(worker);
                sequence.fetch_add(1);
                Check(!worker.CaptureGuiFrontendStartup() && worker.ReadGuiFrontendStartup(Report::Begin + Startup::Phase) == 2,
                      "in-progress atomic report publication preserves the last coherent capture");
                sequence.fetch_add(1);
                auto& lock = deonline::CoreStartupReferenceTestAccess::Lock(worker);
                AcquireSRWLockExclusive(&lock);
                Check(worker.CaptureGuiFrontendStartup() && !worker.CaptureGuiFrontendInput(),
                      "completed Core scalar capture never waits on the other protocol's held script lock");
                ReleaseSRWLockExclusive(&lock);
                if (mode == 209)
                {
                    // Cancellation is intentionally a cross-thread atomic
                    // signal; only lifecycle and copied reads require ownership.
                    bool        accepted = false;
                    std::thread canceller([&] { accepted = worker.Cancel(); });
                    canceller.join();
                    Check(accepted, "foreign cancellation requests bounded view retirement without moving cleanup ownership");
                }
                else
                    SetEvent(release);
                if (mode == 206 || mode == 207 || mode == 226 || mode == 227)
                {
                    Until([&] { return worker.CaptureGuiFrontendStartup() && worker.ReadGuiFrontendStartup(Report::Stopped + Startup::Phase) == 3; },
                          "Busy shutdown publishes its actual pending owner without ending the worker");
                    Check(worker.Poll() == 0 && GetModuleHandleW(L"core.dll") && !worker.BeginGuiFrontendStartupReference(&anchor),
                          "Busy shutdown retains a live same-thread retry and rejects replacement work");
                    if (mode == 227)
                        Check(worker.Cancel(), "cancellation during Busy shutdown preserves cleanup retries");
                }
                if (mode == 208 || mode == 228)
                {
                    Until(
                        [&]
                        {
                            std::ifstream pending(log);
                            char          operation = 0;
                            unsigned long pendingThread = 0;
                            while (pending >> operation >> pendingThread)
                                if (operation == 'F')
                                    return true;
                            return false;
                        },
                        "Busy release is invoked on the worker before retry");
                    Check(worker.Poll() == 0 && GetModuleHandleW(L"core.dll"), "Busy release cannot abandon its owning worker thread");
                    if (mode == 228)
                        Check(worker.Cancel(), "cancellation during Busy release preserves cleanup retries");
                }
            }
            Until([&] { return worker.Poll() == 1; }, "completed Core worker performs owner-thread shutdown after every outcome");
            const bool retained = mode == 210 || mode == 211 || mode == 212 || mode == 213 || mode == 214 || mode == 219 || mode == 220 || mode == 222 ||
                                  mode == 223 || mode == 225;
            const auto expectedError = mode == 200 || mode == 207 || mode == 208                  ? 0u
                                       : mode == 201                                              ? 20u
                                       : mode == 202 || mode == 203 || mode == 220                ? 21u
                                       : mode == 204 || mode == 215                               ? 22u
                                       : mode == 214 || mode == 222                               ? 25u
                                       : mode == 209 || mode == 227 || mode == 228 || mode == 229 ? 12u
                                       : retained || mode == 226                                  ? 24u
                                                                                                  : 23u;
            Check(worker.Read(16) == expectedError, "completed owner preserves workload, cancellation and cleanup failure classifications");
            Check(worker.Read(17) == (mode == 200 || mode == 207 || mode == 208 ? 1u : 0u), "failed or cancelled workload cannot become successful cleanup");
            for (std::uint32_t field = 0; field < 8; ++field)
                Check(!worker.Read(field), "completed mode does not reuse the historical full-startup8 schema");
            Check(worker.Read(13) == (retained ? 0u : 1u) && (GetModuleHandleW(L"core.dll") != nullptr) == retained,
                  "only independently retired completed Core code is unloaded");
            Check(!worker.ReadGuiFrontendStartup(Report::FieldCount) && !worker.Read(19), "report and worker scalar reads enforce exact bounds");
            const bool hasReport = mode != 201 && mode != 202 && mode != 220;
            Check(worker.CaptureGuiFrontendStartup() == hasReport, "only actual successful32 readbacks create lifecycle evidence");
            if (hasReport)
            {
                if (mode != 212)
                    Check(worker.ReadGuiFrontendStartup(Report::Begin + Startup::Phase) == (mode == 203 ? 5u : 2u),
                          "Begin report retains the real initial lifecycle phase");
                if (mode == 203 || mode == 212 || mode == 229)
                    Check(!worker.ReadGuiFrontendStartup(Report::AfterView), "skipped view keeps the AfterView32 segment empty");
                if (mode == 229)
                    Check(worker.Read(9) == static_cast<std::uint32_t>(Startup::Status::Stopped) && !worker.CaptureGuiFrontendInput(),
                          "never-called view has a valid stopped status and invents no256 evidence");
                if (!retained)
                    Check(worker.ReadGuiFrontendStartup(Report::Stopped + Startup::Phase) == 4 &&
                              worker.ReadGuiFrontendStartup(Report::Stopped + Startup::CoreRetained) == 1 &&
                              !worker.ReadGuiFrontendStartup(Report::Stopped + Startup::BackendStarted) &&
                              worker.ReadGuiFrontendStartup(Report::Stopped + Startup::CleanupComplete) == 1,
                          "Stopped32 is copied before release, with completed Core retained and backend retired");
                if (mode == 200)
                    Check(worker.ReadGuiFrontendStartup(Report::AfterView + Startup::CoreRetained) == 1 &&
                              worker.ReadGuiFrontendStartup(Report::AfterView + Startup::BackendStarted) == 1 &&
                              !worker.ReadGuiFrontendStartup(Report::AfterView + Startup::FrontendRetained) &&
                              worker.ReadGuiFrontendStartup(Report::AfterView + Startup::LastViewCleanupComplete) == 1,
                          "view retirement preserves the completed Core/backend beyond the GUI lifetime");
            }
            if (retained)
                Check(!worker.Begin(&anchor) && !worker.BeginGuiFrontendStartupReference(&anchor) && !worker.BeginGuiFrontendInputReference(&anchor),
                      "completed startup quarantine cannot be bypassed by historical or new attempts");
            std::ifstream operations(log);
            char          operation = 0;
            unsigned long threadId = 0, ownerId = 0;
            unsigned      begins = 0, shuts = 0, releases = 0, initializes = 0;
            while (operations >> operation >> threadId)
            {
                if (!ownerId)
                    ownerId = threadId;
                Check(threadId == ownerId && threadId != GetCurrentThreadId(), "all Core lifecycle callbacks execute on the same worker thread");
                begins += operation == 'B';
                shuts += operation == 'S';
                releases += operation == 'F';
                initializes += operation == 'I';
            }
            Check(initializes == 0, "new completed Core path never calls InitializeCore");
            Check(begins == (mode == 201 ? 0u : 1u), "new mode uses its own completed Core factory");
            Check(shuts == (mode == 201 || mode == 202 || mode == 220   ? 0u
                            : mode == 226                               ? 44u
                            : mode == 206 || mode == 207 || mode == 227 ? 4u
                                                                        : 1u),
                  "known owners always receive shutdown, including cooperative cancellation and failed view calls");
            Check(releases == (mode == 208 || mode == 228                                                ? 4u
                               : (!retained || mode == 214 || mode == 222) && mode != 201 && mode != 202 ? 1u
                                                                                                         : 0u),
                  "unverified owners are retained without release, while proven cleanup reaches release");
            if (mode == 209)
                Check(worker.Read(10) == static_cast<std::uint32_t>(Startup::Status::Ok) && !GetModuleHandleW(L"core.dll"),
                      "cancellation still completes shutdown and unload");
            CloseHandle(release);
            CloseHandle(ready);
            CloseHandle(final);
            CloseHandle(detachReady);
            CloseHandle(detachGate);
            std::printf("{\"success\":true,\"checks\":%u,\"mode\":%u,\"fixture_is_mta\":false,\"game_launched\":false}\n", checks, mode);
            return 0;
        }
        if (mode >= 100)
        {
            // This controlled producer checks the real worker/cache/retirement
            // boundary. It contains no MTA GUI or native engine implementation.
            namespace New = DefinitiveGuiFrontendInputReference;
            Check(!worker.CaptureGuiFrontendInput() && !worker.ReadGuiFrontendInput(0), "new frontend input cache is empty before Begin");
            Check(worker.BeginGuiFrontendInputReference(&anchor), "independent Core-owned frontend input begins");
            Check(!worker.Begin(&anchor) && !worker.BeginGuiFrontendReference(&anchor) && !worker.BeginGuiSurfaceReference(&anchor),
                  "Core-owned input shares exclusive worker ownership with historical protocols");
            if (mode == 101 || mode == 102)
            {
                Until([&] { return worker.Poll() == 1; }, "new protocol rejection completes");
                Check(worker.Read(16) == (mode == 101 ? 13u : 14u) && !GetModuleHandleW(L"core.dll"),
                      "wrong new version/configuration unloads before retaining an owner");
                Check(!worker.CaptureGuiFrontendInput(), "rejected new protocol invents no snapshot");
            }
            else
            {
                Until([&] { return worker.CaptureGuiFrontendInput() && worker.ReadGuiFrontendInput(DefinitiveGuiStartupReference::Phase) == 1; },
                      "new coherent256 active snapshot");
                Check(worker.ReadGuiFrontendInput(New::NativeMetrics + 7) == 2 && worker.ReadGuiFrontendInput(New::Bindings + 11) == 2 &&
                          !worker.ReadGuiFrontendInput(256),
                      "new cache preserves native high counter halves and independent binding fields");
                Check(!worker.CaptureGuiFrontend() && !worker.CaptureGuiSurface() && !worker.CaptureGuiFrontendControl(),
                      "new mode neither exposes old caches nor opens the script control mailbox");
                bool        foreign = true;
                std::thread observer([&] { foreign = worker.CaptureGuiFrontendInput() || worker.ReadGuiFrontendInput(0) != 0; });
                observer.join();
                Check(!foreign, "foreign observer cannot capture or read the new input cache");
                auto& lock = deonline::CoreStartupReferenceTestAccess::Lock(worker);
                AcquireSRWLockExclusive(&lock);
                Check(!worker.CaptureGuiFrontendInput() && worker.ReadGuiFrontendInput(New::NativeMetrics + 7) == 2,
                      "contended capture preserves the last complete new snapshot");
                ReleaseSRWLockExclusive(&lock);
                if (mode == 111)
                    Check(worker.Cancel(), "new input worker accepts cooperative cancellation without a script restoration Ack");
                else
                    SetEvent(release);
                Until([&] { return worker.Poll() == 1; }, "new input worker terminal cleanup");
                const bool unsafe = mode >= 103 && mode <= 109;
                Check(worker.Read(16) == (unsafe ? 15u : mode == 111 ? 12u : 0u), "new worker independently checks native/binding retirement");
                Check((GetModuleHandleW(L"core.dll") != nullptr) == unsafe, "new unsafe native or CPU ownership retains Core code");
                Check(worker.CaptureGuiFrontendInput() && worker.ReadGuiFrontendInput(DefinitiveGuiStartupReference::Phase) == 2,
                      "new terminal copied snapshot remains available");
                if (unsafe)
                    Check(!worker.Begin(&anchor) && !worker.BeginGuiFrontendReference(&anchor) && !worker.BeginGuiFrontendInputReference(&anchor),
                          "new input quarantine cannot be bypassed by another protocol");
                if (mode == 110)
                    Check(worker.ReadGuiFrontendInput(DefinitiveGuiStartupReference::CleanupDeadlineExceeded) == 1,
                          "new eventual cleanup preserves historical workload deadline failure");
            }
            CloseHandle(release);
            CloseHandle(ready);
            CloseHandle(final);
            CloseHandle(detachReady);
            CloseHandle(detachGate);
            std::printf("{\"success\":true,\"checks\":%u,\"mode\":%u,\"fixture_is_mta\":false,\"game_launched\":false}\n", checks, mode);
            return 0;
        }
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

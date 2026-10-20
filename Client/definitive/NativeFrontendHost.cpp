/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/
#include "NativeFrontendHost.h"
#include "NativeFrontendActivation.h"
#include "CoreStartupReference.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <process.h>

namespace deonline
{
    namespace N = NativeFrontendHostProtocol;
    namespace R = FrontendRuntimeReport;
    namespace C = DefinitiveFrontendRuntime;

    NativeFrontendHost::NativeFrontendHost(CoreStartupReference& runtime, std::atomic<DWORD>& controllerThread) noexcept
        : runtime(runtime), controllerThread(controllerThread)
    {
        published[N::Version].store(N::ApiVersion);
    }

    bool NativeFrontendHost::Start(const void* address, const N::RequestV1& request) noexcept
    {
        if (!address || request.size != sizeof(request) || request.version != N::ApiVersion || request.reserved || (request.flags & ~N::SupportedFlags))
            return false;
        bool expected = false;
        if (!started.compare_exchange_strong(expected, true))
            return false;
        moduleAddress = address;
        report = {};
        report[N::Version] = N::ApiVersion;
        report[N::HostPhase] = static_cast<std::uint32_t>(N::Phase::Starting);
        report[N::Flags] = request.flags;
        HMODULE pinned = nullptr;
        // The ASI owns a process-long reference too. Pin this actual Bridge
        // before handing its code and original worker to an independent host.
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(address), &pinned))
        {
            Fail(N::Failure::ModulePin);
            Publish();
            return false;
        }
        report[N::ControllerPinned] = 1;
        Publish();
        const auto thread = _beginthreadex(nullptr, 0, ThreadMain, this, 0, nullptr);
        if (!thread)
        {
            Fail(N::Failure::ThreadCreation);
            Publish();
            return false;
        }
        // No destructor joins a thread under the loader lock. The pinned host
        // owns its worker until explicit, verified retirement or process exit.
        CloseHandle(reinterpret_cast<HANDLE>(thread));
        return true;
    }

    bool NativeFrontendHost::Stop() noexcept
    {
        if (!started.load())
            return false;
        stopRequested.store(true);
        return true;
    }

    bool NativeFrontendHost::Read(std::uint32_t* output, std::uint32_t count) const noexcept
    {
        if (!output || count != N::FieldCount)
            return false;
        for (unsigned attempt = 0; attempt < 4; ++attempt)
        {
            const auto before = publication.load();
            if (before & 1)
                continue;
            N::Snapshot copy{};
            for (std::size_t field = 0; field < copy.size(); ++field)
                copy[field] = published[field].load();
            if (before == publication.load())
            {
                std::copy(copy.begin(), copy.end(), output);
                return true;
            }
        }
        return false;
    }

    void NativeFrontendHost::Publish() noexcept
    {
        publication.fetch_add(1);
        for (std::size_t field = 0; field < report.size(); ++field)
            published[field].store(report[field]);
        publication.fetch_add(1);
        Log();
    }

    void NativeFrontendHost::Fail(N::Failure failure) noexcept
    {
        if (!report[N::LastFailure])
            report[N::LastFailure] = static_cast<std::uint32_t>(failure);
        report[N::HostPhase] = static_cast<std::uint32_t>(N::Phase::Failed);
    }

    void NativeFrontendHost::Log() noexcept
    {
        // Publish runs often for heartbeat/key responsiveness; durable output
        // records ownership transitions rather than every keyboard sample.
        bool changed = false;
        for (const auto field : {N::HostPhase, N::RuntimeGeneration, N::LastCommand, N::CommandSequence, N::WorkerPhase, N::WorkerFailure, N::CleanupPending,
                                 N::CleanupComplete, N::ViewAttempts, N::ViewsRetired, N::BindingLoads, N::LastFailure})
            changed = changed || report[field] != logged[field];
        if (!changed || log == INVALID_HANDLE_VALUE)
            return;
        char       line[1400]{};
        const auto length = std::snprintf(
            line, sizeof(line),
            "{\"schema\":\"mta_native_frontend_host_v1\",\"tick_ms\":%llu,\"phase\":%u,\"owner_thread\":%u,\"flags\":%u,"
            "\"runtime_generation\":%u,\"command\":%u,\"command_sequence\":%u,\"worker_phase\":%u,\"worker_failure\":%u,"
            "\"core_retained\":%u,\"cleanup_pending\":%u,\"cleanup_complete\":%u,\"core_stopped\":%u,\"core_released\":%u,\"core_unloaded\":%u,"
            "\"view_attempts\":%u,\"views_retired\":%u,\"configuration_read_only\":%u,\"manager_creations\":%u,\"binding_loads\":%u,\"configuration_loads\":%u,"
            "\"failure\":%u,\"controller_pinned\":%u,\"stop_requested\":%u,\"network_started\":false,\"gameplay_ready\":false,\"full_mta_ready\":false}\n",
            static_cast<unsigned long long>(GetTickCount64()), report[N::HostPhase], report[N::OwnerThread], report[N::Flags], report[N::RuntimeGeneration],
            report[N::LastCommand], report[N::CommandSequence], report[N::WorkerPhase], report[N::WorkerFailure], report[N::CoreRetained],
            report[N::CleanupPending], report[N::CleanupComplete], report[N::CoreStoppedVerified], report[N::CoreReleasedVerified],
            report[N::CoreUnloadedVerified], report[N::ViewAttempts], report[N::ViewsRetired], report[N::ConfigurationReadOnly],
            report[N::BindingManagerCreations], report[N::BindingLoads], report[N::ConfigurationLoads], report[N::LastFailure], report[N::ControllerPinned],
            report[N::StopRequested]);
        DWORD written = 0;
        if (length > 0 && static_cast<std::size_t>(length) < sizeof(line) && WriteFile(log, line, static_cast<DWORD>(length), &written, nullptr) &&
            written == static_cast<DWORD>(length) && FlushFileBuffers(log))
            logged = report;
    }

    unsigned __stdcall NativeFrontendHost::ThreadMain(void* context) noexcept
    {
        static_cast<NativeFrontendHost*>(context)->Run();
        return 0;
    }

    void NativeFrontendHost::Run() noexcept
    {
        report[N::OwnerThread] = GetCurrentThreadId();
        controllerThread.store(GetCurrentThreadId(), std::memory_order_release);
        std::uint32_t generation = 0;
        try
        {
            HMODULE                    self = nullptr;
            std::array<wchar_t, 32768> path{};
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   reinterpret_cast<LPCWSTR>(moduleAddress), &self))
            {
                const auto count = GetModuleFileNameW(self, path.data(), static_cast<DWORD>(path.size()));
                if (count && count < path.size())
                {
                    const auto      logs = std::filesystem::path(std::wstring(path.data(), count)).parent_path() / L"mta" / L"logs";
                    std::error_code ignored;
                    std::filesystem::create_directories(logs, ignored);
                    log = CreateFileW((logs / L"nativefrontend.log").c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                }
            }
            Publish();
            generation = runtime.BeginFrontendRuntime(moduleAddress, (report[N::Flags] & N::LocalInput) != 0, (report[N::Flags] & N::BindingPersistence) != 0);
            if (!generation)
                Fail(N::Failure::RuntimeBegin);
            else
            {
                report[N::RuntimeGeneration] = generation;
            }
        }
        catch (...)
        {
            Fail(N::Failure::Exception);
        }
        Publish();
        NativeFrontendActivation activation;
        bool                     togglePending = false, stopSent = false;
        while (generation)
        {
            runtime.HeartbeatFrontendRuntime(generation);
            const auto poll = runtime.PollFrontendRuntime();
            const bool captured = runtime.CaptureFrontendRuntime();
            if (captured)
            {
                const auto read = [&](std::uint32_t field) { return runtime.ReadFrontendRuntime(field); };
                report[N::WorkerCaptured] = 1;
                for (const auto fields : {std::array<std::uint32_t, 2>{N::WorkerPhase, R::WorkerPhase},
                                          {N::WorkerFailure, R::LastFailure},
                                          {N::CoreRetained, R::CoreOwnerRetained},
                                          {N::CleanupPending, R::CleanupPending},
                                          {N::CleanupComplete, R::CleanupComplete},
                                          {N::CoreStoppedVerified, R::CoreStoppedVerified},
                                          {N::CoreReleasedVerified, R::CoreReleasedVerified},
                                          {N::CoreUnloadedVerified, R::CoreUnloadedVerified},
                                          {N::ViewAttempts, R::ViewAttempts},
                                          {N::ViewsRetired, R::ViewsRetired},
                                          {N::ConfigurationReadOnly, R::CoreOffset + C::ConfigurationReadOnly},
                                          {N::BindingManagerCreations, R::CoreOffset + C::BindingManagerCreations},
                                          {N::BindingLoads, R::CoreOffset + C::BindingLoads},
                                          {N::ConfigurationLoads, R::CoreOffset + C::ConfigurationLoads}})
                    report[fields[0]] = read(fields[1]);
                if (!report[N::StopRequested] && !report[N::LastFailure] && !report[N::WorkerFailure] &&
                    report[N::WorkerPhase] >= static_cast<std::uint32_t>(R::Phase::Hidden) &&
                    report[N::WorkerPhase] <= static_cast<std::uint32_t>(R::Phase::Hiding))
                    report[N::HostPhase] = static_cast<std::uint32_t>(N::Phase::Running);
            }
            if (poll != 0)
            {
                if (!captured)
                    Fail(N::Failure::RuntimeRead);
                else if (poll < 0 || !report[N::StopRequested] || report[N::WorkerFailure] || !report[N::CleanupComplete] || !report[N::CoreStoppedVerified] ||
                         !report[N::CoreReleasedVerified] || !report[N::CoreUnloadedVerified])
                    Fail(N::Failure::UnexpectedWorkerExit);
                else
                    report[N::HostPhase] = static_cast<std::uint32_t>(N::Phase::Stopped);
                break;
            }
            DWORD         process = 0;
            const auto    window = GetForegroundWindow();
            const bool    focused = window && GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId();
            std::uint32_t keys = 0;
            if (focused)
                for (const auto entry : {std::array<int, 2>{VK_CONTROL, NativeFrontendActivation::Control},
                                         {VK_SHIFT, NativeFrontendActivation::Shift},
                                         {VK_F9, NativeFrontendActivation::F9},
                                         {VK_END, NativeFrontendActivation::End}})
                    if (GetAsyncKeyState(entry[0]) & 0x8000)
                        keys |= static_cast<std::uint32_t>(entry[1]);
            report[N::Focused] = focused ? 1u : 0u;
            report[N::KeyMask] = keys;
            const auto action = activation.Update(focused, keys, GetTickCount64());
            if (action == NativeFrontendActivation::Action::Stop)
                stopRequested.store(true);
            if (action == NativeFrontendActivation::Action::Toggle)
                togglePending = true;
            const auto command = [&](R::Command value) noexcept
            {
                auto sequence = report[N::CommandSequence];
                if (sequence >= UINT32_MAX - 1 && value != R::Command::Stop)
                {
                    Fail(N::Failure::CommandExhausted);
                    stopRequested.store(true);
                    return false;
                }
                if (sequence == UINT32_MAX || !runtime.CommandFrontendRuntime(generation, sequence + 1, static_cast<std::uint32_t>(value)))
                    return false;
                report[N::CommandSequence] = sequence + 1;
                report[N::LastCommand] = static_cast<std::uint32_t>(value);
                return true;
            };
            if (stopRequested.load())
            {
                report[N::StopRequested] = 1;
                report[N::HostPhase] = static_cast<std::uint32_t>(N::Phase::Stopping);
                if (!stopSent)
                    stopSent = command(R::Command::Stop) || runtime.Cancel();
            }
            else if (togglePending && focused && captured && !runtime.ReadFrontendRuntime(R::OutstandingSequence))
            {
                const auto phase = static_cast<R::Phase>(report[N::WorkerPhase]);
                if (phase == R::Phase::Hidden)
                    togglePending = !command(R::Command::Show);
                else if (phase == R::Phase::Visible)
                    togglePending = !command(R::Command::Hide);
            }
            Publish();
            Sleep(20);
        }
        Publish();
        if (log != INVALID_HANDLE_VALUE)
        {
            FlushFileBuffers(log);
            CloseHandle(log);
            log = INVALID_HANDLE_VALUE;
        }
        // Explicit Stop ends this controller for the process. Keeping its
        // identity reserved prevents a script thread adopting the old worker's
        // owner token or silently creating a replacement MTA runtime.
    }
}

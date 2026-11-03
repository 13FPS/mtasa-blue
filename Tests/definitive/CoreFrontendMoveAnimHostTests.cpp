/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real retained Core console-to-workload routing on an owned WARP host
 ****************************************************************************/
// Reuse actual renderer/GUI/owner helpers only. The historical host main and
// its capability/settings matrices are compiled under another name, not run.
#define wmain HistoricalCoreFrontendStartupHostMain
#include "CoreFrontendStartupHostTests.cpp"
#undef wmain
#include "../../Client/core/DefinitiveMoveAnimWorkload.h"

namespace
{
    namespace Move = DefinitiveMoveAnimWorkload;
    using ReadMove = Frontend::Status (*)(void*, std::uint32_t, Move::ReportV1*);
    void FocusOwnedWindow(HWND window)
    {
        DWORD process = 0;
        Check(window && IsWindow(window) && GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId(),
              "focus only this disposable host's own test window");
        // Foreground requests are subject to real Windows policy. Make one
        // ordinary request, then let the operator activate this exact owned
        // window instead of repeatedly taking focus from another application.
        // Share one wait budget across both checkpoints within the runner's
        // existing 45-second deadline; this is not an eligibility override.
        static ULONGLONG remainingFocusWaitMs = 30000;
        const auto       started = GetTickCount64();
        const auto       focused = [&]() noexcept { return GetForegroundWindow() == window && GetActiveWindow() == window && GetFocus() == window; };
        ShowWindow(window, SW_SHOWNORMAL);
        // The runner starts this disposable host with STARTUPINFO SW_HIDE.
        // The first ShowWindow may consume that inherited state instead of
        // our show request. Reveal only this still-hidden owned window before
        // asking Windows for ordinary foreground activation.
        if (!IsWindowVisible(window))
            ShowWindow(window, SW_SHOWNORMAL);
        SetActiveWindow(window);
        SetForegroundWindow(window);
        SetFocus(window);
        if (!focused())
        {
            std::printf("MTA_MOVEANIM_HOST_FOCUS_READY pid=%lu hwnd=%p remaining_wait_ms=%llu title=MTA owned movement-route test\n",
                        static_cast<unsigned long>(GetCurrentProcessId()), static_cast<void*>(window), static_cast<unsigned long long>(remainingFocusWaitMs));
            std::fflush(stdout);
        }
        MSG message{};
        do
        {
            while (PeekMessageW(&message, window, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            // Only after Windows grants foreground to this window may its
            // owning UI thread finish ordinary active-window/keyboard focus.
            if (GetForegroundWindow() == window && !focused())
            {
                SetActiveWindow(window);
                SetFocus(window);
            }
            if (focused() || !IsWindow(window) || GetTickCount64() - started >= remainingFocusWaitMs)
                break;
            MsgWaitForMultipleObjectsEx(0, nullptr, 25, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        } while (true);
        const auto elapsed = (std::min)(GetTickCount64() - started, remainingFocusWaitMs);
        remainingFocusWaitMs -= elapsed;
        Check(GetForegroundWindow() == window && GetActiveWindow() == window && GetFocus() == window,
              "Windows actually grants foreground and focus to the owned non-game host");
    }
    Move::ReportV1 MoveReport(Owned& owned, ReadMove read)
    {
        Move::ReportV1 value;
        Check(read(owned.startup, Move::ApiVersion, &value) == Frontend::Status::Ok, "read cached real Core movement report without a native query");
        return value;
    }
    void MoveReportArguments(Owned& owned, ReadMove read)
    {
        Move::ReportV1                            output;
        std::array<unsigned char, sizeof(output)> before{};
        std::memcpy(before.data(), &output, sizeof(output));
        Check(read(owned.startup, 0, &output) == Frontend::Status::InvalidArgument && !std::memcmp(&output, before.data(), sizeof(output)),
              "cached Core read rejects wrong version and preserves caller bytes");
        Check(read(owned.startup, 1, nullptr) == Frontend::Status::InvalidArgument &&
                  read(reinterpret_cast<void*>(std::uintptr_t{1}), 1, &output) == Frontend::Status::InvalidArgument &&
                  !std::memcmp(&output, before.data(), sizeof(output)),
              "cached Core read rejects absent storage and foreign owner without adopting it");
        Frontend::Status status = Frontend::Status::Ok;
        std::thread      foreign([&] { status = read(owned.startup, 1, &output); });
        foreign.join();
        Check(status == Frontend::Status::WrongThread && !std::memcmp(&output, before.data(), sizeof(output)),
              "real cached report still requires the retained Core producer thread");
    }
}

int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    try
    {
        Check(argc == 3 && sizeof(void*) == 8 && std::wstring(argv[2]) == L"runtime-moveanim", "one explicit owned x64 movement-route case supplied");
        const std::filesystem::path root(argv[1]);
        Check(root.is_absolute(), "owned fixture root is absolute");
        Owned owned;
        InitializeWarp(owned);
        owned.localWindow = CreateWindowExW(0, L"STATIC", L"MTA owned movement-route test", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr,
                                            GetModuleHandleW(nullptr), nullptr);
        Check(owned.localWindow != nullptr, "create one ordinary owned non-game window for the real focus gate");
        FocusOwnedWindow(owned.localWindow);
        owned.core = LoadLibraryExW((root / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.core != nullptr, "load the newly rebuilt actual Core in a hashed disposable non-game host");
        const auto wide = root.wstring(), game = root.parent_path().parent_path().wstring();
        Export<void (*)(const wchar_t*, std::size_t)>(owned.core, "SetMTADirectory")(wide.data(), wide.size());
        Export<void (*)(const wchar_t*, std::size_t)>(owned.core, "SetGTADirectory")(game.data(), game.size());
        owned.runtimeOwner = true;
        owned.beginRuntime = Export<Runtime::Begin>(owned.core, "MTA_BeginDEFrontendRuntime");
        owned.readRuntime = Export<Runtime::Read>(owned.core, "MTA_ReadDEFrontendRuntime");
        // RuntimeState verifies the unchanged Startup32 prefix through the
        // real startup export. Its helper definition is reused, while the
        // historical main that normally resolves this pointer is not run.
        owned.readStartup = Export<Startup::Read>(owned.core, "MTA_ReadDEFrontendStartup");
        owned.pulseRuntime = Export<Runtime::Pulse>(owned.core, "MTA_PulseDEFrontendRuntime");
        owned.shutdownRuntime = Export<Runtime::Shutdown>(owned.core, "MTA_ShutdownDEFrontendRuntime");
        owned.releaseRuntime = Export<Runtime::Release>(owned.core, "MTA_ReleaseDEFrontendRuntime");
        const auto readMove = Export<ReadMove>(owned.core, "MTA_ReadDEFrontendRuntimeMoveAnimWorkloadV1");
        owned.startup = owned.beginRuntime(1);
        Check(owned.startup != nullptr && RuntimeState(owned)[Startup::Phase] == 2, "real retained Runtime creates its existing configured Core owner");
        MoveReportArguments(owned, readMove);
        Check(owned.pulseRuntime(owned.startup) == Frontend::Status::Ok, "idle retained Core producer remains pulseable before activation");
        const auto initial = MoveReport(owned, readMove);
        Check(initial.phase == Move::Phase::Idle && !initial.queued && !initial.beginAttempted && !initial.attemptPending,
              "startup and idle pulses never activate the movement workload");

        owned.create = Export<Frontend::Create>(owned.core, "MTA_CreateDEFrontendSession");
        owned.pulse = Export<Frontend::Pulse>(owned.core, "MTA_PulseDEFrontendSession");
        owned.read = Export<Frontend::Read>(owned.core, "MTA_ReadDEFrontendSession");
        owned.shutdown = Export<Frontend::Shutdown>(owned.core, "MTA_ShutdownDEFrontendSession");
        owned.release = Export<Frontend::Release>(owned.core, "MTA_ReleaseDEFrontendSession");
        owned.cgui = LoadLibraryExW((root / L"cgui.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.cgui != nullptr, "load the actual MTA GUI for console character and Return dispatch");
        Check(Export<mta::gui::ConfigureGuiD3D12AssetRoot>(owned.cgui, "ConfigureGUID3D12AssetRoot")(mta::gui::GuiD3D12AssetRootApiVersion, wide.data(),
                                                                                                     static_cast<std::uint32_t>(wide.size())) == 1,
              "configure only the owned fixture asset root");
        mta::gui::GuiD3D12CreateInfo info;
        info.device = owned.device.Get();
        info.queue = owned.queue.Get();
        info.targetFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        info.width = Owned::Width;
        info.height = Owned::Height;
        owned.renderer = Export<mta::gui::CreateGuiD3D12Session>(owned.cgui, "CreateGUID3D12Session")(mta::gui::GuiD3D12ApiVersion, &info);
        Check(owned.renderer && owned.renderer->GetGUI(owned.gui) == GuiD3D12Status::Ok && owned.gui, "borrow the real WARP GUI on the same retained producer");
        owned.observed = std::make_unique<ObservedFrontendGui>(*owned.gui);
        owned.gui = owned.observed.get();
        owned.frontend = owned.create(Frontend::ApiVersion, owned.gui);
        Check(owned.frontend && owned.State()[Frontend::Phase] == 2 && RuntimeState(owned)[Runtime::ViewBorrowed],
              "real MTA menu borrows the configured Core session");
        const auto settleUntil = GetTickCount64() + 5000;
        do
        {
            Frame(owned, 4, 4);
            if (owned.State()[Frontend::MenuVisible] && !owned.State()[Frontend::MenuFading])
                break;
            Sleep(1);
        } while (GetTickCount64() < settleUntil);
        Check(owned.State()[Frontend::MenuVisible] && !owned.State()[Frontend::MenuFading], "real menu finishes its fade before console input");
        const auto enable = Export<Console::Enable>(owned.core, "MTA_EnableDEFrontendConsole");
        const auto visible = Export<Console::SetVisible>(owned.core, "MTA_SetDEFrontendConsoleVisible");
        const auto readConsole = Export<Console::Read>(owned.core, "MTA_ReadDEFrontendConsole");
        Check(enable(owned.frontend, 1) == Frontend::Status::Ok && visible(owned.frontend, 1) == Frontend::Status::Ok,
              "enable the actual retained Runtime console without a fixture command handler");
        for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>("testmoveanim"); *cursor; ++cursor)
            owned.gui->ProcessCharacter(*cursor);
        owned.gui->ProcessKeyboardInput(CGUIKeys::Return, true);
        owned.gui->ProcessKeyboardInput(CGUIKeys::Return, false);
        Console::Snapshot console{};
        Check(readConsole(owned.frontend, console.data(), Console::FieldCount) == Frontend::Status::Ok && console[Console::CommandsExecuted] == 1 &&
                  console[Console::CommandsRegistered] == 3 && !console[Console::CommandsRejected],
              "supported retained Runtime exposes the explicit third command through the existing real parser");
        const auto queued = MoveReport(owned, readMove);
        Check(queued.queued == 1 && queued.phase == Move::Phase::Queued && !queued.beginAttempted && !queued.attemptPending,
              "the console callback queues CPU work and never enters native workload Begin");
        (void)owned.pulseRuntime(owned.startup);
        Check(!MoveReport(owned, readMove).beginAttempted && RuntimeState(owned)[Runtime::ViewBorrowed],
              "a live borrowed menu excludes native movement admission");
        Check(owned.shutdown(owned.frontend) == Frontend::Status::Ok && owned.State()[Frontend::CleanupComplete],
              "menu shutdown retires real widgets and aggregate input");
        (void)owned.pulseRuntime(owned.startup);
        Check(!MoveReport(owned, readMove).beginAttempted, "stopped but unreleased GUI owner still cannot admit the workload");
        Check(owned.release(owned.frontend) == Frontend::Status::Ok, "release the real view before moving to native admission");
        owned.frontend = nullptr;
        Check(!RuntimeState(owned)[Runtime::ViewBorrowed], "view borrow has retired before the next retained Core pulse");
        FocusOwnedWindow(owned.localWindow);
        Check(owned.pulseRuntime(owned.startup) == Frontend::Status::Ok, "normal producer pulse handles the queued workload after view retirement");
        const auto refused = MoveReport(owned, readMove);
        Check(refused.beginAttempted == 1 && !refused.queued && !refused.attemptPending && refused.phase == Move::Phase::Finished &&
                  refused.beginStatus != mta::game_de::Status::Ok && refused.native.phase == mta::game_de::MoveAnimWorkloadPhaseV1::Refused &&
                  refused.native.notEntered == 1,
              "actual backend safely refuses this non-game image and retires its explicit no-entry attempt");
        Check(!refused.native.owner.generation && !refused.native.cleanupPending && !refused.native.tokenRegistered && !refused.native.overridePublished &&
                  !refused.native.hooksEnabled && !refused.native.writeAcknowledged && !refused.native.reapplyAcknowledged && !refused.native.endAcknowledged &&
                  refused.native.physicalConfirmationRequired && !refused.native.fullMtaReady,
              "real wrong-image refusal produces no native movement evidence or forged End acknowledgement");
        Check(owned.pulseRuntime(owned.startup) == Frontend::Status::Ok && MoveReport(owned, readMove).beginAttempted == 1,
              "an eligible later pulse cannot rearm the consumed explicit command");
        const auto preStop = RuntimeState(owned);
        Check(preStop[Startup::CoreRetained] && preStop[Startup::BackendStarted] && !preStop[Startup::FullMtaReady] && !preStop[Startup::GameplayReady],
              "safe native refusal retains the real Core services without claiming gameplay readiness");
        Check(owned.shutdownRuntime(owned.startup) == Frontend::Status::Ok, "shutdown drains movement ownership before session services and backend");
        const auto terminal = RuntimeState(owned);
        Check(terminal[Runtime::CleanupComplete] && !terminal[Runtime::CleanupPending] && !terminal[Runtime::ServicesRetained] &&
                  !terminal[Startup::BackendStarted] && terminal[Startup::Phase] == 4,
              "real final Runtime owner is retired before release");
        const auto stale = owned.startup;
        Check(owned.releaseRuntime(owned.startup) == Frontend::Status::Ok, "release the completed actual Core after all borrowers retire");
        owned.startup = nullptr;
        Move::ReportV1                                 staleOutput;
        std::array<unsigned char, sizeof(staleOutput)> oldBytes{};
        std::memcpy(oldBytes.data(), &staleOutput, sizeof(staleOutput));
        Check(readMove(stale, 1, &staleOutput) == Frontend::Status::InvalidArgument && !std::memcmp(oldBytes.data(), &staleOutput, sizeof(staleOutput)),
              "released owner cannot return stale cached movement state");
        std::ofstream result(root / L"frontend-startup-result.json");
        result << "{\"schema\":\"mta_actual_core_move_anim_host_v1\",\"success\":true,\"checks\":" << checks + 1
               << ",\"actualCoreMoveAnimConsoleRouteTested\":true,\"actualCoreMoveAnimDeferredAdmissionTested\":true,"
                  "\"actualCoreMoveAnimWrongImageRetirementTested\":true"
               << ",\"actualCoreMoveAnimReadAdmissionTested\":true,\"backendNoEntryProved\":" << (refused.native.notEntered ? "true" : "false")
               << ",\"nativeEngineCodeExecuted\":false,\"nativeMovementVerified\":false,\"physicalConfirmationRequired\":true,\"clockFields\":96"
               << ",\"fullMtaReady\":false,\"gameLaunched\":false}\n";
        Check(result.good(), "save actual Core route evidence and honest non-game refusal");
        std::printf("Passed %u actual MTA frontend startup host checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Actual Core movement host failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}

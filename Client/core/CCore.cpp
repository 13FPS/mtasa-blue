/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CCore.cpp
 *  PURPOSE:     Base core class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <game/CGame.h>
#include <game/CSettings.h>
#include <Accctrl.h>
#include <Aclapi.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <memory>
#include "Userenv.h"  // This will enable SharedUtil::ExpandEnvString
#define ALLOC_STATS_MODULE_NAME "core"
#include "SharedUtil.hpp"
#include <clocale>
#include "DXHook/CDirect3DHook9.h"
#include "DXHook/CDirect3DHookManager.h"
#include "CTimingCheckpoints.hpp"
#include "CModelCacheManager.h"
#include <SharedUtil.Detours.h>
#include <ServerBrowser/CServerCache.h>
#include "CDiscordRichPresence.h"
#include "CSteamClient.h"
#include "CCrashDumpWriter.h"
#include "FastFailCrashHandler/WerCrashHandler.h"
#include "DefinitiveStartupState.h"
#include "DefinitiveInputReference.h"
#include "DefinitiveClockReference.h"
#include "DefinitiveApplicationFrameReference.h"
#include "DefinitiveStartupObservationReference.h"
#include "DefinitiveClockSetReference.h"
#include "DefinitiveMovementReference.h"
#include "DefinitiveRenderReference.h"
#include "DefinitiveRenderEndpointLog.h"
#include "DefinitiveRenderSetupLog.h"
#include "DefinitiveRenderRootsLog.h"
#include "DefinitiveGuiStartup.h"
#include "DefinitiveGuiStartupReference.h"
#include "DefinitiveGuiSurface.h"
#include "DefinitiveGuiSurfaceReference.h"
#include "DefinitiveGuiFrontendReference.h"
#include "DefinitiveGuiFrontendInputReference.h"
#include "DefinitiveFrontendBindings.h"
#include "DefinitivePersistentFrontend.h"
#include "FrontendCursorLeaseOwner.h"
#include "NetworkModuleStartup.h"
#include <type_traits>

using SharedUtil::CalcMTASAPath;
using namespace std;

namespace fs = std::filesystem;

// Set to true to enable the freeze watchdog (monitors main thread responsiveness)
// Do NOT enable it unless you run a QA testing cycle (see commit desc: 3e54dcb2742bccf0319b9552b2ed5a2c0a012425)
constexpr bool  bFreezeWatchdogEnabled = false;
constexpr DWORD uiFreezeWatchdogTimeoutSeconds = 30;  // Player won't be patient beyond this; we get no info

static float fTest = 1;

extern CCore* g_pCore;
bool          g_bBoundsChecker = false;
SString       g_strJingleBells;

extern fs::path g_gtaDirectory;
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
extern fs::path                        g_mtaDirectory;
extern "C" __declspec(dllexport) void* MTA_CreateDEFrontendSession(std::uint32_t, CGUI*);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_PulseDEFrontendSession(void*, std::int32_t, std::int32_t, std::uint32_t);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDEFrontendSession(void*, std::uint32_t*, std::uint32_t);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ShutdownDEFrontendSession(void*);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReleaseDEFrontendSession(void*);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_EnableDEFrontendConsole(void*, std::uint32_t);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDEFrontendConsole(void*, std::uint32_t*, std::uint32_t);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ProcessDEFrontendConsoleMessage(void*, std::uint32_t, std::uintptr_t, std::intptr_t,
                                                                                                std::uint32_t*);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_EnableDEFrontendBindings(void*, std::uint32_t);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDEFrontendBindings(void*, std::uint32_t*, std::uint32_t);
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ProduceDEFrontendNeutralInput(void*, mta::game_de::MtaInputState*, std::uint32_t);

namespace
{
    struct InputReferenceXml
    {
        HMODULE   module = nullptr;
        CXML*     xml = nullptr;
        CXMLFile* file = nullptr;
        void (*release)() = nullptr;
        bool retainModule = false;

        ~InputReferenceXml() { Close(); }
        bool Close(bool retainOnUnloadFailure = false) noexcept
        {
            bool clean = true;
            // File wrappers must die before their factory and DLL. Attempt each
            // release independently so one failure cannot skip the remaining owner.
            if (file)
            {
                auto* owned = file;
                file = nullptr;
                try
                {
                    xml->DeleteXML(owned);
                }
                catch (...)
                {
                    clean = false;
                    retainModule = true;
                }
            }
            if (xml)
            {
                xml = nullptr;
                try
                {
                    release();
                }
                catch (...)
                {
                    clean = false;
                    retainModule = true;
                }
            }
            if (module && !retainModule)
            {
                const auto owned = module;
                if (!retainOnUnloadFailure)
                    module = nullptr;
                if (!FreeLibrary(owned))
                    clean = false;
                else
                    module = nullptr;
            }
            // A failed foreign factory release may leave live XML objects. Keep
            // their code loaded and report failure instead of inventing cleanup.
            return clean && !retainModule;
        }

        bool Open()
        {
            // XML owns a singleton interface. A pre-existing owner cannot safely
            // be borrowed or released by this short-lived Core diagnostic.
            if (!g_mtaDirectory.is_absolute() || GetModuleHandleW(L"xmll.dll"))
                return false;
            module = LoadLibraryExW((g_mtaDirectory / L"xmll.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module)
                return false;
            using Initialize = CXML* (*)(const char*);
            const auto initialize = reinterpret_cast<Initialize>(GetProcAddress(module, "InitXMLInterface"));
            release = reinterpret_cast<void (*)()>(GetProcAddress(module, "ReleaseXMLInterface"));
            if (!initialize || !release)
                return false;
            // nullptr explicitly skips XML save-file recovery, which may rename
            // files. This session parses existing configuration without writing.
            xml = initialize(nullptr);
            if (!xml)
                return false;
            const auto configPath = ToUTF8((g_mtaDirectory / MTA_CONFIG_PATH).wstring());
            file = xml->CreateXML(configPath.c_str(), false, true);
            return file && file->Parse();
        }
    };

    bool InputReferenceFocused() noexcept
    {
        DWORD      process = 0;
        const auto foreground = GetForegroundWindow();
        return foreground && GetWindowThreadProcessId(foreground, &process) && process == GetCurrentProcessId();
    }

    template <std::size_t N>
    void CopyInputReferenceController(std::array<std::uint32_t, N>& snapshot, std::uint32_t first, const CControllerState& state) noexcept
    {
        static_assert(sizeof(CControllerState) == 24 * sizeof(std::int16_t));
        std::array<std::int16_t, 24> fields{};
        std::memcpy(fields.data(), &state, sizeof(state));
        for (std::size_t index = 0; index < fields.size(); ++index)
            snapshot[first + index] = static_cast<std::uint32_t>(static_cast<std::int32_t>(fields[index]));
    }

    bool CopyMovementReferenceState(CGameAdapterModule& adapter, DefinitiveMovementReference::Snapshot& snapshot)
    {
        mta::game_de::NativeMovementTrialState state;
        if (!adapter.GetNativeMovementTrialState(state))
            return false;
        const std::array<std::uint64_t, 12> values = {
            state.bound,         state.active,          state.callbackCount,          state.scopedSteps,     state.routedQueries, state.neutralQueries,
            state.restoredSteps, state.restoreFailures, state.foreignThreadCallbacks, state.nestedCallbacks, state.failureReason, state.lastMoveState};
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            snapshot[DefinitiveMovementReference::NativeMetrics + index * 2] = static_cast<std::uint32_t>(values[index]);
            snapshot[DefinitiveMovementReference::NativeMetrics + index * 2 + 1] = static_cast<std::uint32_t>(values[index] >> 32);
        }
        snapshot[DefinitiveMovementReference::BackendPinnedExpected] = state.modulePinned ? 1u : 0u;
        return true;
    }

    bool CopyRenderReferenceState(CGameAdapterModule& adapter, DefinitiveRenderReference::Snapshot& snapshot)
    {
        using namespace DefinitiveRenderReference;
        mta::game_de::RenderObservationState state;
        if (!adapter.GetNativeRenderObservationState(state))
            return false;
        CopyNativeState(snapshot, state);
        return true;
    }

    bool CopyRenderReferenceState(CGameAdapterModule& adapter, DefinitiveRenderReference::SnapshotV2& snapshot)
    {
        mta::game_de::RenderObservationStateV2 state;
        if (!adapter.GetNativeRenderObservationState(state))
            return false;
        DefinitiveRenderReference::CopyNativeStateV2(snapshot, state);
        return true;
    }

    bool CopyRenderReferenceState(CGameAdapterModule& adapter, DefinitiveRenderReference::SnapshotV3& snapshot)
    {
        mta::game_de::RenderObservationStateV3 state;
        if (!adapter.GetNativeRenderObservationState(state))
            return false;
        DefinitiveRenderReference::CopyNativeStateV3(snapshot, state);
        return true;
    }

    bool CopyRenderReferenceState(CGameAdapterModule& adapter, DefinitiveRenderReference::SnapshotV4& snapshot)
    {
        mta::game_de::RenderObservationStateV4 state;
        if (!adapter.GetNativeRenderObservationState(state))
            return false;
        DefinitiveRenderReference::CopyNativeStateV4(snapshot, state);
        return true;
    }

    bool CopyRenderReferenceState(CGameAdapterModule& adapter, DefinitiveRenderReference::SnapshotV5& snapshot)
    {
        mta::game_de::RenderObservationStateV5 state;
        if (!adapter.GetNativeRenderObservationState(state))
            return false;
        DefinitiveRenderReference::CopyNativeStateV5(snapshot, state);
        return true;
    }

    bool CopyRenderReferenceState(CGameAdapterModule& adapter, DefinitiveRenderReference::SnapshotV6& snapshot)
    {
        mta::game_de::RenderObservationStateV6 state;
        if (!adapter.GetNativeRenderObservationState(state))
            return false;
        DefinitiveRenderReference::CopyNativeStateV6(snapshot, state);
        return true;
    }

    void RunGuiStartupReference(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept
    {
        using namespace DefinitiveGuiStartupReference;
        struct Owner
        {
            CGameAdapterModule&               adapter;
            DefinitiveGuiStartup::Session     gui;
            mta::game_de::RenderDeviceLeaseV1 lease;
            bool                              beginAttempted = false;

            void CopyGui(Snapshot& values) noexcept
            {
                const auto result = gui.Snapshot();
                values[GuiPrepared] = result.modulePrepared;
                values[WorkloadSucceeded] = result.workloadSucceeded;
                values[WorkloadHresult] = result.hresult;
                values[WorkloadGuiStatus] = result.guiStatus;
                values[NodeCount] = result.nodeCount;
                values[AdapterLuidLow] = result.deviceLuidLow;
                values[AdapterLuidHigh] = result.deviceLuidHigh;
                values[TargetFormat] = result.targetFormat;
                values[Width] = result.width;
                values[Height] = result.height;
                values[QueueCreated] = result.queueCreated;
                values[TargetCreated] = result.targetCreated;
                values[RenderSubmitted] = result.guiFrames;
                SetPair(values, FenceSignaledLow, result.fenceValue);
                SetPair(values, FenceCompletedLow, result.completedFenceValue);
                values[ReadbackVerified] = result.readbackComplete && result.workloadSucceeded;
                values[ReadbackChangedPixels] = result.changedPixels;
                values[GuiCleanupComplete] = result.cleanupComplete;
                values[OwnQueueSubmissions] = result.ownQueueSubmissions;
                values[GuiStage] = result.stage;
                values[GuiFailureStage] = result.failureStage;
                values[ModuleRetained] = result.moduleRetained;
                values[SessionRetained] = result.sessionRetained;
                values[DeviceRetained] = result.deviceRetained || lease.device;
                values[DeviceRemoved] = result.deviceRemoved;
                values[DeviceReleaseFault] = result.deviceReleaseFault;
                values[BackgroundVerified] = result.backgroundVerified;
                values[TextureVerified] = result.textureVerified;
                values[OpaqueAlphaVerified] = result.opaqueAlphaVerified;
                values[WidgetsCreated] = result.widgetsCreated;
            }
            bool Prepare(Snapshot& values)
            {
                // Resolve/load cgui and configure its existing asset resolver
                // while every peer still runs. A failed Begin may retain a peer
                // stopped inside the loader or allocator until End can retry.
                const auto root = g_mtaDirectory.wstring();
                const auto path = (g_mtaDirectory / L"cgui.dll").wstring();
                const bool ready = gui.Prepare(path.c_str(), root.c_str()) == DefinitiveGuiStartup::Status::Ok;
                CopyGui(values);
                return ready;
            }
            bool Begin(Snapshot&)
            {
                beginAttempted = true;
                return adapter.BeginNativeRenderDeviceObservation();
            }
            bool ReadNative(Snapshot& values)
            {
                mta::game_de::RenderObservationStateV6 state;
                if (!adapter.GetNativeRenderObservationState(state))
                    return false;
                const auto& base = state.base.base.base.base.base;
                values[NativeFailureReason] = base.failureReason;
                values[ObserverPendingCleanup] = base.pendingCleanup;
                values[BackendPinnedExpected] = base.modulePinned ? 1u : 0u;
                return true;
            }
            bool TryDevice(Snapshot& values)
            {
                if (!adapter.TakeNativeRenderDeviceLease(lease))
                    return false;
                SetPair(values, LeaseGenerationLow, lease.generation);
                SetPair(values, AcceptedSnapshotsLow, lease.acceptedSnapshots);
                SetPair(values, DispatchEntriesLow, lease.dispatchEntries);
                values[SourceProfile] = lease.sourceProfileId;
                values[LeaseMetadataValid] = lease.size == sizeof(lease) && lease.version == mta::game_de::RenderDeviceLeaseVersionV1 && lease.device &&
                                             lease.generation && lease.acceptedSnapshots && lease.dispatchEntries && lease.sourceProfileId == 2 &&
                                             !lease.reserved;
                values[DeviceRetained] = 1;
                return true;
            }
            bool End(Snapshot& values)
            {
                const bool ended = adapter.EndNativeRenderObservation();
                if (beginAttempted)
                    ReadNative(values);
                return ended;
            }
            bool Attach() noexcept
            {
                if (!lease.device)
                    return true;
                // Attach stores the transferred reference without COM. If it
                // refuses, keep the reference on this worker for cleanup retry.
                if (gui.AttachDevice(lease.device) != DefinitiveGuiStartup::Status::Ok)
                    return false;
                lease.device = nullptr;
                return true;
            }
            bool StartGpu(Snapshot& values) noexcept
            {
                const bool started = Attach() && gui.Start() == DefinitiveGuiStartup::Status::Ok;
                CopyGui(values);
                return started;
            }
            Outcome PollGpu(Snapshot& values) noexcept
            {
                const auto status = gui.Poll();
                CopyGui(values);
                return status == DefinitiveGuiStartup::Status::Busy                              ? Outcome::Pending
                       : status == DefinitiveGuiStartup::Status::Ok && values[WorkloadSucceeded] ? Outcome::Passed
                                                                                                 : Outcome::Failed;
            }
            bool ShutdownGpu(Snapshot& values) noexcept
            {
                if (!Attach())
                    return false;
                const auto status = gui.Shutdown();
                CopyGui(values);
                values[GuiShutdownStatus] = static_cast<std::uint32_t>(status);
                return status == DefinitiveGuiStartup::Status::Ok && values[GuiCleanupComplete];
            }
        } owner{adapter};
        struct Clock
        {
            std::uint64_t Now() const noexcept { return static_cast<std::uint64_t>(GetTickCount64_()); }
            void          Wait(std::uint32_t milliseconds) const noexcept { Sleep(milliseconds); }
        } clock;
        const auto runMs = clock.Now();
        const auto result = Run(owner, clock, request);
        try
        {
            // Run returns only after native hooks/holds and GUI/GPU leases have
            // drained. Logging cannot take a shared allocator or file lock while
            // End still owns suspended peers, and cannot change cleanup success.
            std::string line = "{\"schema\":\"mta_core_gui_startup_v1\",\"pid\":" + std::to_string(GetCurrentProcessId()) +
                               ",\"runMs\":" + std::to_string(runMs) + ",\"values\":[";
            for (std::size_t index = 0; index < result.size(); ++index)
            {
                if (index)
                    line += ',';
                line += std::to_string(result[index]);
            }
            line += "]}\n";
            const auto path = CalcMTASAPath("mta/logs/gui-startup.log");
            SharedUtil::MakeSureDirExists(path);
            SharedUtil::FileAppend(path, line.data(), static_cast<unsigned long>(line.size()));
        }
        catch (...)
        {
        }
    }

    void RunGuiSurfaceReference(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept
    {
        using namespace DefinitiveGuiStartupReference;
        using SurfaceSnapshot = DefinitiveGuiSurfaceReference::Snapshot;
        using SurfaceStatus = DefinitiveGuiSurface::Status;
        struct Owner
        {
            CGameAdapterModule&                     adapter;
            DefinitiveGuiSurface::Session           gui;
            mta::game_de::RenderDeviceWindowLeaseV2 lease;
            bool                                    beginAttempted = false;
            std::uint64_t                           displayStarted = 0;

            void CopyGui(SurfaceSnapshot& values) noexcept
            {
                const auto s = gui.Snapshot();
                values[GuiPrepared] = s.modulePrepared;
                values[WorkloadHresult] = s.hresult;
                values[WorkloadGuiStatus] = s.guiStatus;
                values[NodeCount] = s.nodeCount;
                values[AdapterLuidLow] = s.deviceLuidLow;
                values[AdapterLuidHigh] = s.deviceLuidHigh;
                values[TargetFormat] = s.targetFormat;
                values[Width] = s.width;
                values[Height] = s.height;
                values[QueueCreated] = s.queueCreated;
                values[TargetCreated] = s.swapchainCreated;
                values[RenderSubmitted] = s.guiFrames ? 1u : 0u;
                SetPair(values, FenceSignaledLow, s.fenceValue);
                SetPair(values, FenceCompletedLow, s.completedFenceValue);
                values[ReadbackVerified] = s.readbackComplete && s.readbackRedVerified && s.transparentPixelVerified && s.premultipliedPixelsVerified;
                values[ReadbackChangedPixels] = s.readbackPixels;
                values[GuiCleanupComplete] = s.cleanupComplete;
                values[OwnQueueSubmissions] = s.ownQueueSubmissions;
                values[GuiStage] = s.stage;
                values[GuiFailureStage] = s.failureStage;
                values[ModuleRetained] = s.moduleRetained;
                values[SessionRetained] = s.sessionRetained;
                values[DeviceRetained] = s.deviceRetained || lease.device;
                values[DeviceRemoved] = s.deviceRemoved;
                values[DeviceReleaseFault] = s.releaseFault;
                values[BackgroundVerified] = s.transparentPixelVerified;
                values[TextureVerified] = s.readbackRedVerified;
                values[OpaqueAlphaVerified] = s.premultipliedPixelsVerified;
                values[WidgetsCreated] = s.widgetsCreated;
                using namespace DefinitiveGuiSurfaceReference;
                values[ParentValidated] = s.parentValidated;
                values[ParentWindowed] = s.parentWindowed;
                values[ParentAlive] = s.parentAlive;
                values[ChildCreated] = s.childCreated;
                values[ChildShown] = s.childShown;
                values[CompositionCreated] = s.compositionCreated;
                values[SwapchainCreated] = s.swapchainCreated;
                values[PresentCalls] = s.presentCalls;
                values[PresentSucceeded] = s.presentSucceeded;
                values[PresentBusy] = s.presentBusy;
                values[PresentHresult] = s.presentHresult;
                values[GuiFrames] = s.guiFrames;
                values[ResizeCount] = s.resizeCount;
                values[ResizePending] = s.resizePending;
                values[ParentDestroyed] = s.parentDestroyed;
                values[StopRequested] = s.stopRequested;
                values[ChildRetained] = s.childRetained;
                values[QueueRetained] = s.queueRetained;
                values[SurfaceRetained] = s.surfaceRetained;
                values[WindowCleanupComplete] = s.windowCleanupComplete;
                values[CompositionCleanupComplete] = s.compositionCleanupComplete;
                values[ClassUnregistered] = s.classUnregistered;
                values[OwnerThreadId] = s.ownerThreadId;
                values[ParentThreadId] = s.parentThreadId;
                values[AlphaMode] = s.alphaMode;
                values[SignalAttempts] = s.signalAttempts;
                values[DisplayDuration] = DisplayDurationMs;
                values[PrivateSurface] = 1;
            }
            bool Prepare(SurfaceSnapshot& values)
            {
                // As with offscreen startup, resolve cgui before Begin can hold
                // peers. No Win32 surface or COM work runs during retained holds.
                const auto root = g_mtaDirectory.wstring();
                const auto path = (g_mtaDirectory / L"cgui.dll").wstring();
                const bool prepared = gui.Prepare(path.c_str(), root.c_str()) == SurfaceStatus::Ok;
                CopyGui(values);
                return prepared;
            }
            bool Begin(SurfaceSnapshot&)
            {
                beginAttempted = true;
                return adapter.BeginNativeRenderDeviceWindowObservation();
            }
            bool ReadNative(SurfaceSnapshot& values)
            {
                mta::game_de::RenderObservationStateV6 state;
                if (!adapter.GetNativeRenderObservationState(state))
                    return false;
                const auto& base = state.base.base.base.base.base;
                values[NativeFailureReason] = base.failureReason;
                values[ObserverPendingCleanup] = base.pendingCleanup;
                values[BackendPinnedExpected] = base.modulePinned ? 1u : 0u;
                return true;
            }
            bool TryDevice(SurfaceSnapshot& values)
            {
                if (!adapter.TakeNativeRenderDeviceWindowLease(lease))
                    return false;
                SetPair(values, LeaseGenerationLow, lease.generation);
                SetPair(values, AcceptedSnapshotsLow, lease.acceptedSnapshots);
                SetPair(values, DispatchEntriesLow, lease.dispatchEntries);
                values[SourceProfile] = lease.sourceProfileId;
                values[LeaseMetadataValid] = lease.size == sizeof(lease) && lease.version == mta::game_de::RenderDeviceWindowLeaseVersionV2 && lease.device &&
                                             lease.window && lease.windowed == 1 && lease.generation && lease.acceptedSnapshots && lease.dispatchEntries &&
                                             lease.sourceProfileId == 2 && !lease.reserved[0] && !lease.reserved[1];
                values[DeviceRetained] = 1;
                return true;
            }
            bool End(SurfaceSnapshot& values)
            {
                const bool ended = adapter.EndNativeRenderObservation();
                if (beginAttempted)
                    ReadNative(values);
                return ended;
            }
            bool Attach() noexcept
            {
                if (!lease.device)
                    return true;
                if (gui.AttachDeviceWindow(lease.device, static_cast<HWND>(lease.window), lease.windowed == 1) != SurfaceStatus::Ok)
                    return false;
                lease.device = nullptr;
                return true;
            }
            bool StartGpu(SurfaceSnapshot& values) noexcept
            {
                // Only MTA-owned window/swapchain resources are created, after
                // native End. The borrowed parent is revalidated by the helper.
                const bool started = Attach() && gui.Start() == SurfaceStatus::Ok;
                if (started)
                    displayStarted = static_cast<std::uint64_t>(GetTickCount64_());
                CopyGui(values);
                return started;
            }
            Outcome PollGpu(SurfaceSnapshot& values) noexcept
            {
                const auto status = gui.Poll();
                CopyGui(values);
                const auto s = gui.Snapshot();
                if (status == SurfaceStatus::Failed || status == SurfaceStatus::InvalidArgument || status == SurfaceStatus::WrongThread || s.parentDestroyed ||
                    !s.parentAlive || s.releaseFault || s.deviceRemoved)
                    return Outcome::Failed;
                const auto now = static_cast<std::uint64_t>(GetTickCount64_());
                if (now < displayStarted)
                    return Outcome::Failed;
                if (now - displayStarted < DefinitiveGuiSurfaceReference::DisplayDurationMs)
                    return Outcome::Pending;
                // DO_NOT_WAIT can legitimately leave a frame queued. Count those
                // retries explicitly so only successful presents prove output.
                const bool presentsAccounted = s.presentSucceeded <= s.presentCalls && s.presentBusy == s.presentCalls - s.presentSucceeded;
                return values[ReadbackVerified] && s.presentSucceeded >= 2 && presentsAccounted && !s.presentHresult && s.guiFrames >= s.presentSucceeded &&
                               s.childShown && !s.failureStage
                           ? Outcome::Passed
                           : Outcome::Failed;
            }
            bool ShutdownGpu(SurfaceSnapshot& values) noexcept
            {
                if (!Attach())
                    return false;
                const auto status = gui.Shutdown();
                CopyGui(values);
                values[GuiShutdownStatus] = static_cast<std::uint32_t>(status);
                return status == SurfaceStatus::Ok && values[GuiCleanupComplete];
            }
        } owner{adapter};
        struct Clock
        {
            std::uint64_t Now() const noexcept { return static_cast<std::uint64_t>(GetTickCount64_()); }
            void          Wait(std::uint32_t milliseconds) const noexcept { Sleep(milliseconds); }
        } clock;
        const auto runMs = clock.Now();
        const auto result = DefinitiveGuiSurfaceReference::Run(owner, clock, request);
        try
        {
            // All native holds, GUI/GPU owners and the child window have ended
            // before file I/O; failure to log cannot relabel cleanup or drawing.
            std::string line = "{\"schema\":\"mta_core_gui_surface_v1\",\"pid\":" + std::to_string(GetCurrentProcessId()) +
                               ",\"runMs\":" + std::to_string(runMs) + ",\"values\":[";
            for (std::size_t index = 0; index < result.size(); ++index)
            {
                if (index)
                    line += ',';
                line += std::to_string(result[index]);
            }
            line += "]}\n";
            const auto path = CalcMTASAPath("mta/logs/gui-surface.log");
            SharedUtil::MakeSureDirExists(path);
            SharedUtil::FileAppend(path, line.data(), static_cast<unsigned long>(line.size()));
        }
        catch (...)
        {
        }
    }

    template <bool CoreOwnedInput, bool Persistent = false>
    auto RunGuiFrontendReferenceImpl(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request,
                                     const DefinitivePersistentFrontend::RequestV1* persistent = nullptr) noexcept
    {
        static_assert(!Persistent || CoreOwnedInput);
        using namespace DefinitiveGuiStartupReference;
        using SurfaceSnapshot =
            std::conditional_t<Persistent, DefinitivePersistentFrontend::Snapshot,
                               std::conditional_t<CoreOwnedInput, DefinitiveGuiFrontendInputReference::Snapshot, DefinitiveGuiFrontendReference::Snapshot>>;
        using SurfaceStatus = DefinitiveGuiSurface::Status;
        struct Owner
        {
            CGameAdapterModule&                            adapter;
            const DefinitiveInputReference::Request&       request;
            const DefinitivePersistentFrontend::RequestV1* persistent;
            DefinitiveGuiSurface::Session                  gui;
            mta::game_de::RenderDeviceWindowLeaseV2        lease;
            bool                                           beginAttempted = false;
            std::uint64_t                                  displayStarted = 0;

            void*                                           frontend = nullptr;
            DefinitiveFrontend::Snapshot                    frontendState{};
            DefinitiveFrontendConsole::Snapshot             consoleState{};
            DefinitiveGuiFrontendReference::ControlSnapshot control{};
            bool                                            frontendAttempted = false, frontendStopped = false;
            bool                                            consoleAttempted = false;
            bool                                            controlAttempted = false, surfaceStarted = false;
            std::uint64_t                                   controlStarted = 0;
            DefinitiveFrontendBindings::Snapshot            bindingsState{};
            mta::game_de::NativeMovementTrialState          nativeState{};
            bool                                            bindingsAttempted = false;
            bool                                            nativeAttempted = false, nativeStarted = false, nativeVerified = false, nativeEnded = true;
            bool                                            nativeAcquireTimeout = false;
            std::uint64_t                                   nativeStartedAt = 0, nativeAcquireStarted = 0;
            std::uint64_t                                   nativeStoppedAt = 0;
            mta::game_de::FrontendInputLeaseStateV1         nativeLease{};
            FrontendCursorLease::Owner<CSetCursorPosHook>   cursor;
            bool                                            hostFresh = false;
            DefinitivePersistentFrontend::Stop              persistentStop = DefinitivePersistentFrontend::Stop::Running;

            bool HostFresh() noexcept
            {
                if constexpr (!Persistent)
                    return true;
                try
                {
                    hostFresh = persistent && persistent->heartbeat && persistent->heartbeat();
                }
                catch (...)
                {
                    hostFresh = false;
                }
                if (!hostFresh && persistentStop == DefinitivePersistentFrontend::Stop::Running)
                    persistentStop = DefinitivePersistentFrontend::Stop::HostLeaseExpired;
                return hostFresh;
            }

            bool ReadFrontend() noexcept
            {
                return !frontend ||
                       (MTA_ReadDEFrontendSession(frontend, frontendState.data(), DefinitiveFrontend::FieldCount) == DefinitiveFrontend::Status::Ok &&
                        (!consoleAttempted ||
                         MTA_ReadDEFrontendConsole(frontend, consoleState.data(), DefinitiveFrontendConsole::FieldCount) == DefinitiveFrontend::Status::Ok) &&
                        (!bindingsAttempted ||
                         MTA_ReadDEFrontendBindings(frontend, bindingsState.data(), DefinitiveFrontendBindings::FieldCount) == DefinitiveFrontend::Status::Ok));
            }
            static bool StartFrontend(void* context, CGUI* gui)
            {
                auto& self = *static_cast<Owner*>(context);
                // Publish partial ownership before calling the actual Core API;
                // failed menu startup still needs explicit CPU-service teardown.
                self.frontendAttempted = true;
                self.frontend = MTA_CreateDEFrontendSession(DefinitiveFrontend::ApiVersion, gui);
                if (!self.frontend || !self.ReadFrontend() || self.frontendState[DefinitiveFrontend::Phase] != 2)
                    return false;
                // Reuse the real console on the same frontend owner. Native
                // admission is established by the caller before input is shown;
                // failure retains the partial lifecycle for explicit teardown.
                self.consoleAttempted = true;
                const auto enabled = MTA_EnableDEFrontendConsole(self.frontend, DefinitiveFrontendConsole::ApiVersion);
                if (!self.ReadFrontend() || enabled != DefinitiveFrontend::Status::Ok || self.consoleState[DefinitiveFrontendConsole::Phase] != 2)
                    return false;
                if constexpr (CoreOwnedInput)
                {
                    self.bindingsAttempted = true;
                    if (MTA_EnableDEFrontendBindings(self.frontend, DefinitiveFrontendBindings::ApiVersion) != DefinitiveFrontend::Status::Ok ||
                        !self.ReadFrontend())
                        return false;
                }
                return true;
            }
            static bool PulseFrontend(void* context, std::int32_t x, std::int32_t y, bool focused)
            {
                auto& self = *static_cast<Owner*>(context);
                if constexpr (CoreOwnedInput)
                {
                    if (!self.HostFresh() || !self.NativeHealthy() || !InputReferenceFocused())
                        return false;
                }
                else if (!self.Control(DefinitiveGuiFrontendReference::ControlAction::Poll) || !self.control[DefinitiveGuiFrontendReference::Fresh] ||
                         self.control[DefinitiveGuiFrontendReference::Revoked] || self.control[DefinitiveGuiFrontendReference::Fault] ||
                         !InputReferenceFocused())
                    return false;
                return self.frontend && MTA_PulseDEFrontendSession(self.frontend, x, y, focused ? 1u : 0u) == DefinitiveFrontend::Status::Ok &&
                       self.ReadFrontend();
            }
            static bool StopFrontend(void* context)
            {
                auto& self = *static_cast<Owner*>(context);
                if (self.frontend)
                {
                    const auto stopped = MTA_ShutdownDEFrontendSession(self.frontend);
                    if (!self.ReadFrontend() || stopped != DefinitiveFrontend::Status::Ok)
                        return false;
                    if (MTA_ReleaseDEFrontendSession(self.frontend) != DefinitiveFrontend::Status::Ok)
                        return false;
                    self.frontend = nullptr;
                }
                self.frontendStopped = true;
                return true;
            }
            static bool FrontendMessage(void* context, std::uint32_t message, std::uintptr_t wparam, std::intptr_t lparam, bool* handled)
            {
                auto& self = *static_cast<Owner*>(context);
                if (!handled || !self.frontend)
                    return false;
                *handled = false;
                const bool keyboard = message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ||
                                      message == WM_CHAR || message == WM_UNICHAR;
                // Shortcut admission needs the same fresh native lease as menu
                // input. Focus/capture cancellation may still reset local state
                // after focus leaves; it never invokes an engine command.
                if (keyboard)
                {
                    if constexpr (CoreOwnedInput)
                    {
                        if (!self.nativeVerified || !self.HostFresh() || !self.NativeHealthy() || !InputReferenceFocused())
                            return false;
                    }
                    else if (!self.Control(DefinitiveGuiFrontendReference::ControlAction::Poll) || !self.control[DefinitiveGuiFrontendReference::Fresh] ||
                             self.control[DefinitiveGuiFrontendReference::Revoked] || self.control[DefinitiveGuiFrontendReference::Fault] ||
                             !InputReferenceFocused())
                        return false;
                }
                std::uint32_t consumed = 0;
                if (MTA_ProcessDEFrontendConsoleMessage(self.frontend, message, wparam, lparam, &consumed) != DefinitiveFrontend::Status::Ok || consumed > 1)
                    return false;
                *handled = consumed != 0;
                return true;
            }
            bool Control(DefinitiveGuiFrontendReference::ControlAction action) noexcept
            {
                try
                {
                    auto next = control;
                    if (!request.controlLease ||
                        !request.controlLease(static_cast<std::uint32_t>(action), next.data(), static_cast<std::uint32_t>(next.size())))
                        return false;
                    if (std::any_of(next.begin() + 1, next.end(), [](auto value) { return value > 1; }) ||
                        (control[DefinitiveGuiFrontendReference::Generation] &&
                         next[DefinitiveGuiFrontendReference::Generation] != control[DefinitiveGuiFrontendReference::Generation]))
                        return false;
                    control = next;
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            bool PublishNeutral() noexcept
            {
                if (!frontend || !bindingsAttempted || !HostFresh())
                    return false;
                mta::game_de::MtaInputState expected, actual;
                if (MTA_ProduceDEFrontendNeutralInput(frontend, &expected, sizeof(expected)) != DefinitiveFrontend::Status::Ok || !adapter.Pulse() ||
                    !adapter.SubmitControlActions({0, {}}) || !adapter.GetMtaInputState(actual))
                    return false;
                // Both sides reuse MTA's canonical synthesis. The native path
                // receives copied values only; no script owns a restore flag.
                const mta::game_de::MtaInputState zero;
                const bool neutral = std::memcmp(&expected, &zero, sizeof(zero)) == 0 && std::memcmp(&actual, &zero, sizeof(zero)) == 0 && ReadFrontend();
                if constexpr (Persistent)
                {
                    // Only the optional neutral-only lease can outlive a trial.
                    // Ordinary canonical input publication does not renew it.
                    if (neutral && nativeStarted && !nativeEnded)
                        return adapter.RenewFrontendInputLease(nativeLease.generation);
                }
                return neutral;
            }
            bool NativeHealthy() noexcept
            {
                if constexpr (Persistent)
                {
                    if (!adapter.ReadFrontendInputLease(nativeLease) || !nativeLease.active || nativeLease.revoked)
                    {
                        if (persistentStop == DefinitivePersistentFrontend::Stop::Running)
                            persistentStop = nativeLease.reason == static_cast<std::uint32_t>(mta::game_de::FrontendInputLeaseReason::FocusLost)
                                                 ? DefinitivePersistentFrontend::Stop::FocusLost
                                                 : DefinitivePersistentFrontend::Stop::Fault;
                        return false;
                    }
                }
                return nativeStarted && !nativeEnded && adapter.GetNativeMovementTrialState(nativeState) && nativeState.bound && nativeState.active &&
                       nativeState.modulePinned && !nativeState.failureReason && !nativeState.restoreFailures && !nativeState.nestedCallbacks &&
                       nativeState.restoredSteps <= nativeState.scopedSteps;
            }
            void CopyGui(SurfaceSnapshot& values) noexcept
            {
                const auto s = gui.Snapshot();
                values[GuiPrepared] = s.modulePrepared;
                values[WorkloadHresult] = s.hresult;
                values[WorkloadGuiStatus] = s.guiStatus;
                values[NodeCount] = s.nodeCount;
                values[AdapterLuidLow] = s.deviceLuidLow;
                values[AdapterLuidHigh] = s.deviceLuidHigh;
                values[TargetFormat] = s.targetFormat;
                values[Width] = s.width;
                values[Height] = s.height;
                values[QueueCreated] = s.queueCreated;
                values[TargetCreated] = s.swapchainCreated;
                values[RenderSubmitted] = s.guiFrames ? 1u : 0u;
                SetPair(values, FenceSignaledLow, s.fenceValue);
                SetPair(values, FenceCompletedLow, s.completedFenceValue);
                values[ReadbackVerified] = s.readbackComplete && s.readbackPixels > 10000 && s.premultipliedPixelsVerified;
                values[ReadbackChangedPixels] = s.readbackPixels;
                values[GuiCleanupComplete] = s.cleanupComplete;
                values[OwnQueueSubmissions] = s.ownQueueSubmissions;
                values[GuiStage] = s.stage;
                values[GuiFailureStage] = s.failureStage;
                values[ModuleRetained] = s.moduleRetained;
                values[SessionRetained] = s.sessionRetained;
                values[DeviceRetained] = s.deviceRetained || lease.device;
                values[DeviceRemoved] = s.deviceRemoved;
                values[DeviceReleaseFault] = s.releaseFault;
                values[BackgroundVerified] = 0;
                values[TextureVerified] = 0;
                values[OpaqueAlphaVerified] = s.premultipliedPixelsVerified;
                values[WidgetsCreated] = s.widgetsCreated;
                using namespace DefinitiveGuiSurfaceReference;
                values[ParentValidated] = s.parentValidated;
                values[ParentWindowed] = s.parentWindowed;
                values[ParentAlive] = s.parentAlive;
                values[ChildCreated] = s.childCreated;
                values[ChildShown] = s.childShown;
                values[CompositionCreated] = s.compositionCreated;
                values[SwapchainCreated] = s.swapchainCreated;
                values[PresentCalls] = s.presentCalls;
                values[PresentSucceeded] = s.presentSucceeded;
                values[PresentBusy] = s.presentBusy;
                values[PresentHresult] = s.presentHresult;
                values[GuiFrames] = s.guiFrames;
                values[ResizeCount] = s.resizeCount;
                values[ResizePending] = s.resizePending;
                values[ParentDestroyed] = s.parentDestroyed;
                values[StopRequested] = s.stopRequested;
                values[ChildRetained] = s.childRetained;
                values[QueueRetained] = s.queueRetained;
                values[SurfaceRetained] = s.surfaceRetained;
                values[WindowCleanupComplete] = s.windowCleanupComplete;
                values[CompositionCleanupComplete] = s.compositionCleanupComplete;
                values[ClassUnregistered] = s.classUnregistered;
                values[OwnerThreadId] = s.ownerThreadId;
                values[ParentThreadId] = s.parentThreadId;
                values[AlphaMode] = s.alphaMode;
                values[SignalAttempts] = s.signalAttempts;
                values[DisplayDuration] = Persistent       ? 0u
                                          : CoreOwnedInput ? DefinitiveGuiFrontendInputReference::DisplayDurationMs
                                                           : DefinitiveGuiFrontendReference::DisplayDurationMs;
                values[PrivateSurface] = 1;
                values[GuiStarted] = s.sessionCreated;
                ReadFrontend();
                std::copy(frontendState.begin(), frontendState.end(), values.begin() + DefinitiveGuiFrontendReference::Frontend);
                const auto input = gui.InteractionSnapshot();
                if constexpr (CoreOwnedInput)
                {
                    using namespace DefinitiveGuiFrontendInputReference;
                    std::copy(consoleState.begin(), consoleState.end(), values.begin() + Console);
                    std::copy(bindingsState.begin(), bindingsState.end(), values.begin() + Bindings);
                    values[InteractionVersion] = input.version;
                    values[InteractionEnabled] = input.enabled;
                    values[InteractionFocused] = input.focused;
                    values[FocusAcquired] = input.focusAcquired;
                    values[FocusLost] = input.focusLost;
                    DefinitiveGuiFrontendInputReference::SetPair(values, InputEventsLow, input.events);
                    DefinitiveGuiFrontendInputReference::SetPair(values, SyntheticReleasesLow, input.syntheticReleases);
                    values[PressedKeys] = input.pressedKeys;
                    values[PressedButtons] = input.pressedButtons;
                    values[InputDetached] = input.inputDetached;
                    values[InputFailed] = input.inputFailed || cursor.Fault();
                    values[GuiCloseRequested] = input.closeRequested;
                    const std::uint64_t metrics[]{nativeState.bound,           nativeState.active,          nativeState.callbackCount,
                                                  nativeState.scopedSteps,     nativeState.routedQueries,   nativeState.neutralQueries,
                                                  nativeState.restoredSteps,   nativeState.restoreFailures, nativeState.foreignThreadCallbacks,
                                                  nativeState.nestedCallbacks, nativeState.failureReason,   nativeState.lastMoveState};
                    for (std::size_t index = 0; index < std::size(metrics); ++index)
                    {
                        values[NativeMetrics + index * 2] = static_cast<std::uint32_t>(metrics[index]);
                        values[NativeMetrics + index * 2 + 1] = static_cast<std::uint32_t>(metrics[index] >> 32);
                    }
                    values[NativeModulePinned] = nativeState.modulePinned;
                    values[NativeStarted] = nativeStarted;
                    values[NativeVerified] = nativeVerified;
                    values[NativeEnded] = nativeEnded;
                    values[NativeCleanupPending] = nativeAttempted && !nativeEnded;
                    const auto now = static_cast<std::uint64_t>(GetTickCount64_());
                    values[NativeAcquireElapsed] = nativeAcquireStarted && now >= nativeAcquireStarted
                                                       ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - nativeAcquireStarted, NativeAcquireMs))
                                                       : 0;
                    values[NativeAcquireTimeout] = nativeAcquireTimeout;
                    const auto nativeActiveUntil = Persistent && nativeStoppedAt ? nativeStoppedAt : now;
                    values[NativeActiveMs] = nativeStartedAt && nativeActiveUntil >= nativeStartedAt
                                                 ? static_cast<std::uint32_t>(std::min<std::uint64_t>(nativeActiveUntil - nativeStartedAt, UINT32_MAX))
                                                 : 0;
                    values[NativeOwnerRetained] = nativeAttempted && !nativeEnded;
                    // These counters prove only local scoped pad ownership.
                    // Camera queries outside ProcessControl need a live test.
                    values[CameraCaptureVerified] = values[FullEngineInputReady] = 0;
                }
                if constexpr (Persistent)
                {
                    using namespace DefinitivePersistentFrontend;
                    HostFresh();
                    try
                    {
                        if (persistentStop == DefinitivePersistentFrontend::Stop::Running && request.cancelled())
                            persistentStop = DefinitivePersistentFrontend::Stop::Cancelled;
                    }
                    catch (...)
                    {
                        persistentStop = DefinitivePersistentFrontend::Stop::Fault;
                    }
                    values[PersistentVersion] = DefinitivePersistentFrontend::ApiVersion;
                    const auto pair = [&](DefinitivePersistentFrontend::Field field, std::uint64_t value)
                    {
                        values[field] = static_cast<std::uint32_t>(value);
                        values[field + 1] = static_cast<std::uint32_t>(value >> 32);
                    };
                    pair(SessionGenerationLow, persistent->generation);
                    pair(NativeLeaseGenerationLow, nativeLease.generation);
                    pair(NativeLeaseRenewalsLow, nativeLease.renewals);
                    values[HostLeaseFresh] = hostFresh;
                    values[NativeLeaseActive] = nativeLease.active;
                    values[NativeLeaseRevoked] = nativeLease.revoked;
                    values[NativeLeaseReason] = nativeLease.reason;
                    values[HostHeartbeatBudgetMs] = HostHeartbeatMs;
                    values[NativeHeartbeatBudgetMs] = mta::game_de::FrontendInputLeaseLifetimeMsV1;
                    values[DefinitivePersistentFrontend::StopReason] = static_cast<std::uint32_t>(persistentStop);
                }
                else if constexpr (!CoreOwnedInput)
                {
                    using namespace DefinitiveGuiFrontendReference;
                    values[InteractionVersion] = input.version;
                    values[InteractionEnabled] = input.enabled;
                    values[InteractionFocused] = input.focused;
                    values[FocusAcquired] = input.focusAcquired;
                    values[FocusLost] = input.focusLost;
                    SetPair(values, InputEventsLow, input.events);
                    SetPair(values, SyntheticReleasesLow, input.syntheticReleases);
                    values[PressedKeys] = input.pressedKeys;
                    values[PressedButtons] = input.pressedButtons;
                    values[InputDetached] = input.inputDetached;
                    values[InputFailed] = input.inputFailed;
                    values[GuiCloseRequested] = input.closeRequested;
                    std::copy(control.begin(), control.end(), values.begin() + ControlGeneration);
                    const auto now = static_cast<std::uint64_t>(GetTickCount64_());
                    values[ControlWaitingMs] = !surfaceStarted && controlStarted && now >= controlStarted
                                                   ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - controlStarted, UINT32_MAX))
                                                   : 0;
                    values[ControlExpiryMs] = HeartbeatExpiryMs;
                    values[ActiveDuration] =
                        surfaceStarted && now >= displayStarted ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - displayStarted, UINT32_MAX)) : 0;
                    values[FrontendStarted] = surfaceStarted ? 1u : 0u;
                    values[FrontendStopped] = frontendStopped ? 1u : 0u;
                    // Native bool readback establishes the control command only.
                    // Mouse-camera exclusion remains a separate human test gate.
                    values[CameraCaptureVerified] = values[FullEngineInputReady] = 0;
                }
            }
            bool Prepare(SurfaceSnapshot& values)
            {
                // As with offscreen startup, resolve cgui before Begin can hold
                // peers. No Win32 surface or COM work runs during retained holds.
                const auto root = g_mtaDirectory.wstring();
                const auto path = (g_mtaDirectory / L"cgui.dll").wstring();
                const bool prepared = gui.Prepare(path.c_str(), root.c_str()) == SurfaceStatus::Ok;
                CopyGui(values);
                return prepared;
            }
            bool Begin(SurfaceSnapshot&)
            {
                beginAttempted = true;
                return adapter.BeginNativeRenderDeviceWindowObservation();
            }
            bool ReadNative(SurfaceSnapshot& values)
            {
                mta::game_de::RenderObservationStateV6 state;
                if (!adapter.GetNativeRenderObservationState(state))
                    return false;
                const auto& base = state.base.base.base.base.base;
                values[NativeFailureReason] = base.failureReason;
                values[ObserverPendingCleanup] = base.pendingCleanup;
                values[BackendPinnedExpected] = base.modulePinned ? 1u : 0u;
                return true;
            }
            bool TryDevice(SurfaceSnapshot& values)
            {
                if (!adapter.TakeNativeRenderDeviceWindowLease(lease))
                    return false;
                SetPair(values, LeaseGenerationLow, lease.generation);
                SetPair(values, AcceptedSnapshotsLow, lease.acceptedSnapshots);
                SetPair(values, DispatchEntriesLow, lease.dispatchEntries);
                values[SourceProfile] = lease.sourceProfileId;
                values[LeaseMetadataValid] = lease.size == sizeof(lease) && lease.version == mta::game_de::RenderDeviceWindowLeaseVersionV2 && lease.device &&
                                             lease.window && lease.windowed == 1 && lease.generation && lease.acceptedSnapshots && lease.dispatchEntries &&
                                             lease.sourceProfileId == 2 && !lease.reserved[0] && !lease.reserved[1];
                values[DeviceRetained] = 1;
                return true;
            }
            bool End(SurfaceSnapshot& values)
            {
                const bool ended = adapter.EndNativeRenderObservation();
                if (beginAttempted)
                    ReadNative(values);
                return ended;
            }
            bool Attach() noexcept
            {
                if (!lease.device)
                    return true;
                if (gui.AttachDeviceWindow(lease.device, static_cast<HWND>(lease.window), lease.windowed == 1) != SurfaceStatus::Ok)
                    return false;
                lease.device = nullptr;
                return true;
            }
            bool StartGpu(SurfaceSnapshot& values) noexcept
            {
                if (!Attach())
                    return false;
                if constexpr (CoreOwnedInput)
                {
                    const DefinitiveGuiSurface::FrontendCallbacks callbacks{this, &Owner::StartFrontend, &Owner::PulseFrontend, &Owner::StopFrontend,
                                                                            &Owner::FrontendMessage};
                    // Build the actual MTA frontend and its bindings hidden.
                    // Native scope evidence, not a script flag, admits input.
                    nativeAcquireStarted = static_cast<std::uint64_t>(GetTickCount64_());
                    const bool started =
                        gui.EnableFrontend(callbacks, true) == SurfaceStatus::Ok && gui.Start() == SurfaceStatus::Ok && PublishNeutral() && PublishNeutral();
                    CopyGui(values);
                    return started;
                }
                // Native observation has ended before the script is asked to
                // disable player control. No interactive HWND exists until Ack.
                controlAttempted = true;
                controlStarted = static_cast<std::uint64_t>(GetTickCount64_());
                const bool requested = Control(DefinitiveGuiFrontendReference::ControlAction::Request);
                CopyGui(values);
                return requested && control[DefinitiveGuiFrontendReference::Generation] && control[DefinitiveGuiFrontendReference::Requested];
            }
            Outcome PollGpu(SurfaceSnapshot& values) noexcept
            {
                if constexpr (CoreOwnedInput)
                    return PollCoreInput(values);
                using namespace DefinitiveGuiFrontendReference;
                if (!Control(ControlAction::Poll) || control[Revoked] || control[Fault] || !InputReferenceFocused())
                {
                    CopyGui(values);
                    return Outcome::Failed;
                }
                const auto now = static_cast<std::uint64_t>(GetTickCount64_());
                if (!surfaceStarted)
                {
                    if (now < controlStarted || now - controlStarted >= ControlAcquireMs)
                    {
                        values[ControlTimeout] = 1;
                        CopyGui(values);
                        return Outcome::Failed;
                    }
                    if (!control[Acknowledged] || !control[Fresh])
                    {
                        CopyGui(values);
                        return Outcome::Pending;
                    }
                    const DefinitiveGuiSurface::FrontendCallbacks callbacks{this, &Owner::StartFrontend, &Owner::PulseFrontend, &Owner::StopFrontend,
                                                                            &Owner::FrontendMessage};
                    if (gui.EnableFrontend(callbacks) != SurfaceStatus::Ok || gui.Start() != SurfaceStatus::Ok)
                    {
                        CopyGui(values);
                        return Outcome::Failed;
                    }
                    surfaceStarted = true;
                    displayStarted = now;
                }
                if (!control[Fresh])
                {
                    CopyGui(values);
                    return Outcome::Failed;
                }
                const auto status = gui.Poll();
                CopyGui(values);
                const auto s = gui.Snapshot();
                const auto input = gui.InteractionSnapshot();
                if (status == SurfaceStatus::Failed || status == SurfaceStatus::InvalidArgument || status == SurfaceStatus::WrongThread || s.parentDestroyed ||
                    !s.parentAlive || s.releaseFault || s.deviceRemoved || input.inputFailed || frontendState[DefinitiveFrontend::Phase] != 2)
                    return Outcome::Failed;
                if (now < displayStarted)
                    return Outcome::Failed;
                const bool closing = input.closeRequested || frontendState[DefinitiveFrontend::CloseRequested];
                if (!closing && now - displayStarted < DisplayDurationMs)
                    return Outcome::Pending;
                const bool accounted = s.presentSucceeded <= s.presentCalls && s.presentBusy == s.presentCalls - s.presentSucceeded;
                return s.readbackComplete && s.readbackPixels > 10000 && s.premultipliedPixelsVerified && s.presentSucceeded >= 2 &&
                               frontendState[DefinitiveFrontend::Frames] >= 2 && accounted && !s.presentHresult && s.guiFrames >= s.presentSucceeded &&
                               s.childShown && !s.failureStage
                           ? Outcome::Passed
                           : Outcome::Failed;
            }
            bool ShutdownGpu(SurfaceSnapshot& values) noexcept
            {
                if (!Attach())
                    return false;
                if constexpr (CoreOwnedInput)
                    return ShutdownCoreInput(values);
                const auto status = gui.Shutdown();
                const auto input = gui.InteractionSnapshot();
                const auto surface = gui.Snapshot();
                // Input detachment, not a stalled GPU fence, authorizes the
                // script to restore controls. GUI/Core stay retained until all
                // services, GPU resources and the restoration Ack have drained.
                bool restored = !controlAttempted;
                if (controlAttempted && (input.inputDetached || (!surface.childCreated && !frontendAttempted)))
                {
                    const bool observed = Control(DefinitiveGuiFrontendReference::ControlAction::Poll);
                    restored = observed && !control[DefinitiveGuiFrontendReference::Requested];
                    if (observed && control[DefinitiveGuiFrontendReference::Requested])
                        restored = Control(DefinitiveGuiFrontendReference::ControlAction::InputDetached) && control[DefinitiveGuiFrontendReference::Restored];
                }
                CopyGui(values);
                values[GuiShutdownStatus] = static_cast<std::uint32_t>(status);
                return status == SurfaceStatus::Ok && values[GuiCleanupComplete] && restored && !frontend;
            }
            Outcome PollCoreInput(SurfaceSnapshot& values) noexcept
            {
                const auto now = static_cast<std::uint64_t>(GetTickCount64_());
                const auto failed = [&]()
                {
                    CopyGui(values);
                    return Outcome::Failed;
                };
                if (!InputReferenceFocused())
                {
                    if constexpr (Persistent)
                        persistentStop = DefinitivePersistentFrontend::Stop::FocusLost;
                    return failed();
                }
                if (!HostFresh() || now < nativeAcquireStarted || !PublishNeutral())
                    return failed();
                mta::game_de::NativeMovementEligibility eligibility;
                const bool                              eligible = adapter.GetNativeMovementEligibility(eligibility) &&
                                      eligibility.knownMask == mta::game_de::NativeMovementEligibilityMask &&
                                      eligibility.flags == mta::game_de::NativeMovementEligibilityMask && eligibility.eligible;
                if (!nativeAttempted && eligible)
                {
                    // Begin may partially install hooks. Publish ownership first
                    // so every failure still retries End before deleting binds.
                    nativeAttempted = true;
                    nativeEnded = false;
                    nativeStartedAt = now;
                    if constexpr (Persistent)
                    {
                        nativeStarted = adapter.BeginFrontendInputLease(lease.window, nativeLease);
                        // A rejected request with no published generation never
                        // acquired lease ownership. Native setup failures do
                        // publish a generation and must retry their own End.
                        if (!nativeStarted && !nativeLease.generation)
                            nativeEnded = true;
                    }
                    else
                        nativeStarted = adapter.BeginNativeMovementTrial();
                    if (!nativeStarted)
                        return failed();
                }
                if (nativeStarted && !NativeHealthy())
                    return failed();
                if (!nativeVerified)
                {
                    if (now - nativeAcquireStarted >= DefinitiveGuiFrontendInputReference::NativeAcquireMs)
                    {
                        nativeAcquireTimeout = true;
                        return failed();
                    }
                    // neutralQueries counts unknown provider actions only. The
                    // copied zero producer, routed queries and restored scopes
                    // together prove neutral input; those counts need not match.
                    if (!nativeStarted || !eligible || !nativeState.scopedSteps || !nativeState.routedQueries ||
                        nativeState.restoredSteps != nativeState.scopedSteps)
                    {
                        CopyGui(values);
                        return Outcome::Pending;
                    }
                    nativeVerified = true;
                    // Menu lifetime does not change pointer ownership. Both
                    // bounded and persistent Core input reuse MTA's cursor
                    // lease after neutral input, before visible interaction.
                    if (!cursor.AdmitAfterNeutral(nativeVerified, static_cast<HWND>(lease.window),
                                                  [&]() noexcept { return gui.AdmitFrontendInput() == SurfaceStatus::Ok; }))
                        return failed();
                    surfaceStarted = true;
                    displayStarted = now;
                }
                else if (!eligible)
                    return failed();
                // Only the same Core owner renews before drawing. The existing
                // lease still expires independently if that owner stalls.
                auto status = SurfaceStatus::Failed;
                if (!cursor.RenewBeforeDraw([&]() noexcept { status = gui.Poll(); }))
                    return failed();
                CopyGui(values);
                const auto s = gui.Snapshot();
                const auto input = gui.InteractionSnapshot();
                if (status == SurfaceStatus::Failed || status == SurfaceStatus::InvalidArgument || status == SurfaceStatus::WrongThread || s.parentDestroyed ||
                    !s.parentAlive || s.releaseFault || s.deviceRemoved || input.inputFailed || frontendState[DefinitiveFrontend::Phase] != 2 ||
                    now < displayStarted)
                    return Outcome::Failed;
                const bool closing = input.closeRequested || frontendState[DefinitiveFrontend::CloseRequested];
                if (!closing && (Persistent || now - displayStarted < DefinitiveGuiFrontendInputReference::DisplayDurationMs))
                    return Outcome::Pending;
                if constexpr (Persistent)
                {
                    persistentStop = DefinitivePersistentFrontend::Stop::GuiClosed;
                    CopyGui(values);
                }
                const bool accounted = s.presentSucceeded <= s.presentCalls && s.presentBusy == s.presentCalls - s.presentSucceeded;
                return s.readbackComplete && s.readbackPixels > 10000 && s.premultipliedPixelsVerified && s.presentSucceeded >= 2 &&
                               frontendState[DefinitiveFrontend::Frames] >= 2 && accounted && !s.presentHresult && s.guiFrames >= s.presentSucceeded &&
                               s.childShown && !s.failureStage
                           ? Outcome::Passed
                           : Outcome::Failed;
            }
            bool ShutdownCoreInput(SurfaceSnapshot& values) noexcept
            {
                if (!cursor.DisarmBeforeDetach(
                        [&]() noexcept
                        {
                            if constexpr (Persistent)
                            {
                                // Persistent duration proof ends before input detachment
                                // and native/GPU retries. Cleanup time cannot turn an
                                // early close into a successful long-running workload.
                                if (nativeStarted && !nativeStoppedAt)
                                    nativeStoppedAt = static_cast<std::uint64_t>(GetTickCount64_());
                                if (persistentStop == DefinitivePersistentFrontend::Stop::Running)
                                {
                                    // Setup, renewal or rendering can fail before the
                                    // polling path classifies its stop. A fully retired
                                    // owner must never retain a misleading Running reason.
                                    try
                                    {
                                        if (request.cancelled())
                                            persistentStop = DefinitivePersistentFrontend::Stop::Cancelled;
                                        else if (HostFresh())
                                        {
                                            if (nativeLease.generation)
                                                adapter.ReadFrontendInputLease(nativeLease);
                                            persistentStop = nativeLease.reason == static_cast<std::uint32_t>(mta::game_de::FrontendInputLeaseReason::FocusLost)
                                                                 ? DefinitivePersistentFrontend::Stop::FocusLost
                                                                 : DefinitivePersistentFrontend::Stop::Fault;
                                        }
                                    }
                                    catch (...)
                                    {
                                        persistentStop = DefinitivePersistentFrontend::Stop::Fault;
                                    }
                                }
                            }
                            // Disarm engine warps before detaching GUI messages, then
                            // restore native state before retiring the cursor and GPU.
                            return gui.DetachInput() == SurfaceStatus::Ok;
                        }))
                {
                    CopyGui(values);
                    return false;
                }
                if (nativeAttempted && !nativeEnded)
                {
                    const bool ended = Persistent ? adapter.EndFrontendInputLease(nativeLease.generation) : adapter.EndNativeMovementTrial();
                    if constexpr (Persistent)
                        adapter.ReadFrontendInputLease(nativeLease);
                    const bool observed = adapter.GetNativeMovementTrialState(nativeState);
                    nativeEnded = ended && observed && !nativeState.bound && !nativeState.active && nativeState.scopedSteps == nativeState.restoredSteps &&
                                  !nativeState.restoreFailures;
                    if (!nativeEnded)
                    {
                        CopyGui(values);
                        return false;
                    }
                }
                // A failed detach keeps the same MTA hook and owner alive for
                // a later retry in both bounded and persistent menus.
                if (!cursor.EndAfterNative(nativeEnded))
                {
                    CopyGui(values);
                    return false;
                }
                // Clear canonical history while the actual manager still owns
                // it. Failure stays visible, but never skips real CPU teardown.
                if (frontend && bindingsAttempted)
                {
                    PublishNeutral();
                    PublishNeutral();
                }
                const auto status = gui.Shutdown();
                CopyGui(values);
                values[GuiShutdownStatus] = static_cast<std::uint32_t>(status);
                return status == SurfaceStatus::Ok && values[GuiCleanupComplete] && !frontend && nativeEnded &&
                       !bindingsState[DefinitiveFrontendBindings::BindingsRetained];
            }
        } owner{adapter, request, persistent};
        struct Clock
        {
            std::uint64_t Now() const noexcept { return static_cast<std::uint64_t>(GetTickCount64_()); }
            void          Wait(std::uint32_t milliseconds) const noexcept { Sleep(milliseconds); }
        } clock;
        const auto runMs = clock.Now();
        const auto result = DefinitiveGuiStartupReference::Run<SurfaceSnapshot>(owner, clock, request, Persistent);
        try
        {
            // All native holds, GUI/GPU owners and the child window have ended
            // before file I/O; failure to log cannot relabel cleanup or drawing.
            std::string line = std::string("{\"schema\":\"") +
                               (Persistent       ? "mta_core_persistent_frontend_v1"
                                : CoreOwnedInput ? "mta_core_gui_frontend_input_v1"
                                                 : "mta_core_gui_frontend_v1") +
                               "\",\"pid\":" + std::to_string(GetCurrentProcessId()) + ",\"runMs\":" + std::to_string(runMs) + ",\"values\":[";
            for (std::size_t index = 0; index < result.size(); ++index)
            {
                if (index)
                    line += ',';
                line += std::to_string(result[index]);
            }
            line += "]}\n";
            const auto path = CalcMTASAPath(Persistent       ? "mta/logs/persistent-frontend.log"
                                            : CoreOwnedInput ? "mta/logs/gui-frontend-input.log"
                                                             : "mta/logs/gui-frontend.log");
            SharedUtil::MakeSureDirExists(path);
            SharedUtil::FileAppend(path, line.data(), static_cast<unsigned long>(line.size()));
            if constexpr (CoreOwnedInput)
            {
                // Cursor ownership has a separate diagnostic. Keep the public
                // 256/288-field reports unchanged. A bounded scope has neither
                // a persistent host generation nor a renewable native lease.
                const auto  state = owner.cursor.Get() ? owner.cursor.Get()->ReadFrontendLease() : CSetCursorPosHook::FrontendLeaseState{};
                std::string cursorLine;
                if constexpr (Persistent)
                    cursorLine = "{\"schema\":\"mta_core_frontend_cursor_v1\",\"pid\":" + std::to_string(GetCurrentProcessId()) +
                                 ",\"runMs\":" + std::to_string(runMs) + ",\"sessionGeneration\":" + std::to_string(persistent->generation) +
                                 ",\"nativeGeneration\":" + std::to_string(owner.nativeLease.generation);
                else
                    cursorLine = "{\"schema\":\"mta_core_bounded_frontend_cursor_v1\",\"pid\":" + std::to_string(GetCurrentProcessId()) +
                                 ",\"runMs\":" + std::to_string(runMs);
                cursorLine += ",\"attempted\":" + std::to_string(owner.cursor.Attempted()) + ",\"fault\":" + std::to_string(owner.cursor.Fault()) +
                              ",\"cleanupComplete\":" + std::to_string(owner.cursor.Ended() && !state.hooked && !state.inFlight) +
                              ",\"state\":{\"generation\":" + std::to_string(state.generation) + ",\"ownerThread\":" + std::to_string(state.ownerThread) +
                              ",\"hooked\":" + std::to_string(state.hooked) + ",\"active\":" + std::to_string(state.active) +
                              ",\"revoked\":" + std::to_string(state.revoked) + ",\"ended\":" + std::to_string(state.ended) +
                              ",\"lifetimeMs\":" + std::to_string(state.lifetimeMs) + ",\"renewals\":" + std::to_string(state.renewals) +
                              ",\"callbacks\":" + std::to_string(state.callbacks) + ",\"suppressed\":" + std::to_string(state.suppressed) +
                              ",\"passthrough\":" + std::to_string(state.passthrough) + ",\"inFlight\":" + std::to_string(state.inFlight) +
                              ",\"reason\":" + std::to_string(state.reason) + ",\"lastFailure\":" + std::to_string(state.lastFailure) +
                              ",\"attachStatus\":" + std::to_string(state.attachStatus) + ",\"detachStatus\":" + std::to_string(state.detachStatus) +
                              ",\"clipObserved\":" + std::to_string(state.clipObserved) + ",\"clip\":[" + std::to_string(state.clip.left) + ',' +
                              std::to_string(state.clip.top) + ',' + std::to_string(state.clip.right) + ',' + std::to_string(state.clip.bottom) + "]}}\n";
                const auto cursorPath = CalcMTASAPath("mta/logs/frontend-cursor.log");
                SharedUtil::MakeSureDirExists(cursorPath);
                SharedUtil::FileAppend(cursorPath, cursorLine.data(), static_cast<unsigned long>(cursorLine.size()));
            }
            if (owner.consoleAttempted && owner.consoleState[DefinitiveFrontendConsole::Version] == DefinitiveFrontendConsole::ApiVersion)
            {
                // Console diagnostics use their own schema and copied fields;
                // they cannot change the established Frontend160 contract or
                // promote a rendered menu into full gameplay startup proof.
                std::string consoleLine = "{\"schema\":\"mta_core_frontend_console_v1\",\"pid\":" + std::to_string(GetCurrentProcessId()) +
                                          ",\"runMs\":" + std::to_string(runMs) + ",\"values\":[";
                for (std::size_t index = 0; index < owner.consoleState.size(); ++index)
                {
                    if (index)
                        consoleLine += ',';
                    consoleLine += std::to_string(owner.consoleState[index]);
                }
                consoleLine += "]}\n";
                const auto consolePath = CalcMTASAPath("mta/logs/gui-console.log");
                SharedUtil::FileAppend(consolePath, consoleLine.data(), static_cast<unsigned long>(consoleLine.size()));
            }
        }
        catch (...)
        {
        }
        // A completed frontend Core keeps this outcome separately from its
        // lifetime. Closing the view must not destroy the backend owner or
        // relabel the ordinary InitializeCore gameplay-readiness result.
        return result;
    }

    void RunRenderReference(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept
    {
        struct Observer
        {
            CGameAdapterModule& adapter;
            bool                Begin() { return adapter.BeginNativeRenderObservation(); }
            bool                Read(DefinitiveRenderReference::Snapshot& snapshot) { return CopyRenderReferenceState(adapter, snapshot); }
            bool                End() { return adapter.EndNativeRenderObservation(); }
        } observer{adapter};
        struct Clock
        {
            std::uint64_t Now() const noexcept { return static_cast<std::uint64_t>(GetTickCount64_()); }
            void          Wait(std::uint32_t milliseconds) const noexcept { Sleep(milliseconds); }
        } clock;
        // Tests execute this exact retained lifecycle with a value-only fake
        // clock/observer; production supplies the actual Core-owned module.
        if (request.renderApiVersion == 6)
        {
            struct ObserverV6
            {
                CGameAdapterModule&                   adapter;
                mta::game_de::RenderObservationRoute  route;
                const std::uint64_t                   runMs = static_cast<std::uint64_t>(GetTickCount64_());
                DefinitiveRenderEndpointLog::Recorder endpointLog;
                DefinitiveRenderSetupLog::Recorder    setupLog;
                DefinitiveRenderRootsLog::Recorder    rootsLog;
                void                                  LogRoots(std::uint32_t phase) noexcept
                {
                    try
                    {
                        mta::game_de::RenderRootsDiagnosticV1 state;
                        // The exact API6 provider returns cached values only and
                        // refuses while peers are held. MTA file utilities run
                        // here on the owning worker, never inside a native callback.
                        if (!adapter.GetNativeRenderRootsDiagnostic(state))
                            return;
                        rootsLog.Write(state, GetCurrentProcessId(), runMs, phase,
                                       [](const std::string& line)
                                       {
                                           const auto path = CalcMTASAPath("mta/logs/render-roots.log");
                                           SharedUtil::MakeSureDirExists(path);
                                           return SharedUtil::FileAppend(path, line.data(), static_cast<unsigned long>(line.size()));
                                       });
                    }
                    catch (...)
                    {
                        // Optional diagnostics cannot change cleanup ownership.
                    }
                }
                void LogSetup(std::uint32_t phase) noexcept
                {
                    try
                    {
                        mta::game_de::RenderSetupDiagnosticV1 state;
                        // The adapter refuses this read while any peer remains
                        // held. Only then may optional MTA file utilities allocate
                        // or take locks to preserve the precise setup refusal.
                        if (!adapter.GetNativeRenderSetupDiagnostic(state))
                            return;
                        setupLog.Write(state, GetCurrentProcessId(), runMs, phase,
                                       [](const std::string& line)
                                       {
                                           const auto path = CalcMTASAPath("mta/logs/render-setup.log");
                                           SharedUtil::MakeSureDirExists(path);
                                           return SharedUtil::FileAppend(path, line.data(), static_cast<unsigned long>(line.size()));
                                       });
                    }
                    catch (...)
                    {
                        // Diagnostic I/O cannot change hook or module ownership.
                    }
                }
                void Log(std::uint32_t phase) noexcept
                {
                    try
                    {
                        mta::game_de::RenderEndpointDiagnosticV1 state;
                        if (!adapter.GetNativeRenderEndpointDiagnostic(state))
                            return;
                        endpointLog.Write(state, GetCurrentProcessId(), runMs, phase,
                                          [](const std::string& line)
                                          {
                                              // Reuse MTA file utilities under this Core's explicit
                                              // DE root. AddReportLog targets shared AppData instead.
                                              const auto path = CalcMTASAPath("mta/logs/render-endpoint.log");
                                              SharedUtil::MakeSureDirExists(path);
                                              return SharedUtil::FileAppend(path, line.data(), static_cast<unsigned long>(line.size()));
                                          });
                    }
                    catch (...)
                    {
                        // Optional file diagnostics must never change native
                        // observation, cancellation or hook cleanup ownership.
                    }
                }
                bool Begin()
                {
                    const bool began = adapter.BeginNativeRenderDispatchObservation(route);
                    LogSetup(0);
                    Log(0);
                    LogRoots(0);
                    return began;
                }
                bool Read(DefinitiveRenderReference::SnapshotV6& snapshot)
                {
                    const bool read = CopyRenderReferenceState(adapter, snapshot);
                    if (read)
                    {
                        LogSetup(1);
                        Log(1);
                        LogRoots(1);
                    }
                    return read;
                }
                bool End()
                {
                    const bool ended = adapter.EndNativeRenderObservation();
                    if (ended)
                    {
                        LogSetup(2);
                        Log(2);
                        LogRoots(2);
                    }
                    return ended;
                }
            } boundaryObserver{adapter, static_cast<mta::game_de::RenderObservationRoute>(request.renderRoute)};
            DefinitiveRenderReference::Run<DefinitiveRenderReference::SnapshotV6>(boundaryObserver, clock, request);
        }
        else if (request.renderApiVersion == 5)
        {
            struct ObserverV5
            {
                CGameAdapterModule&                   adapter;
                mta::game_de::RenderObservationRoute  route;
                const std::uint64_t                   runMs = static_cast<std::uint64_t>(GetTickCount64_());
                DefinitiveRenderEndpointLog::Recorder endpointLog;
                void                                  Log(std::uint32_t phase) noexcept
                {
                    try
                    {
                        mta::game_de::RenderEndpointDiagnosticV1 state;
                        if (!adapter.GetNativeRenderEndpointDiagnostic(state))
                            return;
                        endpointLog.Write(state, GetCurrentProcessId(), runMs, phase,
                                          [](const std::string& line)
                                          {
                                              // Reuse MTA file utilities under this Core's explicit
                                              // DE root. AddReportLog targets shared AppData instead.
                                              const auto path = CalcMTASAPath("mta/logs/render-endpoint.log");
                                              SharedUtil::MakeSureDirExists(path);
                                              return SharedUtil::FileAppend(path, line.data(), static_cast<unsigned long>(line.size()));
                                          });
                    }
                    catch (...)
                    {
                        // Optional file diagnostics must never change native
                        // observation, cancellation or hook cleanup ownership.
                    }
                }
                bool Begin()
                {
                    const bool began = adapter.BeginNativeRenderFrameBoundaryObservation(route);
                    Log(0);
                    return began;
                }
                bool Read(DefinitiveRenderReference::SnapshotV5& snapshot)
                {
                    const bool read = CopyRenderReferenceState(adapter, snapshot);
                    if (read)
                        Log(1);
                    return read;
                }
                bool End()
                {
                    const bool ended = adapter.EndNativeRenderObservation();
                    if (ended)
                        Log(2);
                    return ended;
                }
            } boundaryObserver{adapter, static_cast<mta::game_de::RenderObservationRoute>(request.renderRoute)};
            DefinitiveRenderReference::Run<DefinitiveRenderReference::SnapshotV5>(boundaryObserver, clock, request);
        }
        else if (request.renderApiVersion == 4)
        {
            struct ObserverV4
            {
                CGameAdapterModule&                  adapter;
                mta::game_de::RenderObservationRoute route;
                bool                                 Begin() { return adapter.BeginNativeRenderClassificationObservation(route); }
                bool Read(DefinitiveRenderReference::SnapshotV4& snapshot) { return CopyRenderReferenceState(adapter, snapshot); }
                bool End() { return adapter.EndNativeRenderObservation(); }
            } classificationObserver{adapter, static_cast<mta::game_de::RenderObservationRoute>(request.renderRoute)};
            DefinitiveRenderReference::Run<DefinitiveRenderReference::SnapshotV4>(classificationObserver, clock, request);
        }
        else if (request.renderApiVersion == 3)
        {
            struct ObserverV3
            {
                CGameAdapterModule&                  adapter;
                mta::game_de::RenderObservationRoute route;
                bool                                 Begin() { return adapter.BeginNativeRenderOwnershipObservation(route); }
                bool Read(DefinitiveRenderReference::SnapshotV3& snapshot) { return CopyRenderReferenceState(adapter, snapshot); }
                bool End() { return adapter.EndNativeRenderObservation(); }
            } ownershipObserver{adapter, static_cast<mta::game_de::RenderObservationRoute>(request.renderRoute)};
            DefinitiveRenderReference::Run<DefinitiveRenderReference::SnapshotV3>(ownershipObserver, clock, request);
        }
        else if (!request.renderRoute)
            DefinitiveRenderReference::Run(observer, clock, request);
        else
        {
            struct ObserverV2
            {
                CGameAdapterModule&                  adapter;
                mta::game_de::RenderObservationRoute route;
                bool                                 Begin() { return adapter.BeginNativeRenderObservation(route); }
                bool Read(DefinitiveRenderReference::SnapshotV2& snapshot) { return CopyRenderReferenceState(adapter, snapshot); }
                bool End() { return adapter.EndNativeRenderObservation(); }
            } explicitObserver{adapter, static_cast<mta::game_de::RenderObservationRoute>(request.renderRoute)};
            DefinitiveRenderReference::Run<DefinitiveRenderReference::SnapshotV2>(explicitObserver, clock, request);
        }
    }

    void RunConfiguredInputReference(CCore* owner, CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept
    {
        using namespace DefinitiveInputReference;
        constexpr auto allowedActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(BACKWARDS) | MtaInput::ActionBit(LEFT) |
                                        MtaInput::ActionBit(RIGHT) | MtaInput::ActionBit(SPRINT) | MtaInput::ActionBit(WALK);
        DefinitiveMovementReference::Snapshot snapshot{};
        const auto                            fieldCount = request.nativeMovement ? DefinitiveMovementReference::FieldCount : FieldCount;
        const auto                            maximumSamples = request.nativeMovement ? DefinitiveMovementReference::MaximumSamples : MaximumSamples;
        snapshot[Version] = ApiVersion;
        snapshot[Flags] = request.nativeMovement ? 0u : 4u;  // Observation mode retains its explicitly assumed context.
        auto reason = StopReason::ConfigError;
        bool clean = true;
        auto started = static_cast<std::uint64_t>(GetTickCount64_());
        auto lastPulse = started;
        try
        {
            // This is the real, partially constructed Core owner. Keep binds/XML
            // on its worker stack because a rejected CCore constructor does not
            // run the classic destructor, which depends on a full game and GUI.
            CKeyBinds         binds(owner);
            InputReferenceXml xml;
            binds.CallAllGTAControlBinds(CONTROL_BOTH, false);
            binds.ResetAllGTAControlStates();
            try
            {
                auto* root = xml.Open() ? xml.file->GetRootNode() : nullptr;
                auto* configured = root && root->GetTagName() == CONFIG_ROOT ? root->FindSubNode(CONFIG_NODE_KEYBINDS) : nullptr;
                if (configured && configured->GetSubNodeCount() && binds.LoadConfiguredBindsFromXML(configured))
                {
                    started = lastPulse = static_cast<std::uint64_t>(GetTickCount64_());
                    bool previousFocus = false, previousArmed = false;
                    reason = StopReason::Running;
                    for (;;)
                    {
                        const auto now = static_cast<std::uint64_t>(GetTickCount64_());
                        snapshot[ElapsedMs] = static_cast<std::uint32_t>((std::min)(now - started, std::uint64_t{0xffffffffu}));
                        if (request.cancelled())
                        {
                            reason = StopReason::Cancelled;
                            break;
                        }
                        if (now - lastPulse > MaximumPulseGapMs)
                        {
                            reason = StopReason::PulseGap;
                            break;
                        }
                        if ((!request.nativeMovement || snapshot[DefinitiveMovementReference::WriterStarted]) && now - started >= request.durationMs)
                        {
                            reason = StopReason::TimeBudget;
                            break;
                        }
                        if (snapshot[SampleCount] >= maximumSamples)
                        {
                            reason = StopReason::SampleBudget;
                            break;
                        }
                        lastPulse = now;
                        const auto poll = binds.PollConfiguredControlKeys(InputReferenceFocused(), nullptr, nullptr, allowedActions);
                        snapshot[SupportedBindings] = poll.supportedBindings;
                        snapshot[IgnoredBindings] = poll.ignoredBindings;
                        snapshot[EligibleKeys] = poll.eligibleKeys;
                        snapshot[UnsupportedKeys] = poll.unsupportedKeys;
                        snapshot[Flags] = (request.nativeMovement ? 0u : 4u) | (poll.focused ? 1u : 0u) | (poll.armed ? 2u : 0u);
                        if (previousFocus && !poll.focused)
                            ++snapshot[FocusLosses];
                        if (!previousArmed && poll.armed)
                            ++snapshot[Rearms];
                        previousFocus = poll.focused;
                        previousArmed = poll.armed;
                        if (!poll.eligibleKeys)
                        {
                            reason = StopReason::NoSupportedKeys;
                            break;
                        }

                        MtaInput::Context context;
                        if (request.nativeMovement)
                        {
                            mta::game_de::NativeMovementEligibility eligibility;
                            if (!adapter.GetNativeMovementEligibility(eligibility))
                            {
                                // Before hooks exist, the worker may observe an
                                // engine input context borrowed for another ped.
                                // Wait within the existing readiness budget;
                                // no frame can authorize writes in this state.
                                if (!snapshot[DefinitiveMovementReference::WriterStarted] && now - started < DefinitiveMovementReference::ReadinessBudgetMs)
                                {
                                    Sleep(SampleIntervalMs);
                                    continue;
                                }
                                reason = static_cast<StopReason>(snapshot[DefinitiveMovementReference::WriterStarted]
                                                                     ? DefinitiveMovementReference::StopReason::EligibilityError
                                                                     : DefinitiveMovementReference::StopReason::ReadinessTimeout);
                                break;
                            }
                            snapshot[DefinitiveMovementReference::ContextKnownMask] = eligibility.knownMask;
                            snapshot[DefinitiveMovementReference::ContextFlags] = eligibility.flags;
                            snapshot[DefinitiveMovementReference::Eligible] = eligibility.eligible ? 1u : 0u;
                            if (eligibility.knownMask != mta::game_de::NativeMovementEligibilityMask ||
                                eligibility.flags != mta::game_de::NativeMovementEligibilityMask || !eligibility.eligible)
                            {
                                reason = static_cast<StopReason>(DefinitiveMovementReference::StopReason::EligibilityError);
                                break;
                            }
                            context = eligibility.context;
                            if (snapshot[DefinitiveMovementReference::WriterStarted] && (!poll.focused || !poll.armed))
                            {
                                reason = static_cast<StopReason>(DefinitiveMovementReference::StopReason::FocusLost);
                                break;
                            }
                            if (!snapshot[DefinitiveMovementReference::WriterStarted] && now - started >= DefinitiveMovementReference::ReadinessBudgetMs)
                            {
                                reason = static_cast<StopReason>(DefinitiveMovementReference::StopReason::ReadinessTimeout);
                                break;
                            }
                        }
                        // The same actual configured manager publishes both modes.
                        // Write admission additionally uses verified eligibility;
                        // excluded weapon/vehicle actions never reach this route.
                        const auto                  frame = binds.BuildConfiguredControlFrame(context);
                        mta::game_de::MtaInputState state;
                        if ((frame.heldActions & ~allowedActions) || !adapter.Pulse() || !adapter.SubmitControlActions({frame.heldActions, frame.context}) ||
                            !adapter.GetMtaInputState(state) || std::memcmp(&frame.controller, &state.current, sizeof(state.current)) != 0)
                        {
                            reason = StopReason::AdapterError;
                            break;
                        }
                        if (request.nativeMovement)
                        {
                            if (!snapshot[DefinitiveMovementReference::WriterStarted] && poll.focused && poll.armed)
                            {
                                if (frame.heldActions || !adapter.BeginNativeMovementTrial())
                                {
                                    reason = static_cast<StopReason>(DefinitiveMovementReference::StopReason::NativeFailure);
                                    break;
                                }
                                snapshot[DefinitiveMovementReference::WriterStarted] = 1;
                                started = lastPulse = static_cast<std::uint64_t>(GetTickCount64_());
                                snapshot[ElapsedMs] = 0;
                            }
                            if (!CopyMovementReferenceState(adapter, snapshot) ||
                                (snapshot[DefinitiveMovementReference::WriterStarted] &&
                                 (!snapshot[DefinitiveMovementReference::NativeMetrics + 2] || snapshot[DefinitiveMovementReference::NativeMetrics + 20])))
                            {
                                reason = static_cast<StopReason>(DefinitiveMovementReference::StopReason::NativeFailure);
                                break;
                            }
                            if (!snapshot[DefinitiveMovementReference::WriterStarted])
                            {
                                // Baseline keys must be released before one
                                // native trial starts. Keep the published clock
                                // at zero until that owned write window begins.
                                Sleep(SampleIntervalMs);
                                continue;
                            }
                        }
                        binds.FinishConfiguredControlFrame();
                        snapshot[Phase] = 1;
                        ++snapshot[Sequence];
                        ++snapshot[SampleCount];
                        snapshot[HeldActionsLow] = static_cast<std::uint32_t>(frame.heldActions);
                        snapshot[HeldActionsHigh] = static_cast<std::uint32_t>(frame.heldActions >> 32);
                        CopyInputReferenceController(snapshot, Current, state.current);
                        CopyInputReferenceController(snapshot, Previous, state.previous);
                        request.publish(snapshot.data(), fieldCount);
                        Sleep(SampleIntervalMs);
                    }
                }
            }
            catch (...)
            {
                reason = StopReason::Exception;
            }

            if (request.nativeMovement)
            {
                // End disarms first, but an engine callback may still own the
                // restoration snapshot. Keep this real Core and its producer
                // thread alive until End proves detachment, even after deadline.
                const auto cleanupStarted = static_cast<std::uint64_t>(GetTickCount64_());
                snapshot[DefinitiveMovementReference::CleanupPending] = 1;
                bool inputReleased = false;
                for (;;)
                {
                    if (snapshot[DefinitiveMovementReference::CleanupAttempts] != 0xffffffffu)
                        ++snapshot[DefinitiveMovementReference::CleanupAttempts];
                    bool ended = false;
                    try
                    {
                        ended = adapter.EndNativeMovementTrial();
                    }
                    catch (...)
                    {
                        reason = StopReason::CleanupError;
                    }
                    if (!inputReleased)
                    {
                        try
                        {
                            binds.CallAllGTAControlBinds(CONTROL_BOTH, false);
                            binds.ResetAllGTAControlStates();
                            binds.PollConfiguredControlKeys(false, nullptr, nullptr, allowedActions);
                            mta::game_de::MtaInputState state;
                            inputReleased = adapter.SubmitControlActions({0, {}}) && adapter.SubmitControlActions({0, {}}) && adapter.GetMtaInputState(state);
                            if (inputReleased)
                            {
                                CopyInputReferenceController(snapshot, Current, state.current);
                                CopyInputReferenceController(snapshot, Previous, state.previous);
                                snapshot[HeldActionsLow] = snapshot[HeldActionsHigh] = snapshot[Flags] = 0;
                            }
                        }
                        catch (...)
                        {
                            reason = StopReason::CleanupError;
                        }
                    }
                    if (ended)
                        break;
                    const auto now = static_cast<std::uint64_t>(GetTickCount64_());
                    if (now - cleanupStarted >= DefinitiveMovementReference::CleanupDeadlineMs)
                    {
                        snapshot[DefinitiveMovementReference::CleanupDeadlineExceeded] = 1;
                        reason = StopReason::CleanupError;
                    }
                    try
                    {
                        CopyMovementReferenceState(adapter, snapshot);
                    }
                    catch (...)
                    {
                    }
                    snapshot[Phase] = 3;
                    ++snapshot[Sequence];
                    snapshot[Reason] = static_cast<std::uint32_t>(reason);
                    try
                    {
                        request.publish(snapshot.data(), fieldCount);
                    }
                    catch (...)
                    {
                    }
                    Sleep(snapshot[DefinitiveMovementReference::CleanupDeadlineExceeded] ? DefinitiveMovementReference::CleanupBackoffMs : SampleIntervalMs);
                }
                snapshot[DefinitiveMovementReference::CleanupPending] = 0;
                if (!CopyMovementReferenceState(adapter, snapshot) || snapshot[DefinitiveMovementReference::NativeMetrics] ||
                    snapshot[DefinitiveMovementReference::NativeMetrics + 2] || snapshot[DefinitiveMovementReference::NativeMetrics + 14] ||
                    snapshot[DefinitiveMovementReference::NativeMetrics + 15] ||
                    snapshot[DefinitiveMovementReference::NativeMetrics + 6] != snapshot[DefinitiveMovementReference::NativeMetrics + 12] ||
                    snapshot[DefinitiveMovementReference::NativeMetrics + 7] != snapshot[DefinitiveMovementReference::NativeMetrics + 13])
                    clean = false;
            }

            try
            {
                binds.CallAllGTAControlBinds(CONTROL_BOTH, false);
                binds.ResetAllGTAControlStates();
                binds.PollConfiguredControlKeys(false, nullptr, nullptr, allowedActions);
                const auto                  neutral = binds.BuildConfiguredControlFrame({});
                mta::game_de::MtaInputState state;
                // Two explicit neutral publications clear both real snapshots,
                // rather than claiming that release also erases previous input.
                clean = !neutral.heldActions && adapter.SubmitControlActions({0, {}}) && adapter.SubmitControlActions({0, {}}) &&
                        adapter.GetMtaInputState(state) && clean;
                if (clean)
                {
                    CopyInputReferenceController(snapshot, Current, state.current);
                    CopyInputReferenceController(snapshot, Previous, state.previous);
                    for (std::uint32_t field = Current; field < FieldCount; ++field)
                        clean = clean && !snapshot[field];
                }
                binds.FinishConfiguredControlFrame();
            }
            catch (...)
            {
                clean = false;
            }
            clean = xml.Close() && clean;
        }
        catch (...)
        {
            reason = StopReason::Exception;
            clean = false;
        }
        // This final callback runs only after the actual bind and XML owners
        // have gone. Adapter teardown follows in the existing constructor catch.
        snapshot[Phase] = 2;
        ++snapshot[Sequence];
        snapshot[Flags] = request.nativeMovement ? 0u : 4u;
        snapshot[HeldActionsLow] = snapshot[HeldActionsHigh] = 0;
        snapshot[InputCleanupComplete] = clean ? 1u : 0u;
        snapshot[Reason] = static_cast<std::uint32_t>(clean ? reason : StopReason::CleanupError);
        try
        {
            request.publish(snapshot.data(), fieldCount);
        }
        catch (...)
        {
        }
    }
}
#endif

template <>
CCore* CSingleton<CCore>::m_pSingleton = NULL;

static auto                Win32LoadLibraryA = LoadLibraryA;
static constexpr long long TIME_DISCORD_UPDATE_RICH_PRESENCE_RATE = 10000;

static HMODULE WINAPI SkipDirectPlay_LoadLibraryA(LPCSTR fileName)
{
    // GTA:SA expects a valid module handle for DirectPlay. We return a handle for an already loaded library.
    if (!StrCmpIA("dpnhpast.dll", fileName))
        return Win32LoadLibraryA("d3d8.dll");

    if (!StrCmpIA("enbseries\\enbhelper.dll", fileName))
    {
        std::error_code ec;

        // Try to load enbhelper.dll from our custom launch directory first.
        const fs::path inLaunchDir = fs::path{FromUTF8(GetLaunchPath())} / "enbseries" / "enbhelper.dll";

        if (fs::is_regular_file(inLaunchDir, ec))
            return Win32LoadLibraryA(UTF8FilePath(inLaunchDir).c_str());

        // Try to load enbhelper.dll from the GTA install directory second.
        const fs::path inGTADir = g_gtaDirectory / "enbseries" / "enbhelper.dll";

        if (fs::is_regular_file(inGTADir, ec))
            return Win32LoadLibraryA(UTF8FilePath(inGTADir).c_str());

        return nullptr;
    }

    return Win32LoadLibraryA(fileName);
}

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
struct CCore::FrontendRuntimeResources
{
    InputReferenceXml                   xml;
    std::unique_ptr<CKeyBinds>          bindings;
    DefinitiveFrontendRuntime::Snapshot snapshot{};
    bool                                cvarsLoaded = false;
    bool                                cvarsAttempted = false;
};

struct CCore::LocalInputRuntimeResources
{
    DefinitiveLocalInputRuntime::Snapshot snapshot{};
    mta::game_de::LocalInputLeaseStateV1  lease;
    HWND                                  window = nullptr;
    bool                                  nativePending = false;
    bool                                  fault = false;
};

struct CCore::FrontendResources
{
    InputReferenceXml                    xml;
    CLocalization*                       previousLocalization = nullptr;
    bool                                 updaterAcquired = false;
    DefinitiveFrontend::Snapshot         snapshot{};
    DefinitiveFrontendConsole::Snapshot  consoleSnapshot{};
    DefinitiveFrontendBindings::Snapshot bindingsSnapshot{};
    mta::game_de::MtaInputState          neutralInput{};
    bool                                 bindingsEnabled = false;
    bool                                 runtimeBorrowed = false;
    bool                                 menuInputRequested = false, consoleInputRequested = false, consoleEnabled = false;
};

bool CCore::FlushFrontendRuntimeBindings() noexcept
{
    if (!m_frontendRuntimeResources || !m_frontendRuntimeResources->bindings)
        return false;
    try
    {
        auto& bindings = *m_frontendRuntimeResources->bindings;
        if (bindings.Count(KeyBindType::FUNCTION) || bindings.Count(KeyBindType::CONTROL_FUNCTION))
            return false;
        const CControllerState zero;
        // The manager survives a hidden view, but its held-key history must
        // not. Use its existing configured producer without dispatching any
        // command or function, including when a view failed before Enable.
        for (unsigned frame = 0; frame != 2; ++frame)
        {
            bindings.PollConfiguredControlKeys(false, nullptr, nullptr, DefinitiveFrontendBindings::MovementActions);
            const auto neutral = bindings.BuildConfiguredControlFrame({});
            bindings.FinishConfiguredControlFrame();
            if (neutral.heldActions || std::memcmp(&neutral.controller, &zero, sizeof(zero)) != 0)
                return false;
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool CCore::LocalInputAllowsFrontend() const noexcept
{
    using namespace DefinitiveLocalInputRuntime;
    return !m_localInputRuntimeResources ||
           (!m_localInputRuntimeResources->nativePending && !m_localInputRuntimeResources->snapshot[Enabled] &&
            !m_localInputRuntimeResources->snapshot[CleanupPending] && m_localInputRuntimeResources->snapshot[CleanupComplete] &&
            m_localInputRuntimeResources->snapshot[CanonicalNeutral]);
}

bool CCore::FlushLocalInputRuntime() noexcept
{
    using namespace DefinitiveLocalInputRuntime;
    if (!m_localInputRuntimeResources)
        return true;
    auto& state = m_localInputRuntimeResources->snapshot;
    state[CanonicalNeutral] = 0;
    if (m_frontendRuntimeResources)
        m_frontendRuntimeResources->snapshot[DefinitiveFrontendRuntime::HiddenNeutralVerified] = 0;
    try
    {
        if (!FlushFrontendRuntimeBindings())
            return false;
        // Both canonical frames must become neutral. Releasing physical keys
        // or the manager's current state alone does not clear previous edges.
        mta::game_de::MtaInputState       observed;
        const mta::game_de::MtaInputState zero;
        const bool                        neutral = m_DefinitiveAdapter.SubmitControlActions({0, {}}) && m_DefinitiveAdapter.SubmitControlActions({0, {}}) &&
                             m_DefinitiveAdapter.GetMtaInputState(observed) && std::memcmp(&observed, &zero, sizeof(zero)) == 0;
        state[CanonicalNeutral] = neutral;
        state[HeldActionsLow] = state[HeldActionsHigh] = state[PhysicalRearmReady] = 0;
        if (m_frontendRuntimeResources)
            m_frontendRuntimeResources->snapshot[DefinitiveFrontendRuntime::HiddenNeutralVerified] = neutral;
        return neutral;
    }
    catch (...)
    {
        state[CanonicalNeutral] = 0;
        return false;
    }
}

DefinitiveLocalInputRuntime::Snapshot CCore::ReadLocalInputRuntime() noexcept
{
    using namespace DefinitiveLocalInputRuntime;
    if (GetCurrentThreadId() != m_frontendCoreOwnerThread)
        return {};
    Snapshot empty{};
    empty[Version] = ApiVersion;
    empty[Phase] = static_cast<std::uint32_t>(State::Disabled);
    empty[OwnerThread] = m_frontendCoreOwnerThread;
    empty[CleanupComplete] = 1;
    if (!m_localInputRuntimeResources)
        return empty;
    auto& resources = *m_localInputRuntimeResources;
    if (resources.nativePending && resources.lease.generation)
    {
        try
        {
            mta::game_de::LocalInputLeaseStateV1 latest;
            if (m_DefinitiveAdapter.ReadLocalInputLease(latest) && latest.generation == resources.lease.generation)
                resources.lease = latest;
        }
        catch (...)
        {
            // A failed diagnostic read cannot discard the last known owner or
            // claim cleanup. Pulse/Suspend still have to retire that attempt.
            resources.snapshot[LastFailure] = static_cast<std::uint32_t>(Failure::NativeRead);
        }
    }
    auto&       state = resources.snapshot;
    const auto& lease = resources.lease;
    state[NativeGenerationLow] = static_cast<std::uint32_t>(lease.generation);
    state[NativeGenerationHigh] = static_cast<std::uint32_t>(lease.generation >> 32);
    state[NativeActive] = lease.active;
    state[NativeBound] = lease.bound;
    state[NativeRevoked] = lease.revoked;
    state[NativeReason] = lease.reason;
    state[NativeLifetimeMs] = lease.lifetimeMs;
    state[RenewalsLow] = static_cast<std::uint32_t>(lease.renewals);
    state[RenewalsHigh] = static_cast<std::uint32_t>(lease.renewals >> 32);
    state[BindingsBorrowed] = state[Configured] && m_frontendRuntimeResources && m_frontendRuntimeResources->bindings != nullptr;
    return state;
}

DefinitiveLocalInputRuntime::Status CCore::StopLocalInputRuntime(bool disable, DefinitiveLocalInputRuntime::Failure failure, bool fault) noexcept
{
    using namespace DefinitiveLocalInputRuntime;
    if (GetCurrentThreadId() != m_frontendCoreOwnerThread)
        return Status::WrongThread;
    if (!m_localInputRuntimeResources)
        return Status::Ok;
    auto& resources = *m_localInputRuntimeResources;
    auto& state = resources.snapshot;
    state[Enabled] = 0;
    if (disable)
        state[Configured] = 0;
    if (failure != Failure::None)
        state[LastFailure] = static_cast<std::uint32_t>(failure);
    resources.fault |= fault;
    if (!resources.nativePending && !state[CleanupPending] && state[CleanupComplete] && state[CanonicalNeutral])
    {
        state[Phase] = static_cast<std::uint32_t>(!state[Configured] ? State::Disabled : resources.fault ? State::Failed : State::Suspended);
        return Status::Ok;
    }
    state[Phase] = static_cast<std::uint32_t>(State::RetainedCleanup);
    state[CleanupPending] = 1;
    state[CleanupComplete] = 0;
    try
    {
        if (resources.nativePending)
        {
            if (state[NativeEndAttempts] != UINT32_MAX)
                ++state[NativeEndAttempts];
            if (resources.lease.generation)
            {
                if (m_DefinitiveAdapter.EndLocalInputLease(resources.lease.generation))
                {
                    mta::game_de::LocalInputLeaseStateV1 ended;
                    if (m_DefinitiveAdapter.ReadLocalInputLease(ended) && ended.generation == resources.lease.generation && !ended.active && !ended.bound &&
                        ended.revoked && ended.reason)
                    {
                        resources.lease = ended;
                        resources.nativePending = false;
                    }
                }
            }
            else if (m_DefinitiveAdapter.Reset())
            {
                // Unsupported Begin without a token is not retirement proof.
                // A successful provider Reset explicitly retires that attempt.
                resources.nativePending = false;
            }
        }
        const bool neutral = FlushLocalInputRuntime();
        if (resources.nativePending || !neutral)
            return Status::Busy;
        state[CleanupPending] = 0;
        state[CleanupComplete] = 1;
        state[Phase] = static_cast<std::uint32_t>(!state[Configured] ? State::Disabled : resources.fault ? State::Failed : State::Suspended);
        // Historical workload failures never prevent a later safe shutdown.
        // Only the actual retained obligations above can keep resources alive.
        return Status::Ok;
    }
    catch (...)
    {
        state[LastFailure] = static_cast<std::uint32_t>(Failure::Exception);
        resources.fault = true;
        return Status::Busy;
    }
}

DefinitiveLocalInputRuntime::Status CCore::ConfigureLocalInputRuntime(const DefinitiveLocalInputRuntime::RequestV1& request) noexcept
{
    using namespace DefinitiveLocalInputRuntime;
    if (GetCurrentThreadId() != m_frontendCoreOwnerThread)
        return Status::WrongThread;
    DWORD      process = 0;
    const auto window = static_cast<HWND>(request.window);
    if (request.size != sizeof(request) || request.version != ApiVersion || request.reserved[0] || request.reserved[1] || !window || !IsWindow(window) ||
        GetAncestor(window, GA_ROOT) != window || !GetWindowThreadProcessId(window, &process) || process != GetCurrentProcessId())
        return Status::InvalidArgument;
    if (!m_frontendStartupOwned || !m_frontendRuntimeResources || !m_frontendRuntimeResources->bindings || m_frontendResources || !LocalInputAllowsFrontend())
        return Status::Busy;
    try
    {
        if (!m_localInputRuntimeResources)
            m_localInputRuntimeResources = new LocalInputRuntimeResources;
        auto& resources = *m_localInputRuntimeResources;
        resources.window = window;
        resources.lease = {};
        resources.fault = false;
        resources.snapshot = {};
        auto& state = resources.snapshot;
        state[Version] = ApiVersion;
        state[Phase] = static_cast<std::uint32_t>(State::Arming);
        state[OwnerThread] = m_frontendCoreOwnerThread;
        state[Configured] = state[Enabled] = state[BindingsBorrowed] = 1;
        state[NativeLifetimeMs] = mta::game_de::LocalInputLeaseLifetimeMsV1;
        if (!FlushLocalInputRuntime())
        {
            const auto stopped = StopLocalInputRuntime(false, Failure::CanonicalInput, true);
            return stopped == Status::Ok ? Status::Failed : stopped;
        }
        state[CleanupComplete] = 1;
        return Status::Ok;
    }
    catch (...)
    {
        if (m_localInputRuntimeResources)
        {
            const auto stopped = StopLocalInputRuntime(false, Failure::Exception, true);
            return stopped == Status::Ok ? Status::Failed : stopped;
        }
        return Status::Failed;
    }
}

DefinitiveLocalInputRuntime::Status CCore::PulseLocalInputRuntime() noexcept
{
    using namespace DefinitiveLocalInputRuntime;
    if (GetCurrentThreadId() != m_frontendCoreOwnerThread)
        return Status::WrongThread;
    if (!m_localInputRuntimeResources)
        return Status::Stopped;
    auto&      resources = *m_localInputRuntimeResources;
    auto&      state = resources.snapshot;
    const auto stop = [&](Failure failure, bool fault = false) noexcept
    {
        const auto result = StopLocalInputRuntime(false, failure, fault);
        return result == Status::Ok && fault ? Status::Failed : result;
    };
    if (!state[Enabled])
        return resources.nativePending || state[CleanupPending] ? StopLocalInputRuntime(false) : Status::Ok;
    if (!m_frontendRuntimeResources || !m_frontendRuntimeResources->bindings || m_frontendResources)
        return Status::Busy;
    try
    {
        auto& bindings = *m_frontendRuntimeResources->bindings;
        if (bindings.Count(KeyBindType::FUNCTION) || bindings.Count(KeyBindType::CONTROL_FUNCTION))
            return stop(Failure::Bindings, true);
        DWORD      process = 0, foregroundProcess = 0;
        const auto foreground = GetForegroundWindow();
        const bool focused = resources.window && IsWindow(resources.window) && GetAncestor(resources.window, GA_ROOT) == resources.window &&
                             GetWindowThreadProcessId(resources.window, &process) && process == GetCurrentProcessId() && foreground &&
                             GetWindowThreadProcessId(foreground, &foregroundProcess) && foregroundProcess == process &&
                             GetAncestor(foreground, GA_ROOT) == resources.window;
        state[Focused] = focused;
        if (!focused)
        {
            if (resources.nativePending)
                return stop(Failure::FocusLost);
            return FlushLocalInputRuntime() ? Status::Ok : stop(Failure::CanonicalInput, true);
        }
        const auto poll = bindings.PollConfiguredControlKeys(true, nullptr, nullptr, DefinitiveFrontendBindings::MovementActions);
        state[SupportedBindings] = poll.supportedBindings;
        state[IgnoredBindings] = poll.ignoredBindings;
        state[EligibleKeys] = poll.eligibleKeys;
        state[UnsupportedKeys] = poll.unsupportedKeys;
        state[PhysicalRearmReady] = poll.armed;
        if (!poll.eligibleKeys)
            return stop(Failure::Bindings, true);
        if (resources.nativePending)
        {
            mta::game_de::LocalInputLeaseStateV1 current;
            if (!m_DefinitiveAdapter.ReadLocalInputLease(current) || current.generation != resources.lease.generation)
                return stop(Failure::NativeRead, true);
            resources.lease = current;
            if (!current.active || !current.bound || current.revoked)
                return stop(Failure::LeaseRevoked);
            if (!poll.armed)
                return stop(Failure::Bindings);
        }
        mta::game_de::NativeMovementEligibility eligibility;
        const bool                              eligible = m_DefinitiveAdapter.GetNativeMovementEligibility(eligibility) &&
                              eligibility.knownMask == mta::game_de::NativeMovementEligibilityMask &&
                              eligibility.flags == mta::game_de::NativeMovementEligibilityMask && eligibility.eligible;
        state[ContextKnownMask] = eligibility.knownMask;
        state[ContextFlags] = eligibility.flags;
        state[ContextEligible] = eligible;
        if (!eligible)
        {
            if (resources.nativePending)
                return stop(Failure::EligibilityLost);
            return FlushLocalInputRuntime() ? Status::Ok : stop(Failure::CanonicalInput, true);
        }
        const auto frame = bindings.BuildConfiguredControlFrame(eligibility.context);
        if (!resources.nativePending)
        {
            // A focused poll arms only after release. Keep both logical and
            // canonical histories neutral until that release and fresh exact
            // eligibility are observed together; never Begin on a held key.
            if (!poll.armed || frame.heldActions)
                return FlushLocalInputRuntime() ? Status::Ok : stop(Failure::CanonicalInput, true);
            resources.nativePending = true;
            state[NativeAttempted] = 1;
            state[CleanupComplete] = 0;
            if (!m_DefinitiveAdapter.BeginLocalInputLease(resources.window, resources.lease))
                return stop(Failure::NativeBegin, true);
            if (!resources.lease.generation || !resources.lease.active || !resources.lease.bound || resources.lease.revoked)
                return stop(Failure::NativeRead, true);
        }
        mta::game_de::MtaInputState canonical;
        if ((frame.heldActions & ~DefinitiveFrontendBindings::MovementActions) ||
            !m_DefinitiveAdapter.SubmitControlActions({frame.heldActions, frame.context}) || !m_DefinitiveAdapter.GetMtaInputState(canonical) ||
            std::memcmp(&frame.controller, &canonical.current, sizeof(frame.controller)) != 0)
            return stop(Failure::CanonicalInput, true);
        if (!m_DefinitiveAdapter.RenewLocalInputLease(resources.lease.generation))
        {
            mta::game_de::LocalInputLeaseStateV1 revoked;
            if (m_DefinitiveAdapter.ReadLocalInputLease(revoked) && revoked.generation == resources.lease.generation && revoked.revoked)
            {
                resources.lease = revoked;
                return stop(Failure::LeaseRevoked);
            }
            return stop(Failure::NativeRenew, true);
        }
        bindings.FinishConfiguredControlFrame();
        const mta::game_de::MtaInputState zero;
        state[CanonicalNeutral] = std::memcmp(&canonical, &zero, sizeof(zero)) == 0;
        state[HeldActionsLow] = static_cast<std::uint32_t>(frame.heldActions);
        state[HeldActionsHigh] = static_cast<std::uint32_t>(frame.heldActions >> 32);
        state[Phase] = static_cast<std::uint32_t>(State::Active);
        state[CleanupComplete] = 0;
        m_frontendRuntimeResources->snapshot[DefinitiveFrontendRuntime::HiddenNeutralVerified] = state[CanonicalNeutral];
        ReadLocalInputRuntime();
        return Status::Ok;
    }
    catch (...)
    {
        return stop(Failure::Exception, true);
    }
}

DefinitiveFrontend::Status CCore::EnableFrontendRuntimeServices() noexcept
{
    using namespace DefinitiveFrontendRuntime;
    using Status = DefinitiveFrontend::Status;
    if (GetCurrentThreadId() != m_frontendCoreOwnerThread)
        return Status::WrongThread;
    if (!m_frontendStartupOwned || !m_DefinitiveAdapter.IsStarted() || m_frontendResources || m_pXML || m_pConfigFile || m_pKeyBinds)
        return Status::Busy;
    if (m_frontendRuntimeResources)
    {
        const auto& state = m_frontendRuntimeResources->snapshot;
        return state[CleanupComplete] ? Status::Stopped : state[ServicesInitialized] ? Status::Ok : Status::Failed;
    }
    try
    {
        // This explicit runtime owns configuration independently of a CGUI
        // borrower. Legacy frontend sessions still own and close their own XML.
        m_frontendRuntimeResources = new FrontendRuntimeResources;
        auto& resources = *m_frontendRuntimeResources;
        auto& state = resources.snapshot;
        state[RuntimeVersion] = ApiVersion;
        state[RuntimePhase] = static_cast<std::uint32_t>(Phase::Starting);
        state[OwnerThread] = m_frontendCoreOwnerThread;
        state[ConfigurationReadOnly] = 1;
        if (!resources.xml.Open())
            throw std::runtime_error("Runtime configuration could not be opened");
        ++state[ConfigurationLoads];
        m_pXML = resources.xml.xml;
        m_pConfigFile = resources.xml.file;
        resources.cvarsAttempted = true;
        if (!m_ClientVariables.Load())
            throw std::runtime_error("Runtime CVars could not be loaded");
        resources.cvarsLoaded = true;
        m_ClientVariables.Set("locale", std::string("en_US"));
        resources.bindings = std::make_unique<CKeyBinds>(this);
        ++state[BindingManagerCreations];
        auto* root = m_pConfigFile->GetRootNode();
        if (!root)
            throw std::runtime_error("Runtime bindings configuration has no root");
        auto* configured = root->FindSubNode(CONFIG_NODE_KEYBINDS);
        if (configured && !resources.bindings->LoadConfiguredBindsFromXML(configured))
            throw std::runtime_error("Runtime configured bindings could not be loaded");
        ++state[BindingLoads];
        if (!FlushFrontendRuntimeBindings())
            throw std::runtime_error("Runtime bindings could not establish neutral history");
        state[ServicesInitialized] = 1;
        state[HiddenNeutralVerified] = 1;
        state[RuntimePhase] = static_cast<std::uint32_t>(Phase::Hidden);
        // Only an active view publishes aliases through the old frontend API.
        // CVars keep their actual nodes in this still-owned XML document.
        m_pXML = nullptr;
        m_pConfigFile = nullptr;
        return Status::Ok;
    }
    catch (...)
    {
        m_pXML = nullptr;
        m_pConfigFile = nullptr;
        if (m_frontendRuntimeResources)
        {
            auto& state = m_frontendRuntimeResources->snapshot;
            state[RuntimePhase] = static_cast<std::uint32_t>(Phase::Failed);
            state[LastFailure] = static_cast<std::uint32_t>(Failure::ServicesStart);
            state[CleanupPending] = 1;
        }
        // A partially opened factory or manager remains attached to this Core
        // until explicit owner-thread shutdown proves it can be released.
        return Status::Failed;
    }
}

DefinitiveFrontendRuntime::Snapshot CCore::ReadFrontendRuntimeServices() const noexcept
{
    using namespace DefinitiveFrontendRuntime;
    if (GetCurrentThreadId() != m_frontendCoreOwnerThread)
        return {};
    if (!m_frontendRuntimeResources)
    {
        Snapshot state{};
        state[RuntimeVersion] = ApiVersion;
        state[OwnerThread] = m_frontendCoreOwnerThread;
        state[RuntimePhase] = static_cast<std::uint32_t>(Phase::Stopped);
        state[ConfigurationReadOnly] = 1;
        // Construction can fail before allocating the configuration owner.
        // There is then no session resource to retire, not an invented load.
        state[CleanupComplete] = 1;
        return state;
    }
    const auto& resources = *m_frontendRuntimeResources;
    auto        state = resources.snapshot;
    state[XmlRetained] = resources.xml.module || resources.xml.xml;
    state[ConfigRetained] = resources.xml.file != nullptr;
    state[CVarsLoaded] = resources.cvarsLoaded;
    state[BindingsRetained] = resources.bindings != nullptr;
    state[ServicesRetained] = state[XmlRetained] || state[ConfigRetained] || state[CVarsLoaded] || state[BindingsRetained];
    state[ViewBorrowed] = m_frontendResources != nullptr;
    state[CleanupPending] = state[CleanupPending] || (m_frontendResources && m_frontendResources->snapshot[DefinitiveFrontend::CleanupPending]);
    const auto revision = static_cast<std::uint64_t>(static_cast<std::uint32_t>(const_cast<CClientVariables&>(m_ClientVariables).GetRevision()));
    state[CVarRevisionLow] = static_cast<std::uint32_t>(revision);
    state[CVarRevisionHigh] = static_cast<std::uint32_t>(revision >> 32);
    state[BindCount] = resources.bindings ? resources.bindings->Count() : 0;
    return state;
}

DefinitiveFrontend::Status CCore::StopFrontendRuntimeServices() noexcept
{
    using namespace DefinitiveFrontendRuntime;
    using Status = DefinitiveFrontend::Status;
    if (GetCurrentThreadId() != m_frontendCoreOwnerThread)
        return Status::WrongThread;
    const auto localStopped = StopLocalInputRuntime(true);
    if (localStopped != Status::Ok)
        return localStopped;
    if (!m_frontendRuntimeResources)
        return Status::Ok;
    if (m_frontendResources || m_pGUI || m_pLocalGUI || m_pXML || m_pConfigFile || m_pKeyBinds)
        return Status::Busy;
    auto& resources = *m_frontendRuntimeResources;
    auto& state = resources.snapshot;
    if (state[CleanupComplete])
        return Status::Ok;
    state[CleanupPending] = 1;
    state[RuntimePhase] = static_cast<std::uint32_t>(Phase::Stopping);
    try
    {
        if (resources.bindings)
        {
            if (!FlushFrontendRuntimeBindings())
                throw std::runtime_error("Runtime bindings retained nonneutral input");
            state[HiddenNeutralVerified] = 1;
            resources.bindings.reset();
        }
        // CVars borrow XML nodes. They are invalidated only after the actual
        // binding manager is gone, and before its factory/document is closed.
        if (resources.cvarsAttempted)
        {
            m_ClientVariables.Unload();
            resources.cvarsLoaded = false;
            resources.cvarsAttempted = false;
        }
        // Keep the actual factory handle for a later owner-thread retry if
        // unloading fails. The old short-lived reference uses its old policy.
        if (!resources.xml.Close(true))
            throw std::runtime_error("Runtime XML owner retained its module");
        state[CleanupPending] = 0;
        state[CleanupComplete] = 1;
        state[RuntimePhase] = static_cast<std::uint32_t>(Phase::Stopped);
        return Status::Ok;
    }
    catch (...)
    {
        state[LastFailure] = static_cast<std::uint32_t>(Failure::ServicesStop);
        state[RuntimePhase] = static_cast<std::uint32_t>(Phase::Failed);
        return Status::Failed;
    }
}

DefinitiveFrontend::Status CCore::RunFrontendStartupView(const DefinitiveInputReference::Request&       request,
                                                         DefinitiveGuiFrontendInputReference::Snapshot& result)
{
    // Construction has already succeeded for this owner. Reuse the same menu,
    // console, native neutral-input admission and retained GPU cleanup as the
    // diagnostic path, without executing the rejecting gameplay constructor.
    result = RunGuiFrontendReferenceImpl<true>(m_DefinitiveAdapter, request);
    using namespace DefinitiveGuiStartupReference;
    if (!result[CleanupComplete] || result[CleanupPending])
        return DefinitiveFrontend::Status::Busy;
    return result[WorkloadSucceeded] && !result[CleanupDeadlineExceeded] ? DefinitiveFrontend::Status::Ok : DefinitiveFrontend::Status::Failed;
}

DefinitiveFrontend::Status CCore::RunPersistentFrontend(const DefinitivePersistentFrontend::RequestV1& persistent,
                                                        DefinitivePersistentFrontend::Snapshot&        result)
{
    DefinitiveInputReference::Request request;
    // Native device acquisition remains bounded. Only the interactive phase
    // uses renewable host/native leases rather than the old absolute timer.
    request.durationMs = DefinitiveGuiFrontendInputReference::MaximumDurationMs;
    request.cancelled = persistent.cancelled;
    request.publish = persistent.publish;
    request.guiFrontendInput = true;
    result = RunGuiFrontendReferenceImpl<true, true>(m_DefinitiveAdapter, request, &persistent);
    using namespace DefinitiveGuiStartupReference;
    if (!result[CleanupComplete] || result[CleanupPending])
        return DefinitiveFrontend::Status::Busy;
    if (result[WorkloadSucceeded] && !result[CleanupDeadlineExceeded])
        return DefinitiveFrontend::Status::Ok;
    const auto stop = static_cast<DefinitivePersistentFrontend::Stop>(result[DefinitivePersistentFrontend::StopReason]);
    if (!result[CleanupDeadlineExceeded] &&
        (stop == DefinitivePersistentFrontend::Stop::Cancelled || stop == DefinitivePersistentFrontend::Stop::HostLeaseExpired ||
         stop == DefinitivePersistentFrontend::Stop::FocusLost))
        return DefinitiveFrontend::Status::Stopped;
    return DefinitiveFrontend::Status::Failed;
}

CCore::CCore(FrontendShellTag)
{
    // This successful Core object owns only frontend services. The ordinary
    // constructor and InitializeCore still require the real game backend.
    g_pCore = this;
    m_frontendShellOnly = true;
    m_pLocalGUI = nullptr;
    m_pGUI = nullptr;
    // Completed frontend objects do not run the classic constructor body.
    // Their existing public getters and menu snapshots must observe explicit
    // absent services rather than rely on allocator-zeroed storage.
    m_pGraphics = nullptr;
    m_pDirect3DData = nullptr;
    m_pConnectManager = nullptr;
    m_pModelCacheManager = nullptr;
    m_pTrayIcon = nullptr;
    m_pMessageLoopHook = nullptr;
    m_pDirectInputHookManager = nullptr;
    m_pDirect3DHookManager = nullptr;
    m_pSetCursorPosHook = nullptr;
    m_pModManager = nullptr;
    m_pNet = nullptr;
    m_pMultiplayer = nullptr;
    m_pMouseControl = nullptr;
    m_pfnMessageProcessor = nullptr;
    m_pMessageBox = nullptr;
    m_bIsOfflineMod = false;
    m_bCursorToggleControls = false;
    m_bQuitOnPulse = false;
    m_bDestroyMessageBox = false;
    m_bModulesLoaded = false;
}

CCore::CCore(FrontendStartupTag) : CCore(FrontendShellTag{})
{
    // Backend initialization follows construction, so a failed or retained
    // adapter remains owned by a completed, inspectable Core object. The GUI
    // controller only borrows this Core and cannot destroy its startup owner.
    m_frontendStartupOwned = true;
}

void CCore::FrontendInputOwnership(void* context, bool requested) noexcept
{
    auto* core = static_cast<CCore*>(context);
    if (core->m_frontendResources)
    {
        auto& resources = *core->m_frontendResources;
        resources.menuInputRequested = requested;
        // Closing one actual window cannot release another window's input.
        // Stopping Core overrides both claims while callbacks are detached.
        resources.snapshot[DefinitiveFrontend::InputRequested] =
            !resources.snapshot[DefinitiveFrontend::CleanupPending] && (resources.menuInputRequested || resources.consoleInputRequested) ? 1u : 0u;
    }
}

void CCore::FrontendConsoleInputOwnership(void* context, bool requested) noexcept
{
    auto* core = static_cast<CCore*>(context);
    if (core->m_frontendResources)
    {
        auto& resources = *core->m_frontendResources;
        resources.consoleInputRequested = requested;
        resources.snapshot[DefinitiveFrontend::InputRequested] =
            !resources.snapshot[DefinitiveFrontend::CleanupPending] && (resources.menuInputRequested || resources.consoleInputRequested) ? 1u : 0u;
    }
}

void CCore::FrontendCloseRequested(void* context) noexcept
{
    auto* core = static_cast<CCore*>(context);
    if (core->m_frontendResources)
        core->m_frontendResources->snapshot[DefinitiveFrontend::CloseRequested] = 1;
}

bool CCore::StartFrontend(CGUI* borrowedGui)
{
    using namespace DefinitiveFrontend;
    if (GetCurrentThreadId() != m_frontendCoreOwnerThread || (m_frontendStartupOwned && !m_frontendStartupAccepting) || !LocalInputAllowsFrontend() ||
        !borrowedGui || m_frontendResources || m_pGUI || m_pXML || m_pConfigFile || m_pLocalGUI || m_pLocalization || m_pCommands || m_pConsoleLogger ||
        m_pKeyBinds)
        return false;
    if (m_frontendRuntimeResources)
    {
        using namespace DefinitiveFrontendRuntime;
        const auto& state = m_frontendRuntimeResources->snapshot;
        const auto  generation = static_cast<std::uint64_t>(state[ViewGenerationLow]) | (static_cast<std::uint64_t>(state[ViewGenerationHigh]) << 32);
        if (!state[ServicesInitialized] || state[DefinitiveFrontendRuntime::CleanupPending] || state[DefinitiveFrontendRuntime::CleanupComplete] ||
            state[RuntimePhase] != static_cast<std::uint32_t>(DefinitiveFrontendRuntime::Phase::Hidden) || state[ViewsAttached] == UINT32_MAX ||
            generation == UINT64_MAX || !m_frontendRuntimeResources->bindings || !m_frontendRuntimeResources->xml.file ||
            !m_frontendRuntimeResources->cvarsLoaded)
            return false;
    }
    m_frontendResources = new FrontendResources;
    auto& state = *m_frontendResources;
    state.runtimeBorrowed = m_frontendRuntimeResources != nullptr;
    if (state.runtimeBorrowed)
    {
        using namespace DefinitiveFrontendRuntime;
        auto&      runtime = m_frontendRuntimeResources->snapshot;
        const auto generation = (static_cast<std::uint64_t>(runtime[ViewGenerationLow]) | (static_cast<std::uint64_t>(runtime[ViewGenerationHigh]) << 32)) + 1;
        runtime[ViewGenerationLow] = static_cast<std::uint32_t>(generation);
        runtime[ViewGenerationHigh] = static_cast<std::uint32_t>(generation >> 32);
        ++runtime[ViewsAttached];
        runtime[HiddenNeutralVerified] = 0;
        runtime[RuntimePhase] = static_cast<std::uint32_t>(DefinitiveFrontendRuntime::Phase::Visible);
    }
    state.snapshot[Version] = ApiVersion;
    state.snapshot[Phase] = 1;
    state.snapshot[OwnerThread] = GetCurrentThreadId();
    state.snapshot[OwnsCoreShell] = m_frontendShellOnly && !m_frontendStartupOwned ? 1u : 0u;
    state.snapshot[ConfigurationReadOnly] = 1;
    state.consoleSnapshot[DefinitiveFrontendConsole::Version] = DefinitiveFrontendConsole::ApiVersion;
    state.consoleSnapshot[DefinitiveFrontendConsole::OwnerThread] = GetCurrentThreadId();
    state.consoleSnapshot[DefinitiveFrontendConsole::CleanupComplete] = 1;
    state.consoleSnapshot[DefinitiveFrontendConsole::ConfigurationReadOnly] = 1;
    state.consoleSnapshot[DefinitiveFrontendConsole::MaxInputCharacters] = 255;
    state.bindingsSnapshot[DefinitiveFrontendBindings::Version] = DefinitiveFrontendBindings::ApiVersion;
    state.bindingsSnapshot[DefinitiveFrontendBindings::OwnerThread] = GetCurrentThreadId();
    state.bindingsSnapshot[DefinitiveFrontendBindings::CleanupComplete] = 1;
    state.bindingsSnapshot[DefinitiveFrontendBindings::ConfigurationReadOnly] = 1;
    state.previousLocalization = g_pLocalization;
    m_pGUI = borrowedGui;
    try
    {
        // Reuse the existing adjacent, recovery-disabled XML owner. Frontend
        // CVars remain real MTA XML nodes, but neither config nor global locale
        // preferences are saved by this bounded lifecycle.
        if (state.runtimeBorrowed)
        {
            // A fresh GUI borrows the same actual manager and configuration.
            // Applied binds live here, not in old widgets or a reloaded file.
            m_pXML = m_frontendRuntimeResources->xml.xml;
            m_pConfigFile = m_frontendRuntimeResources->xml.file;
            m_pKeyBinds = m_frontendRuntimeResources->bindings.get();
        }
        else
        {
            if (!state.xml.Open())
                throw std::runtime_error("Frontend XML/configuration could not be opened");
            m_pXML = state.xml.xml;
            m_pConfigFile = state.xml.file;
            state.snapshot[XmlLoaded] = state.snapshot[ConfigLoaded] = 1;
            if (!m_ClientVariables.Load())
                throw std::runtime_error("Frontend CVars could not be loaded");
            m_ClientVariables.Set("locale", std::string("en_US"));
        }
        state.snapshot[XmlLoaded] = state.snapshot[ConfigLoaded] = 1;
        const auto localeRoot = ToUTF8((g_mtaDirectory / L"MTA/locale").wstring());
        m_pLocalization = new CLocalization("en_US", localeRoot, false);
        g_pLocalization = m_pLocalization;
        if (m_pLocalization->GetLanguageDirectory().empty())
            throw std::runtime_error("Frontend localization assets are missing");
        state.snapshot[LocalizationLoaded] = 1;
        m_pConsoleLogger = new CConsoleLogger;
        m_pCommands = new CCommands(CCommands::Policy::FrontendLocal);
        if (FrontendBackgroundServices::AcquireUpdater(this) != FrontendBackgroundServices::Status::Ok)
            throw std::runtime_error("Frontend updater ownership is unavailable");
        state.updaterAcquired = true;
        borrowedGui->SetSkin("Default");
        if (!borrowedGui->GetDefaultFont())
            throw std::runtime_error("Frontend GUI fonts are unavailable");
        const CMainMenu::FrontendHost host{this, &CCore::FrontendInputOwnership, &CCore::FrontendCloseRequested};
        m_pLocalGUI = new CLocalGUI(&host);
        SString oldestNews;
        uint    maximumNews = 0;
        GetVersionUpdater()->GetNewsSettings(oldestNews, maximumNews);
        m_pLocalGUI->CreateWindows(false);
        state.snapshot[Phase] = 2;
        return true;
    }
    catch (...)
    {
        // Keep the failed phase and every owner inspectable. The controller
        // must explicitly retire them instead of throwing away a failed handle.
        state.snapshot[Phase] = 5;
        if (state.runtimeBorrowed)
            m_frontendRuntimeResources->snapshot[DefinitiveFrontendRuntime::LastFailure] = static_cast<std::uint32_t>(DefinitiveFrontendRuntime::Failure::View);
        return false;
    }
}

namespace DefinitiveFrontend
{
    struct Controller
    {
        DWORD  owner = GetCurrentThreadId();
        CCore* core = nullptr;
        bool   ownsCore = false;

        static Controller* CreateSession(CGUI* gui)
        {
            bool ownsStartupLock = false;
            if (!DefinitiveFrontendStartup::AcquireFrontendBorrow(ownsStartupLock))
                return nullptr;
            struct StartupBorrowPin
            {
                bool ownsLock;
                ~StartupBorrowPin() { DefinitiveFrontendStartup::ReleaseFrontendBorrow(ownsLock); }
            } pin{ownsStartupLock};
            // Pin singleton lookup through allocation and StartFrontend. The
            // startup owner cannot release Core or begin a native clock scope
            // between an admission check and this borrow becoming observable.
            auto*      existingCore = CCore::GetSingletonPtr();
            const auto ownerThread = GetCurrentThreadId();
            // A partial normal Core still owns its adapter and constructor
            // scope. Frontend adoption cannot move it onto a foreign thread.
            if (existingCore && existingCore->m_frontendCoreOwnerThread != ownerThread)
                return nullptr;
            // A completed startup Core remains inspectable after failure or
            // Shutdown, but that retention does not admit a new GUI borrower.
            if (existingCore && existingCore->m_frontendStartupOwned && !existingCore->m_frontendStartupAccepting)
                return nullptr;
            if (existingCore && !existingCore->LocalInputAllowsFrontend())
                return nullptr;
            auto controller = std::make_unique<Controller>();
            controller->core = existingCore;
            if (!controller->core)
            {
                controller->core = new CCore(CCore::FrontendShellTag{});
                controller->ownsCore = true;
            }
            try
            {
                controller->core->StartFrontend(gui);
                if (controller->core->m_frontendResources)
                    return controller.release();
            }
            catch (...)
            {
            }
            if (controller->ownsCore)
                delete controller->core;
            return nullptr;
        }
        Status                              PulseFrame(std::int32_t x, std::int32_t y, bool focused) { return core->PulseFrontend(x, y, focused); }
        Snapshot                            ReadState() const { return core->ReadFrontend(); }
        Status                              Stop() { return core->StopFrontend(); }
        Status                              EnableConsole() { return core->EnableFrontendConsole(); }
        DefinitiveFrontendConsole::Snapshot ReadConsole() { return core->ReadFrontendConsole(); }
        Status                              SetConsoleVisible(bool visible) { return core->SetFrontendConsoleVisible(visible); }
        Status                              SetMenuVisible(bool visible) { return core->SetFrontendMenuVisible(visible); }
        Status                              ConsoleMessage(std::uint32_t message, std::uintptr_t wparam, std::intptr_t lparam, std::uint32_t& handled)
        {
            return core->ProcessFrontendConsoleMessage(message, wparam, lparam, handled);
        }
        Status ConsoleText(std::uint32_t field, char* output, std::uint32_t capacity, std::uint32_t& required)
        {
            return core->ReadFrontendConsoleText(field, output, capacity, required);
        }
        Status                               EnableBindings() { return core->EnableFrontendBindings(); }
        DefinitiveFrontendBindings::Snapshot ReadBindings() { return core->ReadFrontendBindings(); }
        Status                               ProduceNeutral(mta::game_de::MtaInputState& output) { return core->ProduceFrontendNeutralInput(output); }
        Status                               Retire()
        {
            const auto status = Stop();
            if (status != Status::Ok)
                return status;
            if (core->m_frontendResources->runtimeBorrowed)
            {
                using namespace DefinitiveFrontendRuntime;
                auto& runtime = core->m_frontendRuntimeResources->snapshot;
                // A stopped but unreleased GUI still borrows this Core. Only
                // actual Release makes the next fresh view admissible.
                ++runtime[ViewsRetired];
                runtime[RuntimePhase] = static_cast<std::uint32_t>(DefinitiveFrontendRuntime::Phase::Hidden);
            }
            delete core->m_frontendResources;
            core->m_frontendResources = nullptr;
            if (ownsCore)
                delete core;
            core = nullptr;
            return Status::Ok;
        }
    };
    namespace
    {
        SRWLOCK     frontendLock = SRWLOCK_INIT;
        Controller* activeFrontend = nullptr;
        struct FrontendOperation
        {
            bool acquired = TryAcquireSRWLockExclusive(&frontendLock) != FALSE;
            ~FrontendOperation()
            {
                if (acquired)
                    ReleaseSRWLockExclusive(&frontendLock);
            }
            Status Validate(void* handle) const noexcept
            {
                if (!acquired)
                    return Status::Busy;
                if (!handle || handle != activeFrontend)
                    return Status::InvalidArgument;
                return activeFrontend->owner == GetCurrentThreadId() ? Status::Ok : Status::WrongThread;
            }
        };
    }
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEFrontendApiVersion()
{
    return DefinitiveFrontend::ApiVersion;
}
extern "C" __declspec(dllexport) void* MTA_CreateDEFrontendSession(std::uint32_t version, CGUI* gui)
{
    using namespace DefinitiveFrontend;
    if (version != ApiVersion || !gui)
        return nullptr;
    FrontendOperation operation;
    if (!operation.acquired || activeFrontend)
        return nullptr;
    try
    {
        activeFrontend = Controller::CreateSession(gui);
        return activeFrontend;
    }
    catch (...)
    {
        return nullptr;
    }
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_PulseDEFrontendSession(void* handle, std::int32_t x, std::int32_t y, std::uint32_t focused)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    if (focused > 1)
        return Status::InvalidArgument;
    return activeFrontend->PulseFrame(x, y, focused != 0);
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDEFrontendSession(void* handle, std::uint32_t* output, std::uint32_t count)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    if (!output || count != FieldCount)
        return Status::InvalidArgument;
    try
    {
        const auto snapshot = activeFrontend->ReadState();
        std::copy(snapshot.begin(), snapshot.end(), output);
        return Status::Ok;
    }
    catch (...)
    {
        return Status::Failed;
    }
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ShutdownDEFrontendSession(void* handle)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    return status == Status::Ok ? activeFrontend->Stop() : status;
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReleaseDEFrontendSession(void* handle)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    const auto retired = activeFrontend->Retire();
    if (retired == Status::Ok)
    {
        delete activeFrontend;
        activeFrontend = nullptr;
    }
    return retired;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEFrontendConsoleApiVersion()
{
    return DefinitiveFrontendConsole::ApiVersion;
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_EnableDEFrontendConsole(void* handle, std::uint32_t version)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    if (version != DefinitiveFrontendConsole::ApiVersion)
        return Status::InvalidArgument;
    return activeFrontend->EnableConsole();
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDEFrontendConsole(void* handle, std::uint32_t* output, std::uint32_t count)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    if (!output || count != DefinitiveFrontendConsole::FieldCount)
        return Status::InvalidArgument;
    try
    {
        const auto snapshot = activeFrontend->ReadConsole();
        std::copy(snapshot.begin(), snapshot.end(), output);
        return Status::Ok;
    }
    catch (...)
    {
        return Status::Failed;
    }
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_SetDEFrontendConsoleVisible(void* handle, std::uint32_t visible)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    return visible <= 1 ? activeFrontend->SetConsoleVisible(visible != 0) : Status::InvalidArgument;
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_SetDEFrontendMenuVisible(void* handle, std::uint32_t visible)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    return visible <= 1 ? activeFrontend->SetMenuVisible(visible != 0) : Status::InvalidArgument;
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ProcessDEFrontendConsoleMessage(void* handle, std::uint32_t message, std::uintptr_t wparam,
                                                                                                std::intptr_t lparam, std::uint32_t* handled)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    if (!handled || message > 0xffffu)
        return Status::InvalidArgument;
    return activeFrontend->ConsoleMessage(message, wparam, lparam, *handled);
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDEFrontendConsoleText(void* handle, std::uint32_t field, char* output,
                                                                                          std::uint32_t capacity, std::uint32_t* required)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    if (!required || field > static_cast<std::uint32_t>(DefinitiveFrontendConsole::Text::Output) || capacity > DefinitiveFrontendConsole::MaximumTextCapacity ||
        (!output && capacity) || (output && !capacity))
        return Status::InvalidArgument;
    return activeFrontend->ConsoleText(field, output, capacity, *required);
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEFrontendBindingsApiVersion()
{
    return DefinitiveFrontendBindings::ApiVersion;
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_EnableDEFrontendBindings(void* handle, std::uint32_t version)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    return version == DefinitiveFrontendBindings::ApiVersion ? activeFrontend->EnableBindings() : Status::InvalidArgument;
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDEFrontendBindings(void* handle, std::uint32_t* output, std::uint32_t count)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    if (!output || count != DefinitiveFrontendBindings::FieldCount)
        return Status::InvalidArgument;
    try
    {
        const auto snapshot = activeFrontend->ReadBindings();
        std::copy(snapshot.begin(), snapshot.end(), output);
        return Status::Ok;
    }
    catch (...)
    {
        return Status::Failed;
    }
}

extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ProduceDEFrontendNeutralInput(void* handle, mta::game_de::MtaInputState* output,
                                                                                              std::uint32_t bytes)
{
    using namespace DefinitiveFrontend;
    FrontendOperation operation;
    const auto        status = operation.Validate(handle);
    if (status != Status::Ok)
        return status;
    if (!output || bytes != sizeof(*output))
        return Status::InvalidArgument;
    return activeFrontend->ProduceNeutral(*output);
}

DefinitiveFrontend::Status CCore::EnableFrontendBindings()
{
    using Status = DefinitiveFrontend::Status;
    using namespace DefinitiveFrontendBindings;
    if (!m_frontendResources)
        return Status::InvalidArgument;
    auto& resources = *m_frontendResources;
    auto& state = resources.bindingsSnapshot;
    if (resources.snapshot[DefinitiveFrontend::Phase] != 2 || !m_pConfigFile)
        return Status::Stopped;
    if (state[Phase] == static_cast<std::uint32_t>(Lifecycle::Running))
        return Status::Ok;
    if (state[ReleaseFault] || (m_pKeyBinds && !resources.runtimeBorrowed))
        return Status::Failed;
    resources.bindingsEnabled = true;
    state[Phase] = static_cast<std::uint32_t>(Lifecycle::Starting);
    state[CleanupComplete] = 0;
    state[CVarRevision] = static_cast<std::uint32_t>(m_ClientVariables.GetRevision());
    try
    {
        // A runtime view borrows its owner's actual manager. Recreating or
        // reloading it here would discard applied edits on every fresh GUI.
        if (resources.runtimeBorrowed)
        {
            if (!m_frontendRuntimeResources || m_pKeyBinds != m_frontendRuntimeResources->bindings.get())
                throw std::runtime_error("Runtime view lost its actual binding manager");
        }
        else
        {
            // Configured records avoid GTA defaults, CGame and command dispatch.
            m_pKeyBinds = new CKeyBinds(this);
            state[BindingsCreated] = 1;
            auto* root = m_pConfigFile->GetRootNode();
            if (!root)
                throw std::runtime_error("Frontend bindings configuration has no root");
            auto* configured = root->FindSubNode(CONFIG_NODE_KEYBINDS);
            if (configured && !m_pKeyBinds->LoadConfiguredBindsFromXML(configured))
                throw std::runtime_error("Frontend configured bindings could not be loaded");
        }
        state[BindingsCreated] = 1;
        // A missing configured list is an empty list, never admission to
        // discover classic GTA defaults or start a gameplay input producer.
        state[ConfigurationLoaded] = 1;
        state[BindCount] = m_pKeyBinds->Count();
        state[Phase] = static_cast<std::uint32_t>(Lifecycle::Running);
        mta::game_de::MtaInputState neutral;
        if (ProduceFrontendNeutralInput(neutral) != Status::Ok)
            throw std::runtime_error("Frontend bindings did not produce neutral input");
        // The existing settings UI may edit this session's real manager only
        // after configured input is admitted. Its frontend mode never reaches
        // classic game settings, joystick polling or configuration saves.
        if (!m_pLocalGUI || !m_pLocalGUI->GetMainMenu())
            throw std::runtime_error("Frontend bindings settings require the owned menu");
        m_pLocalGUI->GetMainMenu()->EnableFrontendBindingsSettings();
        return Status::Ok;
    }
    catch (...)
    {
        state[Phase] = static_cast<std::uint32_t>(Lifecycle::Failed);
        if (!resources.runtimeBorrowed)
            SAFE_DELETE(m_pKeyBinds);
        state[CleanupComplete] = resources.runtimeBorrowed ? 0u : 1u;
        return Status::Failed;
    }
}

DefinitiveFrontendBindings::Snapshot CCore::ReadFrontendBindings()
{
    using namespace DefinitiveFrontendBindings;
    if (!m_frontendResources)
        return {};
    const auto& resources = *m_frontendResources;
    auto        state = resources.bindingsSnapshot;
    state[BindingsRetained] = m_pKeyBinds != nullptr;
    state[InputSuppressed] = resources.snapshot[DefinitiveFrontend::InputRequested];
    state[NetworkStarted] = m_pNet != nullptr;
    state[ReleaseFault] = state[ReleaseFault] || resources.snapshot[DefinitiveFrontend::ReleaseFault];
    if (m_pKeyBinds)
        state[BindCount] = m_pKeyBinds->Count();
    if (resources.snapshot[DefinitiveFrontend::Phase] == 2)
        state[CVarRevision] = static_cast<std::uint32_t>(m_ClientVariables.GetRevision());
    return state;
}

DefinitiveFrontend::Status CCore::ProduceFrontendNeutralInput(mta::game_de::MtaInputState& output, bool cleanup)
{
    using Status = DefinitiveFrontend::Status;
    using namespace DefinitiveFrontendBindings;
    if (!m_frontendResources || !m_frontendResources->bindingsEnabled || !m_pKeyBinds)
        return Status::Stopped;
    auto& resources = *m_frontendResources;
    auto& state = resources.bindingsSnapshot;
    if (!cleanup && (resources.snapshot[DefinitiveFrontend::Phase] != 2 || state[Phase] != static_cast<std::uint32_t>(Lifecycle::Running)))
        return Status::Stopped;
    try
    {
        // Configured XML can contain command records, but this producer never
        // takes the mixed GUI/script/function dispatch path. Refuse any later
        // function registration before a control release could invoke it.
        if (m_pKeyBinds->Count(KeyBindType::FUNCTION) || m_pKeyBinds->Count(KeyBindType::CONTROL_FUNCTION))
            throw std::runtime_error("Frontend input cannot invoke function bindings");
        const auto poll = m_pKeyBinds->PollConfiguredControlKeys(false, nullptr, nullptr, MovementActions);
        const auto frame = m_pKeyBinds->BuildConfiguredControlFrame({});
        m_pKeyBinds->FinishConfiguredControlFrame();
        mta::game_de::MtaInputState next;
        next.current = frame.controller;
        next.previous = resources.neutralInput.current;
        const CControllerState zero;
        state[CurrentZero] = !frame.heldActions && std::memcmp(&next.current, &zero, sizeof(zero)) == 0;
        state[PreviousZero] = std::memcmp(&next.previous, &zero, sizeof(zero)) == 0;
        if (!state[CurrentZero] || !state[PreviousZero])
            throw std::runtime_error("Frontend input synthesis was not neutral");
        state[SupportedBindings] = poll.supportedBindings;
        state[IgnoredBindings] = poll.ignoredBindings;
        state[EligibleKeys] = poll.eligibleKeys;
        state[UnsupportedKeys] = poll.unsupportedKeys;
        if (state[NeutralFrames] != UINT32_MAX)
            ++state[NeutralFrames];
        resources.neutralInput = next;
        output = next;
        return Status::Ok;
    }
    catch (...)
    {
        state[Phase] = static_cast<std::uint32_t>(Lifecycle::Failed);
        return Status::Failed;
    }
}

DefinitiveFrontend::Status CCore::EnableFrontendConsole()
{
    using Status = DefinitiveFrontend::Status;
    using namespace DefinitiveFrontendConsole;
    if (!m_frontendResources)
        return Status::InvalidArgument;
    auto& resources = *m_frontendResources;
    auto& state = resources.consoleSnapshot;
    if (resources.snapshot[DefinitiveFrontend::Phase] != 2 || !m_pLocalGUI || !m_pCommands || !m_pCommands->IsFrontendLocal())
        return Status::Stopped;
    if (state[Phase] == static_cast<std::uint32_t>(Lifecycle::Running))
        return Status::Ok;
    if (state[ReleaseFault] || m_pLocalGUI->GetConsole())
        return Status::Failed;
    resources.consoleEnabled = true;
    state[Phase] = static_cast<std::uint32_t>(Lifecycle::Starting);
    state[CleanupComplete] = 0;
    state[CVarRevision] = static_cast<std::uint32_t>(m_ClientVariables.GetRevision());
    try
    {
        // Register only ordinary handlers whose dependencies already exist.
        // The normal ver/time/bind/network commands require later startup.
        m_pCommands->DeleteAll();
        m_pCommands->Add("help", _("this help screen"), CCommandFuncs::Help);
        m_pCommands->Add("clear", _("clears the console"), CCommandFuncs::Clear);
        if (m_pCommands->Count() != 2)
            throw std::runtime_error("Frontend console command registration failed");
        const CConsole::FrontendHost host{this, &CCore::FrontendConsoleInputOwnership};
        m_pLocalGUI->EnableFrontendConsole(host);
        state[ConsoleCreated] = 1;
        state[Phase] = static_cast<std::uint32_t>(Lifecycle::Running);
        return Status::Ok;
    }
    catch (...)
    {
        state[Phase] = static_cast<std::uint32_t>(Lifecycle::Failed);
        try
        {
            m_pLocalGUI->DestroyFrontendConsole();
            m_pCommands->DeleteAll();
            FrontendConsoleInputOwnership(this, false);
            state[CleanupComplete] = 1;
        }
        catch (...)
        {
            // Keep the optional failed owner inspectable rather than releasing
            // a borrowed GUI around callbacks that could still be registered.
            state[ReleaseFault] = 1;
        }
        return Status::Failed;
    }
}

DefinitiveFrontendConsole::Snapshot CCore::ReadFrontendConsole()
{
    using namespace DefinitiveFrontendConsole;
    if (!m_frontendResources)
        return {};
    auto& resources = *m_frontendResources;
    auto  state = resources.consoleSnapshot;
    auto* console = m_pLocalGUI ? m_pLocalGUI->GetConsole() : nullptr;
    state[ConsoleRetained] = console != nullptr;
    state[CommandsRetained] = resources.consoleEnabled && m_pCommands && m_pCommands->Count() != 0;
    state[CommandPolicyActive] = state[CommandsRetained] && m_pCommands->IsFrontendLocal();
    state[CommandsRegistered] = resources.consoleEnabled && m_pCommands ? m_pCommands->Count() : 0;
    state[MenuInputRequested] = resources.menuInputRequested ? 1u : 0u;
    state[ConsoleInputRequested] = resources.consoleInputRequested ? 1u : 0u;
    state[AggregateInputRequested] = resources.snapshot[DefinitiveFrontend::InputRequested];
    state[ConsoleVisible] = console && console->IsVisible();
    state[InputActive] = console && console->IsInputActive();
    state[NetworkStarted] = m_pNet != nullptr;
    state[ReleaseFault] = state[ReleaseFault] || resources.snapshot[DefinitiveFrontend::ReleaseFault];
    if (resources.snapshot[DefinitiveFrontend::Phase] == 2)
        state[CVarRevision] = static_cast<std::uint32_t>(m_ClientVariables.GetRevision());
    if (console)
    {
        state[TextAccepted] = console->GetAcceptedCount();
        state[HistoryEntries] = console->GetHistoryCount();
        state[HistorySelections] = console->GetHistorySelections();
        state[AutoCompleteUses] = console->GetAutoCompleteUses();
        state[OutputBytes] = static_cast<std::uint32_t>(console->GetOutputText().size());
        state[InputBytes] = static_cast<std::uint32_t>(console->GetInputText().size());
    }
    if (resources.consoleEnabled && m_pCommands)
    {
        state[CommandsExecuted] = m_pCommands->GetFrontendExecuted();
        state[CommandsRejected] = m_pCommands->GetFrontendRejected();
        state[LastCommand] = m_pCommands->GetFrontendLastCommand();
    }
    return state;
}

DefinitiveFrontend::Status CCore::SetFrontendConsoleVisible(bool visible)
{
    using Status = DefinitiveFrontend::Status;
    if (!m_frontendResources || m_frontendResources->snapshot[DefinitiveFrontend::Phase] != 2 ||
        m_frontendResources->consoleSnapshot[DefinitiveFrontendConsole::Phase] != 2 || !m_pLocalGUI || !m_pLocalGUI->GetConsole())
        return Status::Stopped;
    try
    {
        m_pLocalGUI->SetConsoleVisible(visible);
        return Status::Ok;
    }
    catch (...)
    {
        m_frontendResources->consoleSnapshot[DefinitiveFrontendConsole::Phase] = 5;
        return Status::Failed;
    }
}

DefinitiveFrontend::Status CCore::SetFrontendMenuVisible(bool visible)
{
    using Status = DefinitiveFrontend::Status;
    if (!m_frontendResources || m_frontendResources->snapshot[DefinitiveFrontend::Phase] != 2 ||
        m_frontendResources->consoleSnapshot[DefinitiveFrontendConsole::Phase] != 2 || !m_pLocalGUI || !m_pLocalGUI->GetMainMenu())
        return Status::Stopped;
    try
    {
        m_pLocalGUI->SetMainMenuVisible(visible);
        return Status::Ok;
    }
    catch (...)
    {
        m_frontendResources->consoleSnapshot[DefinitiveFrontendConsole::Phase] = 5;
        return Status::Failed;
    }
}

DefinitiveFrontend::Status CCore::ProcessFrontendConsoleMessage(std::uint32_t message, std::uintptr_t wparam, std::intptr_t lparam, std::uint32_t& handled)
{
    using Status = DefinitiveFrontend::Status;
    if (!m_frontendResources || m_frontendResources->snapshot[DefinitiveFrontend::Phase] != 2 ||
        m_frontendResources->consoleSnapshot[DefinitiveFrontendConsole::Phase] != 2 || !m_pLocalGUI || !m_pLocalGUI->GetConsole())
        return Status::Stopped;
    try
    {
        const bool consumed = m_pLocalGUI->ProcessFrontendConsoleMessage(message, static_cast<WPARAM>(wparam), static_cast<LPARAM>(lparam));
        handled = consumed ? 1u : 0u;
        return Status::Ok;
    }
    catch (...)
    {
        m_frontendResources->consoleSnapshot[DefinitiveFrontendConsole::Phase] = 5;
        return Status::Failed;
    }
}

DefinitiveFrontend::Status CCore::ReadFrontendConsoleText(std::uint32_t field, char* output, std::uint32_t capacity, std::uint32_t& required)
{
    using Status = DefinitiveFrontend::Status;
    auto* console = m_pLocalGUI ? m_pLocalGUI->GetConsole() : nullptr;
    if (!console)
        return Status::Stopped;
    try
    {
        const auto text = field == static_cast<std::uint32_t>(DefinitiveFrontendConsole::Text::Input) ? console->GetInputText() : console->GetOutputText();
        if (text.size() >= UINT32_MAX)
            return Status::Failed;
        const auto bytes = static_cast<std::uint32_t>(text.size()) + 1;
        required = bytes;
        if (!capacity)
            return Status::Ok;
        if (capacity < bytes)
            return Status::InvalidArgument;
        std::memcpy(output, text.c_str(), bytes);
        return Status::Ok;
    }
    catch (...)
    {
        return Status::Failed;
    }
}

DefinitiveFrontend::Status CCore::PulseFrontend(std::int32_t mouseX, std::int32_t mouseY, bool focused)
{
    using namespace DefinitiveFrontend;
    if (!m_frontendResources)
        return Status::InvalidArgument;
    auto& state = m_frontendResources->snapshot;
    if (state[Phase] != 2)
        return state[Phase] == 5 ? Status::Failed : Status::Stopped;
    try
    {
        m_pLocalGUI->PulseFrontend({mouseX, mouseY, focused});
        if (state[Frames] != UINT32_MAX)
            ++state[Frames];
        return Status::Ok;
    }
    catch (...)
    {
        state[Phase] = 5;
        return Status::Failed;
    }
}

DefinitiveFrontend::Snapshot CCore::ReadFrontend() const
{
    using namespace DefinitiveFrontend;
    if (!m_frontendResources)
        return {};
    const auto& resources = *m_frontendResources;
    auto        state = resources.snapshot;
    if (m_pLocalGUI && m_pLocalGUI->GetMainMenu())
    {
        auto* menu = m_pLocalGUI->GetMainMenu();
        state[MenuVisible] = menu->IsVisible() ? 1u : 0u;
        state[MenuFading] = menu->IsFading() ? 1u : 0u;
        state[CreditsVisible] = menu->IsCreditsVisible() ? 1u : 0u;
        state[AboutClicks] = menu->GetFrontendAboutClicks();
    }
    FrontendBackgroundServices::UpdaterSnapshot updater;
    if (resources.updaterAcquired && FrontendBackgroundServices::ReadUpdater(this, updater) == FrontendBackgroundServices::Status::Ok)
    {
        state[UpdaterCreated] = updater.instanceCreated;
        state[UpdaterWorkerStarted] = updater.workerStarted;
        state[BackgroundRequestsRejected] = static_cast<std::uint32_t>(std::min<std::uint64_t>(updater.backgroundRequestsRejected, UINT32_MAX));
        state[UpdaterConfigLoaded] = updater.configLoaded;
    }
    state[ServicesRetained] =
        m_pLocalGUI || m_pLocalization || m_pConsoleLogger || m_pCommands || m_pKeyBinds || resources.xml.module || resources.updaterAcquired;
    state[GuiBorrowed] = m_pGUI != nullptr;
    state[AdapterStarted] = m_DefinitiveAdapter.IsStarted() ? 1u : 0u;
    state[NetworkStarted] = m_pNet != nullptr;
    // A running menu does not grant CGame, multiplayer or full-client readiness.
    return state;
}

DefinitiveFrontend::Status CCore::StopFrontend()
{
    using namespace DefinitiveFrontend;
    if (!m_frontendResources)
        return Status::InvalidArgument;
    auto& resources = *m_frontendResources;
    auto& state = resources.snapshot;
    if (state[CleanupComplete])
        return Status::Ok;
    if (state[ReleaseFault])
        return Status::Failed;
    state[CleanupPending] = 1;
    state[Phase] = 3;
    if (resources.runtimeBorrowed)
        m_frontendRuntimeResources->snapshot[DefinitiveFrontendRuntime::RuntimePhase] = static_cast<std::uint32_t>(DefinitiveFrontendRuntime::Phase::Stopping);
    try
    {
        // Capture retains grid row pointers and an ordinary Core message box.
        // Retire both while the menu, real bindings and borrowed GUI still
        // exist; a completed frontend skips classic destructor cleanup.
        if (m_pLocalGUI && m_pLocalGUI->GetMainMenu())
            m_pLocalGUI->GetMainMenu()->CancelFrontendBindingsCapture();
        RemoveMessageBox(false);
        m_bDestroyMessageBox = false;
        if (resources.consoleEnabled)
        {
            // Preserve real parser/history/output evidence before deleting
            // widgets and commands. Live ownership fields are cleared below.
            resources.consoleSnapshot = ReadFrontendConsole();
            resources.consoleSnapshot[DefinitiveFrontendConsole::Phase] = 3;
            resources.consoleSnapshot[DefinitiveFrontendConsole::CleanupComplete] = 0;
            if (resources.consoleSnapshot[DefinitiveFrontendConsole::ReleaseFault])
                throw std::runtime_error("Frontend console cleanup ownership is unresolved");
        }
        if (resources.bindingsEnabled)
        {
            resources.bindingsSnapshot = ReadFrontendBindings();
            resources.bindingsSnapshot[DefinitiveFrontendBindings::Phase] = 3;
            resources.bindingsSnapshot[DefinitiveFrontendBindings::CleanupComplete] = 0;
            if (m_pKeyBinds)
            {
                // The native owner ends its scope before this CPU callback.
                // Flush MTA's own released controls while all services still
                // exist, then retire bindings before their XML configuration.
                mta::game_de::MtaInputState neutral;
                if (ProduceFrontendNeutralInput(neutral, true) != Status::Ok)
                    throw std::runtime_error("Frontend bindings could not flush neutral input");
                if (!resources.runtimeBorrowed)
                    SAFE_DELETE(m_pKeyBinds);
            }
        }
        if (resources.runtimeBorrowed && !FlushFrontendRuntimeBindings())
            throw std::runtime_error("Runtime view bindings could not release held input");
        // Stop input first. CPU widget destruction is supported while the GUI
        // renderer retains submitted texture versions; the external GUI/GPU
        // owner must still drain its session before unloading cgui or Core.
        state[InputRequested] = 0;
        resources.menuInputRequested = resources.consoleInputRequested = false;
        if (m_pLocalGUI)
            m_pLocalGUI->ResetInputState();
        if (m_pGUI)
            m_pGUI->SetCursorEnabled(false);
        if (m_pLocalGUI && m_pLocalGUI->GetMainMenu())
        {
            state[AboutClicks] = m_pLocalGUI->GetMainMenu()->GetFrontendAboutClicks();
            state[CreditsVisible] = m_pLocalGUI->GetMainMenu()->IsCreditsVisible() ? 1u : 0u;
        }
        SAFE_DELETE(m_pLocalGUI);
        if (resources.updaterAcquired)
        {
            FrontendBackgroundServices::UpdaterSnapshot updater;
            if (FrontendBackgroundServices::ReadUpdater(this, updater) == FrontendBackgroundServices::Status::Ok)
            {
                state[UpdaterCreated] = updater.instanceCreated;
                state[UpdaterWorkerStarted] = updater.workerStarted;
                state[BackgroundRequestsRejected] = static_cast<std::uint32_t>(std::min<std::uint64_t>(updater.backgroundRequestsRejected, UINT32_MAX));
                state[UpdaterConfigLoaded] = updater.configLoaded;
            }
            const auto status = FrontendBackgroundServices::ReleaseUpdater(this);
            if (status == FrontendBackgroundServices::Status::Busy)
                return Status::Busy;
            if (status != FrontendBackgroundServices::Status::Ok)
                throw std::runtime_error("Frontend updater did not quiesce");
            resources.updaterAcquired = false;
        }
        DestroyFrontendCommands();
        resources.consoleSnapshot[DefinitiveFrontendConsole::CommandsRegistered] = 0;
        resources.consoleSnapshot[DefinitiveFrontendConsole::CommandsRetained] = 0;
        resources.consoleSnapshot[DefinitiveFrontendConsole::CommandPolicyActive] = 0;
        if (g_pLocalization == m_pLocalization)
            g_pLocalization = resources.previousLocalization;
        SAFE_DELETE(m_pLocalization);
        // Runtime CVars still borrow the parked session document. Only legacy
        // per-view configuration is unloaded here; final runtime Stop owns
        // the actual manager, its CVars nodes, then the XML factory lifetime.
        if (!resources.runtimeBorrowed)
            m_ClientVariables.Unload();
        m_pConfigFile = nullptr;
        m_pXML = nullptr;
        m_pKeyBinds = nullptr;
        if (!resources.runtimeBorrowed && !resources.xml.Close())
            throw std::runtime_error("Frontend XML owner retained its module");
        DestroyFrontendLogger();
        m_pGUI = nullptr;
        state[MenuVisible] = state[MenuFading] = state[CreditsVisible] = 0;
        state[CleanupPending] = 0;
        state[CleanupComplete] = 1;
        state[Phase] = 4;
        if (resources.runtimeBorrowed)
            m_frontendRuntimeResources->snapshot[DefinitiveFrontendRuntime::HiddenNeutralVerified] = 1;
        if (resources.consoleEnabled)
        {
            resources.consoleSnapshot[DefinitiveFrontendConsole::ConsoleRetained] = 0;
            resources.consoleSnapshot[DefinitiveFrontendConsole::ConsoleVisible] = 0;
            resources.consoleSnapshot[DefinitiveFrontendConsole::InputActive] = 0;
            resources.consoleSnapshot[DefinitiveFrontendConsole::CleanupComplete] = 1;
            resources.consoleSnapshot[DefinitiveFrontendConsole::Phase] = 4;
        }
        if (resources.bindingsEnabled)
        {
            resources.bindingsSnapshot[DefinitiveFrontendBindings::InputSuppressed] = 0;
            resources.bindingsSnapshot[DefinitiveFrontendBindings::BindingsRetained] = 0;
            resources.bindingsSnapshot[DefinitiveFrontendBindings::CleanupComplete] = 1;
            resources.bindingsSnapshot[DefinitiveFrontendBindings::Phase] = 4;
        }
        return Status::Ok;
    }
    catch (...)
    {
        state[ReleaseFault] = 1;
        if (resources.consoleEnabled)
            resources.consoleSnapshot[DefinitiveFrontendConsole::ReleaseFault] = 1;
        if (resources.bindingsEnabled)
            resources.bindingsSnapshot[DefinitiveFrontendBindings::ReleaseFault] = 1;
        return Status::Failed;
    }
}
#endif

CCore::CCore()
{
    // Initialize the global pointer
    g_pCore = this;

#ifdef MTA_WITH_DEFINITIVE_ADAPTER
    // MTA owns the DE module before any classic configuration, hooks or game
    // factories run. Observations alone cannot satisfy CGame; reject incomplete
    // backends here rather than applying San Andreas addresses to a DE process.
    #if defined(_WIN64)
    DefinitiveInputReference::Request inputReference;
    const bool                        hasInputReference = DefinitiveInputReference::GetActiveRequest(inputReference);
    bool                              clockEntered = false;
    #endif
    try
    {
        if (!m_DefinitiveAdapter.StartAdjacent(this))
            throw std::runtime_error("DE adapter startup: " + m_DefinitiveAdapter.GetError());
        DefinitiveStartup::SetCleanupComplete(false);
        DefinitiveStartup::SetPhase(DefinitiveStartup::Phase::BackendStarted);
        DefinitiveStartup::SetCapabilities(m_DefinitiveAdapter.GetCapabilities());
        // The entry point only recognizes the DE filename. Bind here, outside
        // loader lock, so the backend verifies the exact image before engine access.
        const bool bound = m_DefinitiveAdapter.BindNativeInput();
        DefinitiveStartup::SetCapabilities(m_DefinitiveAdapter.GetCapabilities());
        if (!bound)
            throw std::runtime_error("DE adapter input binding: " + m_DefinitiveAdapter.GetError());
        DefinitiveStartup::SetPhase(DefinitiveStartup::Phase::ExactImageBound);
        if (!m_DefinitiveAdapter.Pulse())
            throw std::runtime_error("DE adapter pulse: " + m_DefinitiveAdapter.GetError());
        DefinitiveStartup::SetPhase(DefinitiveStartup::Phase::PulseComplete);
    #if defined(_WIN64)
        if (hasInputReference)
        {
            if (inputReference.startupObservation)
            {
                clockEntered = true;
                DefinitiveStartupObservationReference::Run(m_DefinitiveAdapter, inputReference);
            }
            else if (inputReference.applicationFrameReference)
            {
                clockEntered = true;
                DefinitiveApplicationFrameReference::Run(m_DefinitiveAdapter, inputReference);
            }
            else if (inputReference.clockSetReference)
            {
                clockEntered = true;
                DefinitiveClockSetReference::Run(m_DefinitiveAdapter, inputReference);
            }
            else if (inputReference.clockReference)
            {
                clockEntered = true;
                DefinitiveClockReference::Run(m_DefinitiveAdapter, inputReference);
            }
            else if (inputReference.guiFrontendInput)
                RunGuiFrontendReferenceImpl<true>(m_DefinitiveAdapter, inputReference);
            else if (inputReference.guiFrontend)
                RunGuiFrontendReferenceImpl<false>(m_DefinitiveAdapter, inputReference);
            else if (inputReference.guiSurface)
                RunGuiSurfaceReference(m_DefinitiveAdapter, inputReference);
            else if (inputReference.guiStartup)
                RunGuiStartupReference(m_DefinitiveAdapter, inputReference);
            else if (inputReference.renderObservation)
                RunRenderReference(m_DefinitiveAdapter, inputReference);
            else
                RunConfiguredInputReference(this, m_DefinitiveAdapter, inputReference);
        }
    #endif
        const bool gameplayReady = m_DefinitiveAdapter.IsGameplayReady();
        DefinitiveStartup::SetGameplayReady(gameplayReady);
        DefinitiveStartup::SetPhase(DefinitiveStartup::Phase::GameplayChecked);
        if (!gameplayReady)
            throw std::runtime_error("DE adapter gameplay unavailable: " + m_DefinitiveAdapter.GameplayUnavailableReason());

        // A complete game facade will still require a ported core, multiplayer and
        // renderer. Keep this gate until those startup paths cease using x86/D3D9.
        throw std::runtime_error("DE core, multiplayer and rendering startup are not implemented");
    }
    catch (...)
    {
        // Stop before rethrow so diagnostics report the owned adapter lifetime,
        // including failed teardown, rather than a potentially pinned DLL name.
        m_DefinitiveAdapter.Stop();
    #if defined(_WIN64)
        if (hasInputReference &&
            (inputReference.clockReference || inputReference.clockSetReference || inputReference.applicationFrameReference ||
             inputReference.startupObservation) &&
            !clockEntered)
        {
            // Binding can refuse before the clock workload begins. Keep this
            // Core owner alive until adapter retirement, then publish that
            // distinct proof rather than inventing a native clock snapshot.
            while (m_DefinitiveAdapter.IsStarted())
            {
                Sleep(DefinitiveClockReference::CleanupBackoffMs);
                m_DefinitiveAdapter.Stop();
            }
            if (inputReference.startupObservation)
                DefinitiveStartupObservationReference::PublishNotEntered(inputReference, true);
            else if (inputReference.applicationFrameReference)
                DefinitiveApplicationFrameReference::PublishNotEntered(inputReference, true);
            else if (inputReference.clockSetReference)
                DefinitiveClockSetReference::PublishNotEntered(inputReference, true);
            else
                DefinitiveClockReference::PublishNotEntered(inputReference, true);
        }
    #endif
        DefinitiveStartup::SetCleanupComplete(!m_DefinitiveAdapter.IsStarted());
        throw;
    }
#endif

    InitializeFrontendInfrastructure();

    // Connection management still belongs to classic startup: its callbacks
    // require the game, multiplayer and local GUI that a DE frontend lacks.
    m_pConnectManager = new CConnectManager;

    // Create the GUI manager and the graphics lib wrapper
    m_pLocalGUI = new CLocalGUI;
    m_pGraphics = new CGraphics(m_pLocalGUI);
    g_pGraphics = m_pGraphics;
    m_pGUI = NULL;

    // Create the mod manager
    m_pModManager = new CModManager;

    CCrashDumpWriter::SetHandlers();

    m_pfnMessageProcessor = NULL;
    m_pMessageBox = NULL;

    m_bIsOfflineMod = false;
    m_bQuitOnPulse = false;
    m_bDestroyMessageBox = false;
    m_bCursorToggleControls = false;
    m_bLastFocused = true;
    m_uiNextRenderTargetRetryTime = 0;
    m_DiagnosticDebug = EDiagnosticDebug::NONE;

    // Create our Direct3DData handler.
    m_pDirect3DData = new CDirect3DData;

    WriteDebugEvent("CCore::CCore");

    // Store initial module bases (will be updated more comprehensively later)
    WerCrash::UpdateModuleBases();

    m_pKeyBinds = new CKeyBinds(this);

    m_pMouseControl = new CMouseControl();

    // Create our hook objects.
    m_pDirect3DHookManager = new CDirect3DHookManager();
    m_pDirectInputHookManager = new CDirectInputHookManager();
    m_pMessageLoopHook = new CMessageLoopHook();
    m_pSetCursorPosHook = new CSetCursorPosHook();

    // Register internal commands.
    RegisterCommands();

    // Setup our hooks.
    ApplyHooks();

    m_pModelCacheManager = nullptr;
    m_iDummyProgressValue = 0;
    m_DummyProgressTimerHandle = NULL;
    m_bDummyProgressUpdateAlways = false;

    m_iUnminimizeFrameCounter = 0;
    m_bDidRecreateRenderTargets = false;
    m_fMinStreamingMemory = 0;
    m_fMaxStreamingMemory = 0;
    m_bGettingIdleCallsFromMultiplayer = false;
    m_bWindowsTimerEnabled = false;
    m_timeDiscordAppLastUpdate = 0;

    // Initialize FPS limiter
    m_pFPSLimiter = std::make_unique<FPSLimiter::FPSLimiter>();

    // Create tray icon
    m_pTrayIcon = new CTrayIcon();
    m_steamClient = std::make_unique<CSteamClient>();

    // Create discord rich presence
    m_pDiscordRichPresence = std::shared_ptr<CDiscordRichPresence>(new CDiscordRichPresence());
}

CCore::~CCore()
{
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    if (m_frontendShellOnly)
    {
        // Only the controller can destroy this shell after its explicit
        // frontend stop; classic hooks and settings were never constructed.
        assert(!m_frontendResources);
        assert(!m_localInputRuntimeResources ||
               (!m_localInputRuntimeResources->nativePending && m_localInputRuntimeResources->snapshot[DefinitiveLocalInputRuntime::CleanupComplete]));
        delete m_localInputRuntimeResources;
        m_localInputRuntimeResources = nullptr;
        assert(!m_frontendRuntimeResources || m_frontendRuntimeResources->snapshot[DefinitiveFrontendRuntime::CleanupComplete]);
        delete m_frontendRuntimeResources;
        m_frontendRuntimeResources = nullptr;
        if (g_pCore == this)
            g_pCore = nullptr;
        return;
    }
#endif
    WriteDebugEvent("CCore::~CCore");

    if constexpr (bFreezeWatchdogEnabled)
        StopWatchdogThread();

    // Reset Discord rich presence
    if (m_pDiscordRichPresence)
        m_pDiscordRichPresence.reset();

    m_steamClient.reset();

    // Destroy tray icon
    delete m_pTrayIcon;

    m_pFPSLimiter.reset();

    // This will set the GTA volume to the GTA volume value in the settings,
    // and is not affected by the master volume setting.
    if (auto* menu = m_pLocalGUI ? m_pLocalGUI->GetMainMenu() : nullptr)
    {
        if (auto* settings = menu->GetSettingsWindow())
            settings->ResetGTAVolume();
    }

    // Remove input hook
    CMessageLoopHook::GetSingleton().RemoveHook();

    if (m_bWindowsTimerEnabled)
    {
        KillTimer(GetHookedWindow(), IDT_TIMER1);
        m_bWindowsTimerEnabled = false;
    }

    extern int ms_iDummyProgressTimerCounter;

    if (m_DummyProgressTimerHandle != NULL)
    {
        DeleteTimerQueueTimer(NULL, m_DummyProgressTimerHandle, INVALID_HANDLE_VALUE);
        m_DummyProgressTimerHandle = NULL;
        ms_iDummyProgressTimerCounter = 0;
    }

    // Delete the mod manager
    delete m_pModManager;
    SAFE_DELETE(m_pMessageBox);

    SAFE_DELETE(m_pModelCacheManager);

    // Destroy early subsystems
    m_bModulesLoaded = false;
    DestroyNetwork();
    DestroyMultiplayer();
    DestroyGame();

    // Remove global events
    g_pCore->m_pGUI->ClearInputHandlers(INPUT_CORE);

    // Store core variables to cvars
    CVARS_SET("console_pos", m_pLocalGUI->GetConsole()->GetPosition());
    CVARS_SET("console_size", m_pLocalGUI->GetConsole()->GetSize());

    // Delete interaction objects.
    DestroyFrontendCommands();
    delete m_pConnectManager;
    delete m_pDirect3DData;

    // Delete hooks.
    delete m_pSetCursorPosHook;
    delete m_pDirect3DHookManager;
    delete m_pDirectInputHookManager;

    // Delete the GUI manager
    delete m_pLocalGUI;
    delete m_pGraphics;

    // Delete the web
    DestroyWeb();

    // Delete lazy subsystems
    DestroyGUI();
    DestroyFrontendConfiguration(true);

    // Delete keybinds
    delete m_pKeyBinds;

    // Delete Mouse Control
    delete m_pMouseControl;

    // Delete the logger
    DestroyFrontendLogger();

    // Delete last so calls to GetHookedWindowHandle do not crash
    delete m_pMessageLoopHook;
}

void CCore::InitializeFrontendInfrastructure()
{
    // Keep the real Core configuration and interaction services separate from
    // classic GUI, renderer and hook creation. A future DE frontend can reuse
    // this boundary without claiming that its CGame or multiplayer is ready;
    // current DE startup still rejects before entering it.
    if (m_pXML || m_pConfigFile || m_pKeyBinds || m_XMLModule.IsOk() || m_pLocalization || m_pConsoleLogger || m_pCommands)
        throw std::logic_error("Core frontend infrastructure is already initialized");

    CLocalization* previousLocalization = g_pLocalization;
    try
    {
        // Set our locale to the C locale, except for character handling which is the system's default.
        std::setlocale(LC_ALL, "C");
        std::setlocale(LC_CTYPE, "");
        // Check LC_COLLATE is the old-time raw ASCII sort order.
        assert(strcoll("a", "B") > 0);

        const char* pszNoValOptions[] = {"window", NULL};
        ParseCommandLine(m_CommandLineOptions, m_szCommandLineArgs, pszNoValOptions);

        CreateXML();
        ApplyCoreInitSettings();
        m_pLocalization = new CLocalization;
        g_pLocalization = m_pLocalization;
        m_pConsoleLogger = new CConsoleLogger;
        m_pCommands = new CCommands;
    }
    catch (...)
    {
        // The containing Core destructor does not run when its constructor
        // throws. Roll back only this initialized phase, while its XML owners
        // still exist, without saving through uncreated keybind/menu services.
        DestroyFrontendCommands();
        DestroyFrontendLogger();
        if (g_pLocalization == m_pLocalization)
            g_pLocalization = nullptr;
        SAFE_DELETE(m_pLocalization);
        g_pLocalization = previousLocalization;
        ReleaseXML(false);
        throw;
    }
}

void CCore::DestroyFrontendCommands()
{
    SAFE_DELETE(m_pCommands);
}

void CCore::DestroyFrontendConfiguration(bool saveConfig)
{
    // Normal classic shutdown reaches this after GUI destruction and before
    // keybind destruction. Preserve that order so its existing save path can
    // still use keybinds, updater and server cache; partial startup skips it.
    ReleaseXML(saveConfig);
    if (g_pLocalization == m_pLocalization)
        g_pLocalization = nullptr;
    SAFE_DELETE(m_pLocalization);
}

void CCore::DestroyFrontendLogger()
{
    // Keep logging available to the other frontend owners during teardown.
    SAFE_DELETE(m_pConsoleLogger);
}

eCoreVersion CCore::GetVersion()
{
    return MTACORE_20;
}

CConsoleInterface* CCore::GetConsole()
{
    return m_pLocalGUI->GetConsole();
}

CCommandsInterface* CCore::GetCommands()
{
    return m_pCommands;
}

CGame* CCore::GetGame()
{
    return m_pGame;
}

CGraphicsInterface* CCore::GetGraphics()
{
    return m_pGraphics;
}

CModManagerInterface* CCore::GetModManager()
{
    return m_pModManager;
}

CMultiplayer* CCore::GetMultiplayer()
{
    return m_pMultiplayer;
}

CXMLNode* CCore::GetConfig()
{
    if (!m_pConfigFile)
        return NULL;
    CXMLNode* pRoot = m_pConfigFile->GetRootNode();
    if (!pRoot)
        pRoot = m_pConfigFile->CreateRootNode(CONFIG_ROOT);
    return pRoot;
}

CGUI* CCore::GetGUI()
{
    return m_pGUI;
}

CNet* CCore::GetNetwork()
{
    return m_pNet;
}

CKeyBindsInterface* CCore::GetKeyBinds()
{
    return m_pKeyBinds;
}

CLocalGUI* CCore::GetLocalGUI()
{
    return m_pLocalGUI;
}

void CCore::SaveConfig(bool bWaitUntilFinished)
{
    if (m_pConfigFile)
    {
        CXMLNode* pBindsNode = GetConfig()->FindSubNode(CONFIG_NODE_KEYBINDS);
        if (!pBindsNode)
            pBindsNode = GetConfig()->CreateSubNode(CONFIG_NODE_KEYBINDS);
        m_pKeyBinds->SaveToXML(pBindsNode);
        GetVersionUpdater()->SaveConfigToXML();
        m_pConfigFile->Write();
        GetServerCache()->SaveServerCache(bWaitUntilFinished);
    }
}

void CCore::ChatEcho(const char* szText, bool bColorCoded)
{
    CChat* pChat = m_pLocalGUI->GetChat();
    if (pChat)
    {
        CColor color(255, 255, 255, 255);
        pChat->SetTextColor(color);
    }

    // Echo it to the console and chat
    m_pLocalGUI->EchoChat(szText, bColorCoded);
    if (bColorCoded)
    {
        m_pLocalGUI->EchoConsole(RemoveColorCodes(szText));
    }
    else
        m_pLocalGUI->EchoConsole(szText);
}

void CCore::DebugEcho(const char* szText)
{
    CDebugView* pDebugView = m_pLocalGUI->GetDebugView();
    if (pDebugView)
    {
        CColor color(255, 255, 255, 255);
        pDebugView->SetTextColor(color);
    }

    m_pLocalGUI->EchoDebug(szText);
}

void CCore::DebugPrintf(const char* szFormat, ...)
{
    // Convert it to a string buffer
    char    szBuffer[1024];
    va_list ap;
    va_start(ap, szFormat);
    VSNPRINTF(szBuffer, 1024, szFormat, ap);
    va_end(ap);

    DebugEcho(szBuffer);
}

void CCore::SetDebugVisible(bool bVisible)
{
    if (m_pLocalGUI)
    {
        m_pLocalGUI->SetDebugViewVisible(bVisible);
    }
}

bool CCore::IsDebugVisible()
{
    if (m_pLocalGUI)
        return m_pLocalGUI->IsDebugViewVisible();
    else
        return false;
}

void CCore::DebugEchoColor(const char* szText, unsigned char R, unsigned char G, unsigned char B)
{
    // Set the color
    CDebugView* pDebugView = m_pLocalGUI->GetDebugView();
    if (pDebugView)
    {
        CColor color(R, G, B, 255);
        pDebugView->SetTextColor(color);
    }

    m_pLocalGUI->EchoDebug(szText);
}

void CCore::DebugPrintfColor(const char* szFormat, unsigned char R, unsigned char G, unsigned char B, ...)
{
    // Set the color
    if (szFormat)
    {
        // Convert it to a string buffer
        char    szBuffer[1024];
        va_list ap;
        va_start(ap, B);
        VSNPRINTF(szBuffer, 1024, szFormat, ap);
        va_end(ap);

        // Echo it to the console and chat
        DebugEchoColor(szBuffer, R, G, B);
    }
}

void CCore::DebugClear()
{
    CDebugView* pDebugView = m_pLocalGUI->GetDebugView();
    if (pDebugView)
    {
        pDebugView->Clear();
    }
}

void CCore::ChatEchoColor(const char* szText, unsigned char R, unsigned char G, unsigned char B, bool bColorCoded)
{
    // Set the color
    CChat* pChat = m_pLocalGUI->GetChat();
    if (pChat)
    {
        CColor color(R, G, B, 255);
        pChat->SetTextColor(color);
    }

    // Echo it to the console and chat
    m_pLocalGUI->EchoChat(szText, bColorCoded);
    if (bColorCoded)
    {
        m_pLocalGUI->EchoConsole(RemoveColorCodes(szText));
    }
    else
        m_pLocalGUI->EchoConsole(szText);
}

void CCore::ChatPrintf(const char* szFormat, bool bColorCoded, ...)
{
    // Convert it to a string buffer
    char    szBuffer[1024];
    va_list ap;
    va_start(ap, bColorCoded);
    VSNPRINTF(szBuffer, 1024, szFormat, ap);
    va_end(ap);

    // Echo it to the console and chat
    ChatEcho(szBuffer, bColorCoded);
}

void CCore::ChatPrintfColor(const char* szFormat, bool bColorCoded, unsigned char R, unsigned char G, unsigned char B, ...)
{
    // Set the color
    if (szFormat)
    {
        if (m_pLocalGUI)
        {
            // Convert it to a string buffer
            char    szBuffer[1024];
            va_list ap;
            va_start(ap, B);
            VSNPRINTF(szBuffer, 1024, szFormat, ap);
            va_end(ap);

            // Echo it to the console and chat
            ChatEchoColor(szBuffer, R, G, B, bColorCoded);
        }
    }
}

void CCore::SetChatVisible(bool bVisible, bool bInputBlocked)
{
    if (m_pLocalGUI)
    {
        m_pLocalGUI->SetChatBoxVisible(bVisible, bInputBlocked);
    }
}

bool CCore::IsChatVisible()
{
    if (m_pLocalGUI)
    {
        return m_pLocalGUI->IsChatBoxVisible();
    }
    return false;
}

bool CCore::IsChatInputBlocked()
{
    if (m_pLocalGUI)
    {
        return m_pLocalGUI->IsChatBoxInputBlocked();
    }
    return false;
}

bool CCore::ClearChat()
{
    if (m_pLocalGUI)
    {
        CChat* pChat = m_pLocalGUI->GetChat();
        if (pChat)
        {
            pChat->Clear();
            return true;
        }
    }
    return false;
}

void CCore::InitiateScreenShot(bool bIsCameraShot)
{
    CScreenShot::InitiateScreenShot(bIsCameraShot);
}

void CCore::EnableChatInput(char* szCommand, DWORD dwColor)
{
    if (m_pLocalGUI)
    {
        if (m_pGame->GetSystemState() == SystemState::GS_PLAYING_GAME && m_pModManager->IsLoaded() && !IsOfflineMod() && !m_pGame->IsAtMenu() &&
            !m_pLocalGUI->GetMainMenu()->IsVisible() && !m_pLocalGUI->GetConsole()->IsVisible() && !m_pLocalGUI->IsChatBoxInputEnabled())
        {
            CChat* pChat = m_pLocalGUI->GetChat();
            pChat->SetCommand(szCommand);
            m_pLocalGUI->SetChatBoxInputEnabled(true);
        }
    }
}

bool CCore::IsChatInputEnabled()
{
    if (m_pLocalGUI)
    {
        return (m_pLocalGUI->IsChatBoxInputEnabled());
    }

    return false;
}

bool CCore::SetChatboxCharacterLimit(int charLimit)
{
    CChat* pChat = m_pLocalGUI->GetChat();

    if (!pChat)
        return false;

    pChat->SetCharacterLimit(charLimit);
    return true;
}

void CCore::ResetChatboxCharacterLimit()
{
    CChat* pChat = m_pLocalGUI->GetChat();

    if (!pChat)
        return;

    pChat->SetCharacterLimit(pChat->GetDefaultCharacterLimit());
}

int CCore::GetChatboxCharacterLimit()
{
    CChat* pChat = m_pLocalGUI->GetChat();

    if (!pChat)
        return 0;

    return pChat->GetCharacterLimit();
}

int CCore::GetChatboxMaxCharacterLimit()
{
    CChat* pChat = m_pLocalGUI->GetChat();

    if (!pChat)
        return 0;

    return pChat->GetMaxCharacterLimit();
}

bool CCore::IsSettingsVisible()
{
    if (auto* menu = m_pLocalGUI ? m_pLocalGUI->GetMainMenu() : nullptr)
    {
        // Frontend visibility queries must not construct game settings before
        // their engine and renderer have been admitted.
        if (auto* settings = menu->GetSettingsWindow())
            return settings->IsVisible();
    }

    return false;
}

bool CCore::IsMenuVisible()
{
    if (m_pLocalGUI)
    {
        return (m_pLocalGUI->GetMainMenu()->IsVisible());
    }

    return false;
}

bool CCore::IsCursorForcedVisible()
{
    if (m_pLocalGUI)
    {
        return (m_pLocalGUI->IsCursorForcedVisible());
    }

    return false;
}

void CCore::ApplyConsoleSettings()
{
    CConsole* pConsole = m_pLocalGUI->GetConsole();
    if (!pConsole) [[unlikely]]
        return;

    CVector2D vec;
    CVARS_GET("console_pos", vec);
    pConsole->SetPosition(vec);
    CVARS_GET("console_size", vec);
    pConsole->SetSize(vec);
}

void CCore::ApplyGameSettings()
{
    bool                      bVal;
    int                       iVal;
    float                     fVal;
    CControllerConfigManager* pController = m_pGame->GetControllerConfigManager();
    CGameSettings*            pGameSettings = m_pGame->GetSettings();

    CVARS_GET("invert_mouse", bVal);
    pController->SetMouseInverted(bVal);
    CVARS_GET("fly_with_mouse", bVal);
    pController->SetFlyWithMouse(bVal);
    CVARS_GET("steer_with_mouse", bVal);
    pController->SetSteerWithMouse(bVal);
    CVARS_GET("classic_controls", bVal);
    pController->SetClassicControls(bVal);
    CVARS_GET("volumetric_shadows", bVal);
    pGameSettings->SetVolumetricShadowsEnabled(bVal);
    CVARS_GET("aspect_ratio", iVal);
    pGameSettings->SetAspectRatio((eAspectRatio)iVal, CVARS_GET_VALUE<bool>("hud_match_aspect_ratio"));
    CVARS_GET("grass", bVal);
    pGameSettings->SetGrassEnabled(bVal);
    CVARS_GET("heat_haze", bVal);
    m_pMultiplayer->SetHeatHazeEnabled(bVal);
    CVARS_GET("fast_clothes_loading", iVal);
    m_pMultiplayer->SetFastClothesLoading((CMultiplayer::EFastClothesLoading)iVal);
    CVARS_GET("tyre_smoke_enabled", bVal);
    m_pMultiplayer->SetTyreSmokeEnabled(bVal);
    pGameSettings->UpdateFieldOfViewFromSettings();
    pGameSettings->ResetBlurEnabled();
    pGameSettings->ResetVehiclesLODDistance();
    pGameSettings->ResetPedsLODDistance();
    pGameSettings->ResetCoronaReflectionsEnabled();
    CVARS_GET("dynamic_ped_shadows", bVal);
    pGameSettings->SetDynamicPedShadowsEnabled(bVal);
    pController->SetVerticalAimSensitivityRawValue(CVARS_GET_VALUE<float>("vertical_aim_sensitivity"));
    pController->SetVerticalAimSensitivitySameAsHorizontal(CVARS_GET_VALUE<bool>("use_mouse_sensitivity_for_aiming"));
    CVARS_GET("mastervolume", fVal);
    pGameSettings->SetRadioVolume(CVARS_GET_VALUE<float>("radiovolume") * fVal * 64.0f);
    pGameSettings->SetSFXVolume(CVARS_GET_VALUE<float>("sfxvolume") * fVal * 64.0f);
}

void CCore::SetConnected(bool bConnected)
{
    m_pLocalGUI->GetMainMenu()->SetIsIngame(bConnected);
    UpdateIsWindowMinimized();  // Force update of stuff

    if (g_pCore->GetCVars()->GetValue("allow_discord_rpc", false))
    {
        const auto discord = g_pCore->GetDiscord();
        if (!discord->IsDiscordRPCEnabled())
            discord->SetDiscordRPCEnabled(true);

        discord->SetPresenceState(bConnected ? _("In-game") : _("Main menu"), false);
        discord->SetPresenceStartTimestamp(0);
        discord->SetPresenceDetails("", false);

        if (bConnected)
            discord->SetPresenceStartTimestamp(time(nullptr));
    }
}

bool CCore::IsConnected()
{
    return m_pLocalGUI->GetMainMenu() && m_pLocalGUI->GetMainMenu()->GetIsIngame();
}

bool CCore::Reconnect(const char* szHost, unsigned short usPort, const char* szPassword, bool bSave)
{
    return m_pConnectManager->Reconnect(szHost, usPort, szPassword, bSave);
}

void CCore::SetOfflineMod(bool bOffline)
{
    m_bIsOfflineMod = bOffline;
}

const char* CCore::GetModInstallRoot(const char* szModName)
{
    m_strModInstallRoot = CalcMTASAPath(PathJoin("mods", szModName));
    return m_strModInstallRoot;
}

void CCore::ForceCursorVisible(bool bVisible, bool bToggleControls)
{
    m_bCursorToggleControls = bToggleControls;
    m_pLocalGUI->ForceCursorVisible(bVisible);
}

void CCore::SetMessageProcessor(pfnProcessMessage pfnMessageProcessor)
{
    m_pfnMessageProcessor = pfnMessageProcessor;
}

void CCore::ShowMessageBox(const char* szTitle, const char* szText, unsigned int uiFlags, GUI_CALLBACK* ResponseHandler)
{
    RemoveMessageBox();

    // Create the message box
    m_pMessageBox = m_pGUI->CreateMessageBox(szTitle, szText, uiFlags);
    if (ResponseHandler)
        m_pMessageBox->SetClickHandler(*ResponseHandler);

    // Make sure it doesn't auto-destroy, or we'll crash if the msgbox had buttons and the user clicks OK
    m_pMessageBox->SetAutoDestroy(false);
}

void CCore::RemoveMessageBox(bool bNextFrame)
{
    if (bNextFrame)
    {
        m_bDestroyMessageBox = true;
    }
    else
    {
        if (m_pMessageBox)
        {
            delete m_pMessageBox;
            m_pMessageBox = NULL;
        }
    }
}

//
// Show message box with possibility of on-line help
//
void CCore::ShowErrorMessageBox(const SString& strTitle, SString strMessage, const SString& strTroubleLink)
{
    if (strTroubleLink.empty())
    {
        CCore::GetSingleton().ShowMessageBox(strTitle, strMessage, MB_BUTTON_OK | MB_ICON_ERROR);
    }
    else
    {
        CQuestionBox* pQuestionBox = CCore::GetSingleton().GetLocalGUI()->GetMainMenu()->GetQuestionWindow();
        pQuestionBox->Reset();
        pQuestionBox->SetTitle(strTitle);
        pQuestionBox->SetMessage(strMessage);
        pQuestionBox->SetOnLineHelpOption(strTroubleLink);
        pQuestionBox->Show();
    }
}

//
// Show message box with possibility of on-line help
//  + with net error code appended to message and trouble link
//
void CCore::ShowNetErrorMessageBox(const SString& strTitle, SString strMessage, SString strTroubleLink, bool bLinkRequiresErrorCode)
{
    uint uiErrorCode = CCore::GetSingleton().GetNetwork()->GetExtendedErrorCode();
    if (uiErrorCode != 0)
    {
        // Do anti-virus check soon
        SetApplicationSettingInt("noav-user-says-skip", 1);
        strMessage += SString(" \nCode: %08X", uiErrorCode);
        if (!strTroubleLink.empty())
            strTroubleLink += SString("&neterrorcode=%08X", uiErrorCode);
    }
    else if (bLinkRequiresErrorCode)
        strTroubleLink = "";  // No link if no error code

    AddReportLog(7100, SString("Core - NetError (%s) (%s)", *strTitle, *strMessage));
    ShowErrorMessageBox(strTitle, strMessage, strTroubleLink);
}

//
// Callback used in CCore::ShowErrorMessageBox
//
void CCore::ErrorMessageBoxCallBack(void* pData, uint uiButton)
{
    CCore::GetSingleton().GetLocalGUI()->GetMainMenu()->GetQuestionWindow()->Reset();

    SString* pstrTroubleLink = (SString*)pData;
    if (uiButton == 1)
    {
        uint uiErrorCode = (uint)pData;
        BrowseToSolution(*pstrTroubleLink, EXIT_GAME_FIRST);
    }
    delete pstrTroubleLink;
}

//
// Check for disk space problems
// Returns false if low disk space, and dialog is being shown
//
bool CCore::CheckDiskSpace(uint uiResourcesPathMinMB, uint uiDataPathMinMB)
{
    SString strDriveWithNoSpace = GetDriveNameWithNotEnoughSpace(uiResourcesPathMinMB, uiDataPathMinMB);
    if (!strDriveWithNoSpace.empty())
    {
        SString strMessage(_("MTA:SA cannot continue because drive %s does not have enough space."), *strDriveWithNoSpace);
        SString strTroubleLink(SString("low-disk-space&drive=%s", *strDriveWithNoSpace.Left(1)));
        g_pCore->ShowErrorMessageBox(_("Fatal error") + _E("CC43"), strMessage, strTroubleLink);
        return false;
    }
    return true;
}

HWND CCore::GetHookedWindow()
{
    return CMessageLoopHook::GetSingleton().GetHookedWindowHandle();
}

void CCore::HideMainMenu()
{
    m_pLocalGUI->GetMainMenu()->SetVisible(false);
}

void CCore::ShowServerInfo(unsigned int WindowType)
{
    RemoveMessageBox();
    CServerInfo::GetSingletonPtr()->Show((eWindowType)WindowType);
}

void CCore::ApplyHooks()
{
    WriteDebugEvent("CCore::ApplyHooks");

    // Create our hooks.
    m_pDirectInputHookManager->ApplyHook();
    // m_pDirect3DHookManager->ApplyHook ( );
    m_pSetCursorPosHook->ApplyHook();

    // Remove useless DirectPlay dependency (dpnhpast.dll) @ 0x745701
    // We have to patch here as multiplayer_sa and game_sa are loaded too late
    DetourLibraryFunction("kernel32.dll", "LoadLibraryA", Win32LoadLibraryA, SkipDirectPlay_LoadLibraryA);
}

bool UsingAltD3DSetup()
{
    static bool bAltStartup = GetApplicationSettingInt("nvhacks", "optimus-alt-startup") ? true : false;
    return bAltStartup;
}

void CCore::ApplyHooks2()
{
    WriteDebugEvent("CCore::ApplyHooks2");
    // Try this one a little later
    if (!UsingAltD3DSetup())
        m_pDirect3DHookManager->ApplyHook();
    else
    {
        // Done a little later to get past the loading time required to decrypt the gta
        // executable into memory...
        CCore::GetSingleton().CreateStartupModules();
    }
}

void CCore::ApplyHooks3(bool bEnable)
{
    if (bEnable)
        CDirect3DHook9::GetSingletonPtr()->ApplyHook();
    else
        CDirect3DHook9::GetSingletonPtr()->RemoveHook();
}

void CCore::SetCenterCursor(bool bEnabled)
{
    if (bEnabled)
        m_pSetCursorPosHook->EnableSetCursorPos();
    else
        m_pSetCursorPosHook->DisableSetCursorPos();
}

////////////////////////////////////////////////////////////////////////
//
// LoadModule
//
// Attempt to load a module. Returns if successful.
// On failure, displays message box and terminates the current process.
//
////////////////////////////////////////////////////////////////////////
void LoadModule(CModuleLoader& m_Loader, const SString& strName, const SString& strModuleName)
{
    WriteDebugEvent("Loading " + strName.ToLower());

    // Ensure DllDirectory has not been changed
    SString strDllDirectory = GetSystemDllDirectory();
    if (CalcMTASAPath("mta").CompareI(strDllDirectory) == false)
    {
        AddReportLog(3119, SString("DllDirectory wrong:  DllDirectory:'%s'  Path:'%s'", *strDllDirectory, *CalcMTASAPath("mta")));
        SetDllDirectory(CalcMTASAPath("mta"));
    }

    // Save current directory (shouldn't change anyway)
    SString strSavedCwd = GetSystemCurrentDirectory();

    // Load appropriate compilation-specific library.
#ifdef MTA_DEBUG
    SString strModuleFileName = strModuleName + "_d.dll";
#else
    SString strModuleFileName = strModuleName + ".dll";
#endif
    // A refused replacement may leave an earlier module safely owned. Its
    // IsOk state cannot establish that the requested startup module loaded.
    if (!m_Loader.LoadModule(CalcMTASAPath(PathJoin("mta", strModuleFileName))))
    {
        SString strMessage("Error loading '%s' module!\n%s", *strName, *m_Loader.GetLastErrorMessage());
        SString strType = "module-not-loadable&name=" + strModuleName;

        // Extra message if d3d9.dll exists
        SString strD3dModuleFilename = PathJoin(GetLaunchPath(), "d3d9.dll");
        if (FileExists(strD3dModuleFilename))
        {
            strMessage += "\n\n";
            strMessage += _("TO FIX, REMOVE THIS FILE:") + "\n";
            strMessage += strD3dModuleFilename;
            strType += "&d3d9=1";
        }
        SetCurrentDirectory(strSavedCwd);
        BrowseToSolution(strType, ASK_GO_ONLINE | EXIT_GAME_FIRST, strMessage);
        throw std::runtime_error("Required startup module could not be loaded");
    }
    // Restore current directory
    SetCurrentDirectory(strSavedCwd);

    WriteDebugEvent(strName + " loaded.");
}

////////////////////////////////////////////////////////////////////////
//
// InitModule
//
// Attempt to initialize a loaded module. Returns if successful.
// On failure, displays message box and terminates the current process.
//
////////////////////////////////////////////////////////////////////////
template <class T, class U>
T* InitModule(CModuleLoader& m_Loader, const SString& strName, const SString& strInitializer, U* pObj)
{
    // Save current directory (shouldn't change anyway)
    SString strSavedCwd = GetSystemCurrentDirectory();

    // Get initializer function from DLL.
    typedef T* (*PFNINITIALIZER)(U*);
    PFNINITIALIZER pfnInit = static_cast<PFNINITIALIZER>(m_Loader.GetFunctionPointer(strInitializer));

    if (pfnInit == NULL)
    {
        MessageBoxUTF8(0, SString(_("%s module is incorrect!"), *strName), "Error" + _E("CC40"), MB_OK | MB_ICONERROR | MB_TOPMOST);
        TerminateProcess(GetCurrentProcess(), 1);
    }

    // If we have a valid initializer, call it.
    T* pResult = pfnInit(pObj);

    // Restore current directory
    SetCurrentDirectory(strSavedCwd);

    WriteDebugEvent(strName + " initialized.");
    return pResult;
}

////////////////////////////////////////////////////////////////////////
//
// CreateModule
//
// Attempt to load and initialize a module. Returns if successful.
// On failure, displays message box and terminates the current process.
//
////////////////////////////////////////////////////////////////////////
template <class T, class U>
T* CreateModule(CModuleLoader& m_Loader, const SString& strName, const SString& strModuleName, const SString& strInitializer, U* pObj)
{
    LoadModule(m_Loader, strName, strModuleName);
    return InitModule<T>(m_Loader, strName, strInitializer, pObj);
}

void CCore::CreateStartupModules()
{
#ifdef MTA_WITH_DEFINITIVE_ADAPTER
    // Admission precedes both the reentrancy flag and any classic module load.
    // Direct callers cannot turn an incomplete DE backend into game_sa or a
    // D3D9 GUI by bypassing the constructor's separate startup gates.
    if (!m_DefinitiveAdapter.IsGameplayReady())
        throw std::runtime_error("DE adapter gameplay unavailable: " + m_DefinitiveAdapter.GameplayUnavailableReason());
    throw std::runtime_error("DE core, multiplayer and rendering startup are not implemented");
#else
    // Core owns module startup policy independently of the renderer callback,
    // so a DE renderer can eventually use the same boundary. Mark entry before
    // creation to preserve the existing reentrancy and failure behavior.
    if (AreModulesLoaded())
        return;
    SetModulesLoaded(true);
    CreateNetwork();
    CreateGame();
    CreateMultiplayer();
    CreateXML();
    CreateGUI();
#endif
}

void CCore::CreateGame()
{
#ifdef MTA_WITH_DEFINITIVE_ADAPTER
    CGame* provided = nullptr;
    if (!m_DefinitiveAdapter.BorrowGameInterface(provided))
        throw std::runtime_error("DE adapter game interface unavailable: " + m_DefinitiveAdapter.GetError());
    m_pGame = provided;
    m_GameInterfaceOwnership = GameInterfaceOwnership::AdapterBorrowed;
#else
    m_pGame = CreateModule<CGame>(m_GameModule, "Game", "game_sa", "GetGameInterface", this);
    m_GameInterfaceOwnership = GameInterfaceOwnership::ClassicModule;
    if (m_pGame->GetGameVersion() >= VERSION_11)
    {
        BrowseToSolution("downgrade", TERMINATE_PROCESS,
                         "Only GTA:SA version 1.0 is supported!\n\nYou are now being redirected to a page where you can patch your version.");
    }
#endif
}

void CCore::CreateMultiplayer()
{
    m_pMultiplayer = CreateModule<CMultiplayer>(m_MultiplayerModule, "Multiplayer", "multiplayer_sa", "InitMultiplayerInterface", this);
    if (m_pMultiplayer)
        m_pMultiplayer->SetIdleHandler(CCore::StaticIdleHandler);
}

void CCore::DeinitGUI()
{
}

void CCore::InitGUI(IDirect3DDevice9* pDevice)
{
    m_pGUI = InitModule<CGUI>(m_GUIModule, "GUI", "InitGUIInterface", pDevice);

    // Apply CPU affinity here (GTA allocates threads on startup, so we have to do it here instead of earlier)
    bool affinity = CVARS_GET_VALUE<bool>("process_cpu_affinity");
    if (!affinity)
        return;

    DWORD_PTR mask;
    DWORD_PTR sys;
    HANDLE    process = GetCurrentProcess();
    BOOL      result = GetProcessAffinityMask(process, &mask, &sys);

    if (result)
        SetProcessAffinityMask(process, mask & ~1);
}

void CCore::CreateGUI()
{
    LoadModule(m_GUIModule, "GUI", "cgui");
}

void CCore::DestroyGUI()
{
    WriteDebugEvent("CCore::DestroyGUI");
    if (m_pGUI)
    {
        m_pGUI = NULL;
    }
    m_GUIModule.UnloadModule();
}

void CCore::CreateNetwork()
{
    if (m_pNet)
        return;
    LoadModule(m_NetModule, "Network", "netc");
    const auto check = reinterpret_cast<NetworkModuleStartup::CheckCompatibility>(m_NetModule.GetFunctionPointer("CheckCompatibility"));
    const auto admitted = NetworkModuleStartup::Run(check, MTA_DM_CLIENT_NET_MODULE_VERSION,
                                                    [&] { m_pNet = InitModule<CNet>(m_NetModule, "Network", "InitNetInterface", this); });
    if (admitted.failure != NetworkModuleStartup::Failure::None)
    {
        SString strMessage = "Network module is missing CheckCompatibility!";
        if (admitted.failure == NetworkModuleStartup::Failure::VersionMismatch)
            strMessage.Format("Network module not compatible! (Expected 0x%x, got 0x%x)", MTA_DM_CLIENT_NET_MODULE_VERSION, admitted.moduleVersion);
#if !defined(MTA_DM_PUBLIC_CONNECTIONS)
        strMessage += "\n\n(Devs: Update source and run win-install-data.bat)";
#endif
        BrowseToSolution("netc-not-compatible", ASK_GO_ONLINE | TERMINATE_PROCESS, strMessage);
        return;
    }
    if (!m_pNet)
        throw std::runtime_error("Network module initializer returned no interface");

    // Set mta version for report log here
    SetApplicationSetting("mta-version-ext", SString("%d.%d.%d-%d.%05d.%d.%03d", MTASA_VERSION_MAJOR, MTASA_VERSION_MINOR, MTASA_VERSION_MAINTENANCE,
                                                     MTASA_VERSION_TYPE, MTASA_VERSION_BUILD, m_pNet->GetNetRev(), m_pNet->GetNetRel()));
    char szSerial[64];
    m_pNet->GetSerial(szSerial, sizeof(szSerial));
    SetApplicationSetting("serial", szSerial);
}

void CCore::CreateXML()
{
    if (!m_pXML)
        m_pXML = CreateModule<CXML>(m_XMLModule, "XML", "xmll", "InitXMLInterface", *CalcMTASAPath("MTA"));

    if (!m_pConfigFile)
    {
        // Load config XML file
        m_pConfigFile = m_pXML->CreateXML(CalcMTASAPath(MTA_CONFIG_PATH));
        if (!m_pConfigFile)
        {
            assert(false);

            if (m_pXML)
            {
                using PFNReleaseXMLInterface = void (*)();
                if (auto pfnRelease = reinterpret_cast<PFNReleaseXMLInterface>(m_XMLModule.GetFunctionPointer("ReleaseXMLInterface")))
                    pfnRelease();
            }

            m_pXML = NULL;
            m_XMLModule.UnloadModule();
            return;
        }

        m_pConfigFile->Parse();
    }

    // Load the keybinds (loads defaults if the subnode doesn't exist)
    if (m_pKeyBinds)
    {
        m_pKeyBinds->LoadFromXML(GetConfig()->FindSubNode(CONFIG_NODE_KEYBINDS));
        m_pKeyBinds->LoadDefaultCommands(false);
    }

    // Load XML-dependant subsystems
    m_ClientVariables.Load();
}

void CCore::DestroyGame()
{
    WriteDebugEvent("CCore::DestroyGame");

    if (m_GameInterfaceOwnership == GameInterfaceOwnership::AdapterBorrowed)
    {
        // Relinquish the borrowed view before its owner tears down. Never call
        // the classic self-deleting Terminate or unload the legacy game module.
        m_pGame = nullptr;
        m_GameInterfaceOwnership = GameInterfaceOwnership::None;
#ifdef MTA_WITH_DEFINITIVE_ADAPTER
        m_DefinitiveAdapter.Stop();
#endif
        return;
    }
    if (m_pGame)
    {
        m_pGame->Terminate();
        m_pGame = NULL;
    }
    m_GameInterfaceOwnership = GameInterfaceOwnership::None;

    m_GameModule.UnloadModule();
}

void CCore::DestroyMultiplayer()
{
    WriteDebugEvent("CCore::DestroyMultiplayer");

    if (m_pMultiplayer)
    {
        m_pMultiplayer = NULL;
    }

    m_MultiplayerModule.UnloadModule();
}

void CCore::DestroyXML()
{
    ReleaseXML(true);
}

void CCore::ReleaseXML(bool saveConfig)
{
    WriteDebugEvent("CCore::DestroyXML");

    // Save and unload configuration
    if (m_pConfigFile)
    {
        if (saveConfig)
            SaveConfig(true);
        delete m_pConfigFile;
        m_pConfigFile = nullptr;
    }

    if (m_pXML)
    {
        using PFNReleaseXMLInterface = void (*)();
        if (auto pfnRelease = reinterpret_cast<PFNReleaseXMLInterface>(m_XMLModule.GetFunctionPointer("ReleaseXMLInterface")))
            pfnRelease();
        m_pXML = NULL;
    }

    m_XMLModule.UnloadModule();
}

void CCore::DestroyNetwork()
{
    WriteDebugEvent("CCore::DestroyNetwork");

    if (m_pNet)
    {
        m_pNet->Shutdown();
        m_pNet = NULL;
    }

    // Skip unload as it can cause exit crashes due to threading issues
    // m_NetModule.UnloadModule();
}

CWebCoreInterface* CCore::GetWebCore()
{
    if (m_pWebCore == nullptr)
    {
        bool gpuEnabled;
        auto cvars = g_pCore->GetCVars();
        cvars->Get("browser_enable_gpu", gpuEnabled);

        m_pWebCore = CreateModule<CWebCoreInterface>(m_WebCoreModule, "CefWeb", "cefweb", "InitWebCoreInterface", this);
        if (!m_pWebCore)
        {
            WriteDebugEvent("CCore::GetWebCore - CreateModule failed");
            return nullptr;
        }

        // Log current working directory
        wchar_t cwdBeforeWebInit[32768]{};
        DWORD   cwdBeforeWebInitLen = GetCurrentDirectoryW(32768, cwdBeforeWebInit);
        if (cwdBeforeWebInitLen > 0)
        {
            WriteDebugEvent(SString("CCore::GetWebCore - CWD before Initialise: %S", cwdBeforeWebInit));
        }

        // Keep m_pWebCore alive even if Initialise() fails
        // CefInitialize() can only be called once per process
        // Deleting and recreating m_pWebCore causes repeated initialization attempts
        // Track initialization state via IsInitialised() instead
        bool bInitSuccess = false;
        try
        {
            bInitSuccess = m_pWebCore->Initialise(gpuEnabled);
        }
        catch (...)
        {
            WriteDebugEvent("CCore::GetWebCore - Initialise threw exception");
            bInitSuccess = false;
        }

        if (!bInitSuccess)
        {
            WriteDebugEvent("CCore::GetWebCore - Initialise failed");
            return nullptr;
        }
    }
    else
    {
        // On subsequent calls, check if initialization succeeded
        if (!m_pWebCore->IsInitialised())
            return nullptr;
    }

    return m_pWebCore;
}

void CCore::DestroyWeb()
{
    WriteDebugEvent("CCore::DestroyWeb");
    SAFE_DELETE(m_pWebCore);
    m_WebCoreModule.UnloadModule();
}

void CCore::UpdateIsWindowMinimized()
{
    m_bIsWindowMinimized = IsIconic(GetHookedWindow()) ? true : false;
    // Update CPU saver for when minimized and not connected
    g_pCore->GetMultiplayer()->SetIsMinimizedAndNotConnected(m_bIsWindowMinimized && !IsConnected());
    g_pCore->GetMultiplayer()->SetMirrorsEnabled(!m_bIsWindowMinimized);

    // Enable timer if not connected at least once
    bool bEnableTimer = !m_bGettingIdleCallsFromMultiplayer;
    if (m_bWindowsTimerEnabled != bEnableTimer)
    {
        m_bWindowsTimerEnabled = bEnableTimer;
        if (bEnableTimer)
            SetTimer(GetHookedWindow(), IDT_TIMER1, 50, (TIMERPROC)NULL);
        else
            KillTimer(GetHookedWindow(), IDT_TIMER1);
    }
}

bool CCore::IsWindowMinimized()
{
    return m_bIsWindowMinimized;
}

void CCore::DoPreFramePulse()
{
    TIMING_CHECKPOINT("+CorePreFrame");

    if constexpr (bFreezeWatchdogEnabled)
        UpdateWatchdogHeartbeat();

    m_pKeyBinds->DoPreFramePulse();

    // Notify the mod manager
    m_pModManager->DoPulsePreFrame();

    m_pLocalGUI->DoPulse();

    CCrashDumpWriter::UpdateCounters();

    TIMING_CHECKPOINT("-CorePreFrame");
}

void CCore::DoPostFramePulse()
{
    TIMING_CHECKPOINT("+CorePostFrame1");
    if (m_bQuitOnPulse)
        Quit();

    if (m_bDestroyMessageBox)
    {
        RemoveMessageBox();
        m_bDestroyMessageBox = false;
    }

    static bool bFirstPulse = true;
    if (bFirstPulse)
    {
        bFirstPulse = false;

        // Validate CVARS
        CClientVariables::GetSingleton().ValidateValues();

        // Apply all settings
        ApplyConsoleSettings();
        ApplyGameSettings();

        // Allow connecting with the local Steam client
        bool allowSteamClient = false;
        CVARS_GET("allow_steam_client", allowSteamClient);
        if (allowSteamClient)
            m_steamClient->Connect();

        m_pGUI->SelectInputHandlers(INPUT_CORE);
    }

    // This is the first frame in the menu?
    if (m_pGame->GetSystemState() == SystemState::GS_FRONTEND)
    {
        if (m_menuFrame < 255)
            ++m_menuFrame;

        if (m_menuFrame == 1)
        {
            WatchDogCompletedSection("L2");  // gta_sa.set seems ok
            WatchDogCompletedSection("L3");  // No hang on startup

            // Start watchdog thread now that initial loading is complete
            if constexpr (bFreezeWatchdogEnabled)
            {
                if (!StartWatchdogThread(GetCurrentThreadId(), uiFreezeWatchdogTimeoutSeconds))
                {
                    WriteDebugEvent("CCore: WARNING - Failed to start watchdog thread");
                }
            }

            // Disable vsync while it's all dark
            m_pGame->DisableVSync();
        }

        if (!m_bCrashDumpEncryptionDone && m_menuFrame >= 5 && m_pNet && m_pNet->IsReady())
        {
            m_bCrashDumpEncryptionDone = true;
            HandleCrashDumpEncryption();
        }

        if (m_menuFrame >= 5 && !m_isNetworkReady && m_pNet->IsReady())
        {
            m_isNetworkReady = true;

            // Parse the command line
            // Does it begin with mtasa://?
            if (m_szCommandLineArgs && strnicmp(m_szCommandLineArgs, "mtasa://", 8) == 0)
            {
                SString strArguments = GetConnectCommandFromURI(m_szCommandLineArgs);
                // Run the connect command
                if (strArguments.length() > 0 && !m_pCommands->Execute(strArguments))
                {
                    ShowMessageBox(_("Error") + _E("CC41"), _("Error executing URL"), MB_BUTTON_OK | MB_ICON_ERROR);
                }
            }
            else
            {
                // We want to load a mod?
                const char* szOptionValue;
                if (szOptionValue = GetCommandLineOption("c"))
                {
                    CCommandFuncs::Connect(szOptionValue);
                }
            }
        }

        if (m_menuFrame >= 75 && m_requestNewNickname && GetLocalGUI()->GetMainMenu()->IsVisible() && !GetLocalGUI()->GetMainMenu()->IsFading() &&
            !GetLocalGUI()->GetMainMenu()->GetQuestionWindow()->IsVisible())
        {
            // Request a new nickname if we're waiting for one
            if (auto* settings = GetLocalGUI()->GetMainMenu()->GetSettingsWindow())
            {
                settings->RequestNewNickname();
                m_requestNewNickname = false;
            }
            // Preserve the request while a frontend defers game settings.
        }
    }

    if (!IsFocused() && m_bLastFocused)
    {
        // Fix for #4948
        // A fatal fault dialog takes focus while pumping messages; releasing the
        // control binds runs Lua handlers that touch the half-destroyed GUI.
        // Skipping the release is safe because both fault paths terminate the
        // process when the dialog closes.
        if (!CLocalGUI::IsFaultDialogOpen())
            m_pKeyBinds->CallAllGTAControlBinds(CONTROL_BOTH, false);
        m_bLastFocused = false;
    }
    else if (IsFocused() && !m_bLastFocused)
    {
        m_bLastFocused = true;
    }

    GetJoystickManager()->DoPulse();  // Note: This may indirectly call CMessageLoopHook::ProcessMessage
    m_pKeyBinds->DoPostFramePulse();

    if (m_pWebCore)
        m_pWebCore->DoPulse();

    // Notify the mod manager and the connect manager
    TIMING_CHECKPOINT("-CorePostFrame1");
    m_pModManager->DoPulsePostFrame();
    TIMING_CHECKPOINT("+CorePostFrame2");
    GetMemStats()->Draw();
    GetGraphStats()->Draw();
    m_pConnectManager->DoPulse();

    // Update Discord Rich Presence status
    if (const long long ticks = GetTickCount64_(); ticks > m_timeDiscordAppLastUpdate + TIME_DISCORD_UPDATE_RICH_PRESENCE_RATE)
    {
        if (const auto discord = g_pCore->GetDiscord(); discord && discord->IsDiscordRPCEnabled())
        {
            discord->UpdatePresence();
            m_timeDiscordAppLastUpdate = ticks;
#ifdef DISCORD_DISABLE_IO_THREAD
            // Update manually if we're not using the IO thread
            discord->UpdatePresenceConnection();
#endif
        }
    }

    TIMING_CHECKPOINT("-CorePostFrame2");
}

// Called after MOD is unloaded
void CCore::OnModUnload()
{
    // reattach the global event
    m_pGUI->SelectInputHandlers(INPUT_CORE);
    // remove unused events
    m_pGUI->ClearInputHandlers(INPUT_MOD);

    // Ensure all these have been removed
    m_pKeyBinds->RemoveAllFunctions();
    m_pKeyBinds->RemoveAllControlFunctions();

    // Reset client script frame rate limit
    GetFPSLimiter()->SetClientEnforcedFPS(FPSLimits::FPS_UNLIMITED);

    // Clear web whitelist
    if (m_pWebCore)
        m_pWebCore->ResetFilter();

    // Destroy tray icon
    m_pTrayIcon->DestroyTrayIcon();

    // Reset chatbox character limit
    ResetChatboxCharacterLimit();
}

void CCore::RegisterCommands()
{
    // m_pCommands->Add ( "e", CCommandFuncs::Editor );
    // m_pCommands->Add ( "clear", CCommandFuncs::Clear );
    m_pCommands->Add("help", _("this help screen"), CCommandFuncs::Help);
    m_pCommands->Add("exit", _("exits the application"), CCommandFuncs::Exit);
    m_pCommands->Add("quit", _("exits the application"), CCommandFuncs::Exit);
    m_pCommands->Add("ver", _("shows the version"), CCommandFuncs::Ver);
    m_pCommands->Add("time", _("shows the time"), CCommandFuncs::Time);
    m_pCommands->Add("showhud", _("shows the hud"), CCommandFuncs::HUD);
    m_pCommands->Add("binds", _("shows all the binds"), CCommandFuncs::Binds);
    m_pCommands->Add("serial", _("shows your serial"), CCommandFuncs::Serial);

#if 0
    m_pCommands->Add ( "vid",               "changes the video settings (id)",  CCommandFuncs::Vid );
    m_pCommands->Add ( "window",            "enter/leave windowed mode",        CCommandFuncs::Window );
    m_pCommands->Add ( "load",              "loads a mod (name args)",          CCommandFuncs::Load );
    m_pCommands->Add ( "unload",            "unloads a mod (name)",             CCommandFuncs::Unload );
#endif

    m_pCommands->Add("connect", _("connects to a server (host port nick pass)"), CCommandFuncs::Connect);
    m_pCommands->Add("reconnect", _("connects to a previous server"), CCommandFuncs::Reconnect);
    m_pCommands->Add("bind", _("binds a key (key control)"), CCommandFuncs::Bind);
    m_pCommands->Add("unbind", _("unbinds a key (key)"), CCommandFuncs::Unbind);
    m_pCommands->Add("copygtacontrols", _("copies the default gta controls"), CCommandFuncs::CopyGTAControls);
    m_pCommands->Add("screenshot", _("outputs a screenshot"), CCommandFuncs::ScreenShot);
    m_pCommands->Add("saveconfig", _("immediately saves the config"), CCommandFuncs::SaveConfig);

    m_pCommands->Add("cleardebug", _("clears the debug view"), CCommandFuncs::DebugClear);
    m_pCommands->Add("chatscrollup", _("scrolls the chatbox upwards"), CCommandFuncs::ChatScrollUp);
    m_pCommands->Add("chatscrolldown", _("scrolls the chatbox downwards"), CCommandFuncs::ChatScrollDown);
    m_pCommands->Add("debugscrollup", _("scrolls the debug view upwards"), CCommandFuncs::DebugScrollUp);
    m_pCommands->Add("debugscrolldown", _("scrolls the debug view downwards"), CCommandFuncs::DebugScrollDown);

    m_pCommands->Add("test", "", CCommandFuncs::Test);
    m_pCommands->Add("showmemstat", _("shows the memory statistics"), CCommandFuncs::ShowMemStat);
    m_pCommands->Add("showframegraph", _("shows the frame timing graph"), CCommandFuncs::ShowFrameGraph);
    m_pCommands->Add("jinglebells", "", CCommandFuncs::JingleBells);
    m_pCommands->Add("fakelag", "", CCommandFuncs::FakeLag);

    m_pCommands->Add("reloadnews", _("for developers: reload news"), CCommandFuncs::ReloadNews);
}

void CCore::SwitchRenderWindow(HWND hWnd, HWND hWndInput)
{
    assert(0);
#if 0
    // Make GTA windowed
    m_pGame->GetSettings()->SetCurrentVideoMode(0);

    // Get the destination window rectangle
    RECT rect;
    GetWindowRect ( hWnd, &rect );

    // Size the GTA window size to the same size as the destination window rectangle
    HWND hDeviceWindow = CDirect3DData::GetSingleton ().GetDeviceWindow ();
    MoveWindow ( hDeviceWindow,
                 0,
                 0,
                 rect.right - rect.left,
                 rect.bottom - rect.top,
                 TRUE );

    // Turn the GTA window into a child window of our static render container window
    SetParent ( hDeviceWindow, hWnd );
    SetWindowLong ( hDeviceWindow, GWL_STYLE, WS_VISIBLE | WS_CHILD );
#endif
}

bool CCore::IsValidNick(const char* szNick)
{
    // Grab the size of the nick. Check that it's within the player
    size_t sizeNick = strlen(szNick);
    if (sizeNick < MIN_PLAYER_NICK_LENGTH || sizeNick > MAX_PLAYER_NICK_LENGTH)
    {
        return false;
    }

    // Check that each character is valid (visible characters exluding space)
    unsigned char ucTemp;
    for (size_t i = 0; i < sizeNick; i++)
    {
        ucTemp = szNick[i];
        if (ucTemp < 33 || ucTemp > 126)
        {
            return false;
        }
    }

    // Nickname is valid, return true
    return true;
}

void CCore::Quit(bool bInstantly)
{
    if (bInstantly)
    {
        AddReportLog(7101, "Core - Quit");
        // Show that we are quiting (for the crash dump filename)
        SetApplicationSettingInt("last-server-ip", 1);

        WatchDogBeginSection("Q0");  // Allow loader to detect freeze on exit

        // Hide game window to make quit look instant
        PostQuitMessage(0);
        ShowWindow(GetHookedWindow(), SW_HIDE);

        // Destroy the client
        CModManager::GetSingleton().Unload();

        WatchDogCompletedSection("Q0");

        // Use TerminateProcess before destroying CCore to ensure clean exit code (Exiting the normal way also crashes).
        TerminateProcess(GetCurrentProcess(), 0);

        // Destroy ourself (unreachable but kept for completeness)
        delete CCore::GetSingletonPtr();
    }
    else
    {
        m_bQuitOnPulse = true;
    }
}

bool CCore::WasLaunchedWithConnectURI()
{
    if (m_szCommandLineArgs && strnicmp(m_szCommandLineArgs, "mtasa://", 8) == 0)
        return true;
    return false;
}

void CCore::ParseCommandLine(std::map<std::string, std::string>& options, const char*& szArgs, const char** pszNoValOptions)
{
    std::set<std::string> noValOptions;
    if (pszNoValOptions)
    {
        while (*pszNoValOptions)
        {
            noValOptions.insert(*pszNoValOptions);
            pszNoValOptions++;
        }
    }

    const char* szCmdLine = GetCommandLine();

    // Skip the leading game executable path (starts and ends with a quotation mark).
    if (szCmdLine[0] == '"')
    {
        if (const char* afterPath = strchr(szCmdLine + 1, '"'); afterPath != nullptr)
        {
            ++afterPath;

            while (*afterPath && isspace(*afterPath))
                ++afterPath;

            szCmdLine = afterPath;
        }
    }

    char szCmdLineCopy[512];
    STRNCPY(szCmdLineCopy, szCmdLine, sizeof(szCmdLineCopy));

    char*       pCmdLineEnd = szCmdLineCopy + strlen(szCmdLineCopy);
    char*       pStart = szCmdLineCopy;
    char*       pEnd = pStart;
    bool        bInQuoted = false;
    std::string strKey;
    szArgs = NULL;

    while (pEnd != pCmdLineEnd)
    {
        pEnd = strchr(pEnd + 1, ' ');
        if (!pEnd)
            pEnd = pCmdLineEnd;
        if (bInQuoted && *(pEnd - 1) == '"')
            bInQuoted = false;
        else if (*pStart == '"')
            bInQuoted = true;

        if (!bInQuoted)
        {
            *pEnd = 0;
            if (strKey.empty())
            {
                if (*pStart == '-')
                {
                    strKey = pStart + 1;
                    if (noValOptions.find(strKey) != noValOptions.end())
                    {
                        options[strKey] = "";
                        strKey.clear();
                    }
                }
                else
                {
                    szArgs = pStart - szCmdLineCopy + szCmdLine;
                    break;
                }
            }
            else
            {
                if (*pStart == '-')
                {
                    options[strKey] = "";
                    strKey = pStart + 1;
                }
                else
                {
                    if (*pStart == '"')
                        pStart++;
                    if (*(pEnd - 1) == '"')
                        *(pEnd - 1) = 0;
                    options[strKey] = pStart;
                    strKey.clear();
                }
            }
            pStart = pEnd;
            while (pStart != pCmdLineEnd && *(++pStart) == ' ')
                ;
            pEnd = pStart;
        }
    }
}

const char* CCore::GetCommandLineOption(const char* szOption)
{
    std::map<std::string, std::string>::iterator it = m_CommandLineOptions.find(szOption);
    if (it != m_CommandLineOptions.end())
        return it->second.c_str();
    else
        return NULL;
}

SString CCore::GetConnectCommandFromURI(const char* szURI)
{
    unsigned short usPort;
    std::string    strHost, strNick, strPassword;
    GetConnectParametersFromURI(szURI, strHost, usPort, strNick, strPassword);

    // Generate a string with the arguments to send to the mod IF we got a host
    SString strDest;
    if (strHost.size() > 0)
    {
        if (strPassword.size() > 0)
            strDest.Format("connect %s %u %s %s", strHost.c_str(), usPort, strNick.c_str(), strPassword.c_str());
        else
            strDest.Format("connect %s %u %s", strHost.c_str(), usPort, strNick.c_str());
    }

    return strDest;
}

void CCore::GetConnectParametersFromURI(const char* szURI, std::string& strHost, unsigned short& usPort, std::string& strNick, std::string& strPassword)
{
    // Grab the length of the string
    size_t sizeURI = strlen(szURI);

    // Parse it right to left
    char szLeft[256];
    szLeft[255] = 0;
    char* szLeftIter = szLeft + 255;

    char szRight[256];
    szRight[255] = 0;
    char* szRightIter = szRight + 255;

    const char* szIterator = szURI + sizeURI;
    bool        bHitAt = false;

    for (; szIterator >= szURI + 8; szIterator--)
    {
        if (!bHitAt && *szIterator == '@')
        {
            bHitAt = true;
        }
        else
        {
            if (bHitAt)
            {
                if (szLeftIter > szLeft)
                {
                    *(--szLeftIter) = *szIterator;
                }
            }
            else
            {
                if (szRightIter > szRight)
                {
                    *(--szRightIter) = *szIterator;
                }
            }
        }
    }

    // Parse the host/port
    char  szHost[64];
    char  szPort[12];
    char* szHostIter = szHost;
    char* szPortIter = szPort;
    memset(szHost, 0, sizeof(szHost));
    memset(szPort, 0, sizeof(szPort));

    bool   bIsInPort = false;
    size_t sizeRight = strlen(szRightIter);
    for (size_t i = 0; i < sizeRight; i++)
    {
        if (!bIsInPort && szRightIter[i] == ':')
        {
            bIsInPort = true;
        }
        else
        {
            if (bIsInPort)
            {
                if (szPortIter < szPort + 11)
                {
                    *(szPortIter++) = szRightIter[i];
                }
            }
            else
            {
                if (szHostIter < szHost + 63)
                {
                    *(szHostIter++) = szRightIter[i];
                }
            }
        }
    }

    // Parse the nickname / password
    char  szNickname[64];
    char  szPassword[64];
    char* szNicknameIter = szNickname;
    char* szPasswordIter = szPassword;
    memset(szNickname, 0, sizeof(szNickname));
    memset(szPassword, 0, sizeof(szPassword));

    bool   bIsInPassword = false;
    size_t sizeLeft = strlen(szLeftIter);
    for (size_t i = 0; i < sizeLeft; i++)
    {
        if (!bIsInPassword && szLeftIter[i] == ':')
        {
            bIsInPassword = true;
        }
        else
        {
            if (bIsInPassword)
            {
                if (szPasswordIter < szPassword + 63)
                {
                    *(szPasswordIter++) = szLeftIter[i];
                }
            }
            else
            {
                if (szNicknameIter < szNickname + 63)
                {
                    *(szNicknameIter++) = szLeftIter[i];
                }
            }
        }
    }

    // If we got any port, convert it to an integral type
    usPort = 22003;
    if (strlen(szPort) > 0)
    {
        usPort = static_cast<unsigned short>(atoi(szPort));
    }

    // Grab the nickname
    if (strlen(szNickname) > 0)
    {
        strNick = szNickname;
    }
    else
    {
        CVARS_GET("nick", strNick);
    }
    strHost = szHost;
    strPassword = szPassword;
}

void CCore::UpdateRecentlyPlayed()
{
    // Get the current host and port
    unsigned int uiPort;
    std::string  strHost;
    CVARS_GET("host", strHost);
    CVARS_GET("port", uiPort);

    if (uiPort == 0 || uiPort > 0xFFFF)
        return;

    const ushort usPort = static_cast<ushort>(uiPort);
    // Save the connection details into the recently played servers list
    in_addr Address;
    if (CServerListItem::Parse(strHost.c_str(), Address))
    {
        CServerBrowser* pServerBrowser = CCore::GetSingleton().GetLocalGUI()->GetMainMenu()->GetServerBrowser();
        CServerList*    pRecentList = pServerBrowser->GetRecentList();
        pRecentList->Remove(Address, usPort);
        pRecentList->AddUnique(Address, usPort, true);

        pServerBrowser->SaveRecentlyPlayedList();
        if (!m_pConnectManager->m_strLastPassword.empty())
            pServerBrowser->SetServerPassword(strHost + ":" + SString("%u", usPort), m_pConnectManager->m_strLastPassword);
    }
    // Save our configuration file
    CCore::GetSingleton().SaveConfig();
}

void CCore::OnPostColorFilterRender()
{
    if (!CGraphics::GetSingleton().HasLine3DPostFXQueueItems() && !CGraphics::GetSingleton().HasPrimitive3DPostFXQueueItems())
        return;

    CGraphics::GetSingleton().EnteringMTARenderZone();

    CGraphics::GetSingleton().DrawPrimitive3DPostFXQueue();
    CGraphics::GetSingleton().DrawLine3DPostFXQueue();

    CGraphics::GetSingleton().LeavingMTARenderZone();
}

void CCore::ApplyCoreInitSettings()
{
    bool aware = CVARS_GET_VALUE<bool>("process_dpi_aware");

    // The minimum supported client for the function below is Windows Vista (Longhorn).
    // For more information, refer to the Microsoft Learn article:
    // https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
    if (aware)
        SetProcessDPIAware();

    int revision = GetApplicationSettingInt("reset-settings-revision");

    // Users with the default skin will be switched to the 2023 version by replacing "Default" with "Default 2023".
    // The "Default 2023" GUI skin was introduced in commit 2d9e03324b07e355031ecb3263477477f1a91399.
    if (revision && revision < 21486)
    {
        auto skin = CVARS_GET_VALUE<std::string>("current_skin");

        if (skin == "Default")
            CVARS_SET("current_skin", "Default 2023");

        SetApplicationSettingInt("reset-settings-revision", 21486);
    }

    HANDLE    process = GetCurrentProcess();
    const int priorities[] = {NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS};
    int       priority = CVARS_GET_VALUE<int>("process_priority") % 3;

    SetPriorityClass(process, priorities[priority]);
}

//
// Called just before GTA calculates frame time deltas
//
void CCore::OnGameTimerUpdate()
{
    // GTA samples its clock right after this. Waiting here keeps the frame deltas it
    // measures exact, whatever runs between Present and the next frame does not leak in
    if (m_pFPSLimiter)
        m_pFPSLimiter->OnGameTimerUpdate();
}

void CCore::OnFPSLimitChange(std::uint16_t fps)
{
    if (m_pNet != nullptr && IsWebCoreLoaded())  // We have to wait for the network module to be loaded
        GetWebCore()->OnFPSLimitChange(fps);
}

//
// DoReliablePulse
//
// This is called once a frame even if minimized
//
void CCore::DoReliablePulse()
{
    ms_TimingCheckpoints.BeginTimingCheckpoints();
    TIMING_CHECKPOINT("+CallIdle2");

    UpdateIsWindowMinimized();

    // Non frame rate limit stuff
    if (IsWindowMinimized())
        m_iUnminimizeFrameCounter = 4;  // Tell script we have unminimized after a short delay

    UpdateModuleTickCount64();
}

//
// Debug timings
//
bool CCore::IsTimingCheckpoints()
{
    return ms_TimingCheckpoints.IsTimingCheckpoints();
}

void CCore::OnTimingCheckpoint(const char* szTag)
{
    ms_TimingCheckpoints.OnTimingCheckpoint(szTag);
}

void CCore::OnTimingDetail(const char* szTag)
{
    ms_TimingCheckpoints.OnTimingDetail(szTag);
}

//
// OnDeviceRestore
//
void CCore::OnDeviceRestore()
{
    m_iUnminimizeFrameCounter = 4;  // Tell script we have restored after 4 frames to avoid double sends
    m_bDidRecreateRenderTargets = true;
    m_uiNextRenderTargetRetryTime = 0;
}

//
// OnPreFxRender
//
void CCore::OnPreFxRender()
{
    if (!CGraphics::GetSingleton().HasLine3DPreGUIQueueItems() && !CGraphics::GetSingleton().HasPrimitive3DPreGUIQueueItems())
        return;

    CGraphics::GetSingleton().EnteringMTARenderZone();

    CGraphics::GetSingleton().DrawPrimitive3DPreGUIQueue();
    CGraphics::GetSingleton().DrawLine3DPreGUIQueue();

    CGraphics::GetSingleton().LeavingMTARenderZone();
}

//
// OnPreHUDRender
//
void CCore::OnPreHUDRender()
{
    const uint uiNow = GetTickCount32();
    if (uiNow >= m_uiNextRenderTargetRetryTime)
    {
        CGraphics::GetSingleton().RetryInvalidRenderTargets();
        m_uiNextRenderTargetRetryTime = uiNow + 250;
    }

    CGraphics::GetSingleton().EnteringMTARenderZone();

    // Maybe capture screen and other stuff
    CGraphics::GetSingleton().GetRenderItemManager()->DoPulse();

    // Handle script stuffs
    if (m_iUnminimizeFrameCounter && --m_iUnminimizeFrameCounter == 0)
    {
        m_pModManager->DoPulsePreHUDRender(true, m_bDidRecreateRenderTargets);
        m_bDidRecreateRenderTargets = false;
    }
    else
        m_pModManager->DoPulsePreHUDRender(false, false);

    // Handle saving depth buffer
    CGraphics::GetSingleton().GetRenderItemManager()->SaveReadableDepthBuffer();

    // Restore in case script forgets
    CGraphics::GetSingleton().GetRenderItemManager()->RestoreDefaultRenderTarget();

    // Draw pre-GUI primitives
    CGraphics::GetSingleton().DrawPreGUIQueue();

    CGraphics::GetSingleton().LeavingMTARenderZone();
}

//
// CCore::CalculateStreamingMemoryRange
//
// Streaming memory range based on system installed memory:
//
//     System RAM MB     min     max
//           512     =   64      96
//          1024     =   96     128
//          2048     =  128     256
//
// Also:
//   Max should be no more than 2 * installed video memory
//   Min should be no more than 1 * installed video memory
//   Max should be no less than 96MB
//   Gap between min and max should be no less than 32MB
//
void CCore::CalculateStreamingMemoryRange()
{
    // Only need to do this once
    if (m_fMinStreamingMemory != 0)
        return;

    // Get system info
    int iSystemRamMB = static_cast<int>(GetWMITotalPhysicalMemory() / 1024LL / 1024LL);
    int iVideoMemoryMB = g_pDeviceState->AdapterState.InstalledMemoryKB / 1024;

    // Calc min and max from lookup table
    SSamplePoint<float> minPoints[] = {{512, 64}, {1024, 96}, {2048, 128}};
    SSamplePoint<float> maxPoints[] = {{512, 96}, {1024, 128}, {2048, 256}};

    float fMinAmount = EvalSamplePosition<float>(minPoints, NUMELMS(minPoints), iSystemRamMB);
    float fMaxAmount = EvalSamplePosition<float>(maxPoints, NUMELMS(maxPoints), iSystemRamMB);

    // Scale max if gta3.img is over 1GB
    SString strGta3imgFilename = PathJoin(UTF8FilePath(g_gtaDirectory), "models", "gta3.img");
    uint    uiFileSizeMB = FileSize(strGta3imgFilename) / 0x100000LL;
    float   fSizeScale = UnlerpClamped(1024, uiFileSizeMB, 2048);
    fMaxAmount += fMaxAmount * fSizeScale;

    // Apply cap dependant on video memory
    fMaxAmount = std::min(fMaxAmount, iVideoMemoryMB * 2.f);
    fMinAmount = std::min(fMinAmount, iVideoMemoryMB * 1.f);

    // Apply 96MB lower limit
    fMaxAmount = std::max(fMaxAmount, 96.f);

    // Apply gap limit
    fMinAmount = fMaxAmount - std::max(fMaxAmount - fMinAmount, 32.f);

    m_fMinStreamingMemory = fMinAmount;
    m_fMaxStreamingMemory = fMaxAmount;
}

//
// GetMinStreamingMemory
//
uint CCore::GetMinStreamingMemory()
{
    CalculateStreamingMemoryRange();

#ifdef MTA_DEBUG
    return 1;
#endif
    return m_fMinStreamingMemory;
}

//
// GetMaxStreamingMemory
//
uint CCore::GetMaxStreamingMemory()
{
    CalculateStreamingMemoryRange();
    return m_fMaxStreamingMemory;
}

//
// OnCrashAverted
//
void CCore::OnCrashAverted(uint uiId)
{
    CCrashDumpWriter::OnCrashAverted(uiId);
}

//
// OnEnterCrashZone
//
void CCore::OnEnterCrashZone(uint uiId)
{
    CCrashDumpWriter::OnEnterCrashZone(uiId);
}

void CCore::UpdateWerCrashModuleBases()
{
    WerCrash::UpdateModuleBases();
}

//
// LogEvent
//
void CCore::LogEvent(uint uiDebugId, const char* szType, const char* szContext, const char* szBody, uint uiAddReportLogId)
{
    if (uiAddReportLogId)
        AddReportLog(uiAddReportLogId, SString("%s - %s", szContext, szBody));

    if (GetDebugIdEnabled(uiDebugId))
    {
        CCrashDumpWriter::LogEvent(szType, szContext, szBody);
        OutputDebugLine(SString("[LogEvent] %d %s %s %s", uiDebugId, szType, szContext, szBody));
    }
}

//
// GetDebugIdEnabled
//
bool CCore::GetDebugIdEnabled(uint uiDebugId)
{
    static CFilterMap debugIdFilterMap(GetVersionUpdater()->GetDebugFilterString());
    return (uiDebugId == 0) || !debugIdFilterMap.IsFiltered(uiDebugId);
}

EDiagnosticDebugType CCore::GetDiagnosticDebug()
{
    return m_DiagnosticDebug;
}

void CCore::SetDiagnosticDebug(EDiagnosticDebugType value)
{
    m_DiagnosticDebug = value;
}

CModelCacheManager* CCore::GetModelCacheManager()
{
    if (!m_pModelCacheManager)
        m_pModelCacheManager = NewModelCacheManager();
    return m_pModelCacheManager;
}

void CCore::StaticIdleHandler()
{
    g_pCore->IdleHandler();
}

// Gets called every game loop, after GTA has been loaded for the first time
void CCore::IdleHandler()
{
    m_bGettingIdleCallsFromMultiplayer = true;
    HandleIdlePulse();
}

// Gets called every 50ms, before GTA has been loaded for the first time
void CCore::WindowsTimerHandler()
{
    if (!m_bGettingIdleCallsFromMultiplayer)
        HandleIdlePulse();
}

// Always called, even if minimized
void CCore::HandleIdlePulse()
{
    UpdateIsWindowMinimized();

    if (IsWindowMinimized())
    {
        DoPreFramePulse();
        DoPostFramePulse();
    }

    if (m_pModManager->IsLoaded())
        m_pModManager->GetClient()->IdleHandler();
}

namespace
{
    bool IsCoreDump(const SString& filePath)
    {
        constexpr std::array<std::uint32_t, 4> markers = {
            0x734C4F50,  // 'POLs'
            0x73443344,  // 'D3Ds'
            0x73474F4C,  // 'LOGs'
            0x73524557   // 'WERs' - WER fail-fast crash info
        };

        constexpr std::size_t tailSize = 64 * 1024;
        constexpr std::size_t minFileSize = 1024;

        std::ifstream file(FromUTF8(filePath), std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const auto fileSize = file.tellg();
        if (fileSize < static_cast<std::streamoff>(minFileSize))
            return false;

        const auto readSize = std::min(static_cast<std::size_t>(fileSize), tailSize);
        const auto readStart = static_cast<std::streamoff>(fileSize) - static_cast<std::streamoff>(readSize);

        file.seekg(readStart);
        std::vector<char> buffer(readSize);
        file.read(buffer.data(), static_cast<std::streamsize>(readSize));

        const auto bytesRead = static_cast<std::size_t>(file.gcount());
        if (bytesRead < sizeof(std::uint32_t))
            return false;

        for (std::size_t i = 0; i + sizeof(std::uint32_t) <= bytesRead; ++i)
        {
            std::uint32_t value;
            std::memcpy(&value, buffer.data() + i, sizeof(std::uint32_t));

            if (std::find(markers.begin(), markers.end(), value) != markers.end())
                return true;
        }

        return false;
    }
}

//
// Handle encryption of Windows crash dump files
//
void CCore::HandleCrashDumpEncryption()
{
    const int iMaxFiles = 10;
    SString   strDumpDirPath = CalcMTASAPath("mta\\dumps");
    SString   strDumpDirPrivatePath = PathJoin(strDumpDirPath, "private");
    SString   strDumpDirPublicPath = PathJoin(strDumpDirPath, "public");
    MakeSureDirExists(strDumpDirPrivatePath + "/");
    MakeSureDirExists(strDumpDirPublicPath + "/");

    SString strMessage = "Dump files in this directory are encrypted and copied to 'dumps\\public' during startup\n\n";
    FileSave(PathJoin(strDumpDirPrivatePath, "README.txt"), strMessage);

    // Limit number of files in the private folder
    {
        std::vector<SString> privateList = FindFiles(PathJoin(strDumpDirPrivatePath, "*.dmp"), true, false, true);
        for (int i = 0; i < (int)privateList.size() - iMaxFiles; i++)
            FileDelete(PathJoin(strDumpDirPrivatePath, privateList[i]));
    }

    // Copy and encrypt private files to public if they don't already exist
    {
        std::vector<SString> privateList = FindFiles(PathJoin(strDumpDirPrivatePath, "*.dmp"), true, false);
        for (uint i = 0; i < privateList.size(); i++)
        {
            const SString& strPrivateFilename = privateList[i];
            SString        strPublicFilename = ExtractBeforeExtension(strPrivateFilename) + ".rsa." + ExtractExtension(strPrivateFilename);
            SString        strPrivatePathFilename = PathJoin(strDumpDirPrivatePath, strPrivateFilename);
            SString        strPublicPathFilename = PathJoin(strDumpDirPublicPath, strPublicFilename);
            if (!FileExists(strPublicPathFilename))
            {
                if (!IsCoreDump(strPrivatePathFilename))
                    continue;

                if (CNet* pNet = GetNetwork(); pNet && pNet->IsReady())
                {
                    pNet->EncryptDumpfile(strPrivatePathFilename, strPublicPathFilename);
                }
            }
        }
    }

    // Limit number of files in the public folder
    {
        std::vector<SString> publicList = FindFiles(PathJoin(strDumpDirPublicPath, "*.dmp"), true, false, true);
        for (int i = 0; i < (int)publicList.size() - iMaxFiles; i++)
            FileDelete(PathJoin(strDumpDirPublicPath, publicList[i]));
    }

    // And while we are here, limit number of items in core.log as well
    {
        SString strCoreLogPathFilename = CalcMTASAPath("mta\\core.log");
        SString strFileContents;
        FileLoad(strCoreLogPathFilename, strFileContents);

        SString              strDelmiter = "** -- Unhandled exception -- **";
        std::vector<SString> parts;
        strFileContents.Split(strDelmiter, parts);

        if (parts.size() > iMaxFiles)
        {
            strFileContents = strDelmiter + strFileContents.Join(strDelmiter, parts, parts.size() - iMaxFiles);
            FileSave(strCoreLogPathFilename, strFileContents);
        }
    }
}

//
// Flag to make sure stuff only gets done when everything is ready
//
void CCore::SetModulesLoaded(bool bLoaded)
{
    m_bModulesLoaded = bLoaded;
}

bool CCore::AreModulesLoaded()
{
    return m_bModulesLoaded;
}

//
// Handle dummy progress when game seems stalled
//
int ms_iDummyProgressTimerCounter = 0;

void CALLBACK TimerProc(void* lpParametar, BOOLEAN TimerOrWaitFired)
{
    ms_iDummyProgressTimerCounter++;
}

//
// Refresh progress output
//
void CCore::UpdateDummyProgress(int iValue, const char* szType)
{
    if (iValue != -1)
    {
        m_iDummyProgressValue = iValue;
        m_strDummyProgressType = szType;
    }

    if (m_DummyProgressTimerHandle == NULL)
    {
        // Using this timer is quicker than checking tick count with every call to UpdateDummyProgress()
        ::CreateTimerQueueTimer(&m_DummyProgressTimerHandle, NULL, TimerProc, this, DUMMY_PROGRESS_ANIMATION_INTERVAL, DUMMY_PROGRESS_ANIMATION_INTERVAL,
                                WT_EXECUTEINTIMERTHREAD);
    }

    if (!ms_iDummyProgressTimerCounter)
        return;
    ms_iDummyProgressTimerCounter = 0;

    // Compose message with amount
    SString strMessage;
    if (m_iDummyProgressValue)
        strMessage = SString("%d%s", m_iDummyProgressValue, *m_strDummyProgressType);

    CGraphics::GetSingleton().SetProgressMessage(strMessage);
}

//
// Do SetCursorPos if allowed
//
void CCore::CallSetCursorPos(int X, int Y)
{
    if (CCore::GetSingleton().IsFocused() && !CLocalGUI::GetSingleton().IsMainMenuVisible())
        m_pLocalGUI->SetCursorPos(X, Y, true);
}

bool CCore::GetRequiredDisplayResolution(int& iOutWidth, int& iOutHeight, int& iOutColorBits, int& iOutAdapterIndex, bool& bOutAllowUnsafeResolutions)
{
    CVARS_GET("show_unsafe_resolutions", bOutAllowUnsafeResolutions);
    return GetVideoModeManager()->GetRequiredDisplayResolution(iOutWidth, iOutHeight, iOutColorBits, iOutAdapterIndex);
}

bool CCore::GetDeviceSelectionEnabled()
{
    return GetApplicationSettingInt("device-selection-disabled") ? false : true;
}

void CCore::NotifyRenderingGrass(bool bIsRenderingGrass)
{
    m_bIsRenderingGrass = bIsRenderingGrass;
    CDirect3DEvents9::CloseActiveShader();
}

bool CCore::GetRightSizeTxdEnabled()
{
    if (g_pCore->GetDiagnosticDebug() == EDiagnosticDebug::RESIZE_NEVER_0000)
        return false;
    if (g_pCore->GetDiagnosticDebug() == EDiagnosticDebug::RESIZE_ALWAYS_0000)
        return true;

    // 32 bit users get rightsized txds
    if (!Is64BitOS())
        return true;

    // Low ram users get rightsized txds
    int iSystemRamMB = static_cast<int>(GetWMITotalPhysicalMemory() / 1024LL / 1024LL);
    if (iSystemRamMB <= 2048)
        return true;

    return false;
}

SString CCore::GetBlueCopyrightString()
{
    SString strCopyright = BLUE_COPYRIGHT_STRING;
    return strCopyright.Replace("%BUILD_YEAR%", std::to_string(BUILD_YEAR).c_str());
}

// Set streaming memory size override [See `engineStreamingSetMemorySize`]
// Use `0` to turn it off, and thus restore the value to the `cvar` setting
void CCore::SetCustomStreamingMemory(size_t sizeBytes)
{
    // NOTE: The override is applied to the game in `CClientGame::DoPulsePostFrame`
    // There's no specific reason we couldn't do it here, but we wont
    m_CustomStreamingMemoryLimitBytes = sizeBytes;
}

bool CCore::IsUsingCustomStreamingMemorySize()
{
    return m_CustomStreamingMemoryLimitBytes != 0;
}

// Streaming memory size used [In Bytes]
size_t CCore::GetStreamingMemory()
{
    return IsUsingCustomStreamingMemorySize() ? m_CustomStreamingMemoryLimitBytes
                                              : CVARS_GET_VALUE<size_t>("streaming_memory") * 1024 * 1024;  // MB to B conversion
}

// Discord rich presence
std::shared_ptr<CDiscordInterface> CCore::GetDiscord()
{
    return m_pDiscordRichPresence;
}

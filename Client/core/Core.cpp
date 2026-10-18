/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Client/core/Core.cpp
 *  PURPOSE:     Core library entry point
 *
 *  Multi Theft Auto is available from https://multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <filesystem>
#include <stdexcept>
#define DECLARE_PROFILER_SECTION_Core
#include "profiler/SharedUtil.Profiler.h"
#define UTF8_FILE_HOOKS_PERSONALITY_Core
#include "SharedUtil.Win32Utf8FileHooks.hpp"
#include "SharedUtil.Memory.h"
#include "DefinitiveStartupState.h"
#include "DefinitiveInputReference.h"
#include "DefinitiveMovementReference.h"
#include "DefinitiveClockReference.h"
#include "DefinitiveClockSetReference.h"
#include "DefinitiveRenderReference.h"
#include "DefinitiveGuiStartupReference.h"
#include "DefinitiveGuiSurfaceReference.h"
#include "DefinitiveGuiFrontendInputReference.h"
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include "DefinitiveProcess.h"
    #include "DefinitiveFrontendStartup.h"
#endif

#define CORE_API extern "C" __declspec(dllexport)

namespace fs = std::filesystem;

CCore*         g_pCore = NULL;
CGraphics*     g_pGraphics = NULL;
CLocalization* g_pLocalization = NULL;

HINSTANCE g_hModule = NULL;

fs::path g_gtaDirectory;
fs::path g_mtaDirectory;

namespace
{
    bool g_utf8FileHooksInstalled = false;
    bool g_isDefinitiveProcessCandidate = false;
}

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
namespace DefinitiveInputReference
{
    namespace
    {
        SRWLOCK requestLock = SRWLOCK_INIT;
        Request configuredRequest;
        DWORD   requestOwner = 0;
        bool    pending = false;
        bool    active = false;
    }

    bool Configure(const Request& request) noexcept
    {
        const auto maximum = request.clockSetReference   ? DefinitiveClockSetReference::MaximumDurationMs
                             : request.clockReference    ? DefinitiveClockReference::MaximumDurationMs
                             : request.guiFrontendInput  ? DefinitiveGuiFrontendInputReference::MaximumDurationMs
                             : request.guiFrontend       ? DefinitiveGuiFrontendReference::MaximumDurationMs
                             : request.guiSurface        ? DefinitiveGuiSurfaceReference::MaximumDurationMs
                             : request.guiStartup        ? DefinitiveGuiStartupReference::MaximumDurationMs
                             : request.renderObservation ? DefinitiveRenderReference::MaximumDurationMs
                             : request.nativeMovement    ? DefinitiveMovementReference::MaximumDurationMs
                                                         : MaximumDurationMs;
        if ((request.clockReference && request.clockSetReference) ||
            (request.clockSetReference && request.durationMs < DefinitiveClockSetReference::MinimumDurationMs) ||
            ((request.clockReference || request.clockSetReference) &&
             (request.guiFrontendInput || request.guiFrontend || request.guiSurface || request.guiStartup || request.nativeMovement ||
              request.renderObservation || request.renderRoute || request.renderApiVersion != 1 || request.controlLease)) ||
            (request.guiFrontendInput && (request.guiFrontend || request.guiSurface || request.guiStartup || request.nativeMovement ||
                                          request.renderObservation || request.renderRoute || request.renderApiVersion != 1 || request.controlLease)) ||
            (request.guiFrontend && (request.guiSurface || request.guiStartup || request.nativeMovement || request.renderObservation || request.renderRoute ||
                                     request.renderApiVersion != 1 || !request.controlLease)) ||
            (!request.guiFrontend && request.controlLease) ||
            (request.guiSurface &&
             (request.guiStartup || request.nativeMovement || request.renderObservation || request.renderRoute || request.renderApiVersion != 1)) ||
            (request.guiStartup && (request.nativeMovement || request.renderObservation || request.renderRoute || request.renderApiVersion != 1)) ||
            (request.nativeMovement && request.renderObservation) || (request.renderRoute && (!request.renderObservation || request.renderRoute > 2)) ||
            (request.renderObservation &&
             ((request.renderApiVersion == 1 && request.renderRoute) || (request.renderApiVersion == 2 && !request.renderRoute) ||
              (request.renderApiVersion >= 3 && request.renderRoute != 2) || !request.renderApiVersion || request.renderApiVersion > 6)) ||
            !request.durationMs || request.durationMs > maximum || !request.cancelled || !request.publish || !TryAcquireSRWLockExclusive(&requestLock))
            return false;
        const bool accepted = !pending && !active;
        if (accepted)
        {
            configuredRequest = request;
            requestOwner = GetCurrentThreadId();
            pending = true;
        }
        ReleaseSRWLockExclusive(&requestLock);
        return accepted;
    }

    bool Activate() noexcept
    {
        AcquireSRWLockExclusive(&requestLock);
        const bool accepted = pending && !active && requestOwner == GetCurrentThreadId();
        if (accepted)
        {
            pending = false;
            active = true;
        }
        ReleaseSRWLockExclusive(&requestLock);
        return accepted;
    }

    bool GetActiveRequest(Request& request) noexcept
    {
        AcquireSRWLockShared(&requestLock);
        const bool available = active && requestOwner == GetCurrentThreadId();
        if (available)
            request = configuredRequest;
        ReleaseSRWLockShared(&requestLock);
        return available;
    }

    void Complete() noexcept
    {
        AcquireSRWLockExclusive(&requestLock);
        if (active && requestOwner == GetCurrentThreadId())
        {
            configuredRequest = {};
            requestOwner = 0;
            active = false;
        }
        ReleaseSRWLockExclusive(&requestLock);
    }
}
#endif

BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hModule = dll;
        DisableThreadLibraryCalls(dll);
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
        g_isDefinitiveProcessCandidate = DefinitiveProcess::IsCurrentCandidate();
#endif
        // The classic file-hook setup predates DE. Defer DE engine validation
        // to InitializeCore instead of installing legacy hooks under loader lock.
        if (!g_isDefinitiveProcessCandidate)
        {
            AddUtf8FileHooks();
            g_utf8FileHooksInstalled = true;
        }
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        if (g_utf8FileHooksInstalled)
            RemoveUtf8FileHooks();

        if (!g_isDefinitiveProcessCandidate && IsGTAProcess())
        {
            // For now, ExitProcess if any destruction is attempted (or we'll crash)
            ExitProcess(0);

            if (g_pCore)
            {
                AddReportLog(7102, "Core - PROCESS_DETACH");

                delete g_pCore;
                g_pCore = nullptr;
            }
        }
    }

    return TRUE;
}

CORE_API void SetMTADirectory(const wchar_t* path, size_t length)
{
    if (!g_mtaDirectory.empty() || !path || !path[0] || !length)
        return;

    g_mtaDirectory.assign(std::wstring_view{path, length});
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    // The DE owner supplies its explicit diagnostic root outside loader lock.
    // It need not resemble a classic MTA installation; use the existing root
    // override before startup logging can fall back to registry/localization.
    SharedUtil::SetMTASABaseDirOverride(ToUTF8(g_mtaDirectory.wstring()));
#endif
}

CORE_API void SetGTADirectory(const wchar_t* path, size_t length)
{
    if (!g_gtaDirectory.empty() || !path || !path[0] || !length)
        return;

    g_gtaDirectory.assign(std::wstring_view{path, length});
}

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
CORE_API std::uint32_t MTA_DEFrontendStartupApiVersion()
{
    return DefinitiveFrontendStartup::ApiVersion;
}

CORE_API void* MTA_BeginDEFrontendStartup(std::uint32_t version)
{
    // A completed frontend is a separate opt-in lifecycle. Its success cannot
    // enter InitializeCore's FullCoreReady path or skip the gameplay gate.
    if (version != DefinitiveFrontendStartup::ApiVersion || !g_isDefinitiveProcessCandidate || g_mtaDirectory.empty() || g_gtaDirectory.empty())
        return nullptr;
    return DefinitiveFrontendStartup::BeginOwner();
}

CORE_API DefinitiveFrontendStartup::Status MTA_PulseDEFrontendStartup(void* handle)
{
    return DefinitiveFrontendStartup::PulseOwner(handle);
}

CORE_API DefinitiveFrontendStartup::Status MTA_BindDEFrontendStartupInput(void* handle)
{
    return DefinitiveFrontendStartup::BindOwner(handle);
}

CORE_API DefinitiveFrontendStartup::Status MTA_ReadDEFrontendStartup(void* handle, std::uint32_t* output, std::uint32_t count)
{
    return DefinitiveFrontendStartup::ReadOwner(handle, output, count);
}

CORE_API DefinitiveFrontendStartup::Status MTA_ShutdownDEFrontendStartup(void* handle)
{
    return DefinitiveFrontendStartup::ShutdownOwner(handle);
}

CORE_API DefinitiveFrontendStartup::Status MTA_ReleaseDEFrontendStartup(void* handle)
{
    return DefinitiveFrontendStartup::ReleaseOwner(handle);
}

CORE_API DefinitiveFrontendStartup::Status MTA_RunDEFrontendStartupView(void* handle, std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                                        DefinitiveInputReference::PublishCallback publish)
{
    return DefinitiveFrontendStartup::RunViewOwner(handle, durationMs, cancel, publish);
}

CORE_API std::uint32_t MTA_DEPersistentFrontendApiVersion()
{
    return DefinitivePersistentFrontend::ApiVersion;
}

CORE_API DefinitivePersistentFrontend::Status MTA_RunDEPersistentFrontendV1(void* handle, const DefinitivePersistentFrontend::RequestV1* request)
{
    return DefinitivePersistentFrontend::RunOwner(handle, request);
}
#endif

CORE_API int InitializeCore()
{
    DefinitiveStartup::BeginAttempt();
    if (g_pCore)
        return DefinitiveStartup::CompleteResult(1);

    if (!g_isDefinitiveProcessCandidate && !IsGTAProcess())
        return DefinitiveStartup::CompleteResult(2);

    if (g_mtaDirectory.empty())
        return DefinitiveStartup::CompleteResult(3);

    if (g_gtaDirectory.empty())
        return DefinitiveStartup::CompleteResult(4);

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    // A request belongs to this one real initialization attempt. The worker
    // keeps callback storage alive until this scope and the partial Core unwind.
    struct InputReferenceAttempt
    {
        bool active = DefinitiveInputReference::Activate();
        ~InputReferenceAttempt()
        {
            if (active)
                DefinitiveInputReference::Complete();
        }
    } inputReferenceAttempt;
#endif

    DefinitiveStartup::SetPhase(DefinitiveStartup::Phase::DirectoriesReady);

    // Group our processes and windows under a single taskbar button
    SetCurrentProcessExplicitAppUserModelID(L"Multi Theft Auto " MTA_STR(MTASA_VERSION_MAJOR) L"." MTA_STR(MTASA_VERSION_MINOR));

    WriteDebugEvent(SString("ModuleFileName: %s", *GetLaunchPathFilename()));

    SetMemoryAllocationFailureHandler();

#ifdef MTA_WITH_DEFINITIVE_ADAPTER
    // Adapter prerequisites are reported through MTA's initialization result;
    // no C++ exception may escape the exported loader entry point. The partial
    // constructor's module member releases the backend before this returns.
    try
    {
        g_pCore = new CCore();
    }
    catch (const std::exception& error)
    {
        g_pCore = nullptr;
        WriteDebugEvent(SString("DE adapter initialization rejected: %s", error.what()));
        return DefinitiveStartup::CompleteResult(5);
    }
#else
    g_pCore = new CCore();
#endif
    return DefinitiveStartup::CompleteResult(0);
}

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
CORE_API std::uint32_t MTA_DEStartupApiVersion()
{
    return 1;
}

CORE_API std::uint32_t MTA_ReadDEStartupState(std::uint32_t field)
{
    return DefinitiveStartup::Read(field);
}

CORE_API std::uint32_t MTA_DEInputReferenceApiVersion()
{
    return DefinitiveInputReference::ApiVersion;
}

CORE_API std::uint32_t MTA_DEClockReferenceApiVersion()
{
    return DefinitiveClockReference::ApiVersion;
}

CORE_API std::uint32_t MTA_ConfigureDEClockReference(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                     DefinitiveInputReference::PublishCallback publish)
{
    DefinitiveInputReference::Request request{durationMs, cancel, publish};
    request.clockReference = true;
    return g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure(request) ? 1u : 0u;
}

CORE_API std::uint32_t MTA_DEClockSetReferenceApiVersion()
{
    return DefinitiveClockSetReference::ApiVersion;
}

CORE_API std::uint32_t MTA_ConfigureDEClockSetReference(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                        DefinitiveInputReference::PublishCallback publish)
{
    DefinitiveInputReference::Request request{durationMs, cancel, publish};
    request.clockSetReference = true;
    return g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure(request) ? 1u : 0u;
}

CORE_API std::uint32_t MTA_ConfigureDEInputReference(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                     DefinitiveInputReference::PublishCallback publish)
{
    return g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure({durationMs, cancel, publish}) ? 1u : 0u;
}

CORE_API std::uint32_t MTA_DEMovementReferenceApiVersion()
{
    return DefinitiveMovementReference::ApiVersion;
}

CORE_API std::uint32_t MTA_ConfigureDEMovementReference(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                        DefinitiveInputReference::PublishCallback publish)
{
    return g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure({durationMs, cancel, publish, true}) ? 1u : 0u;
}

CORE_API std::uint32_t MTA_DERenderReferenceApiVersion()
{
    return DefinitiveRenderReference::ApiVersion;
}

CORE_API std::uint32_t MTA_ConfigureDERenderReference(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                      DefinitiveInputReference::PublishCallback publish)
{
    return g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure({durationMs, cancel, publish, false, true}) ? 1u : 0u;
}

CORE_API std::uint32_t MTA_DERenderReferenceApiVersionV2()
{
    return DefinitiveRenderReference::ApiVersionV2;
}

CORE_API std::uint32_t MTA_ConfigureDERenderReferenceV2(std::uint32_t durationMs, std::uint32_t route, DefinitiveInputReference::CancelCallback cancel,
                                                        DefinitiveInputReference::PublishCallback publish)
{
    // A separate export makes route choice explicit without changing API1's
    // signature or silently claiming that the requested RHI is active.
    return (route == 1 || route == 2) && g_isDefinitiveProcessCandidate &&
                   DefinitiveInputReference::Configure({durationMs, cancel, publish, false, true, route, 2})
               ? 1u
               : 0u;
}
CORE_API std::uint32_t MTA_DERenderReferenceApiVersionV3()
{
    return DefinitiveRenderReference::ApiVersionV3;
}
CORE_API std::uint32_t MTA_ConfigureDERenderReferenceV3(std::uint32_t durationMs, std::uint32_t route, DefinitiveInputReference::CancelCallback cancel,
                                                        DefinitiveInputReference::PublishCallback publish)
{
    return route == 2 && g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure({durationMs, cancel, publish, false, true, route, 3}) ? 1u : 0u;
}
CORE_API std::uint32_t MTA_DERenderReferenceApiVersionV4()
{
    return DefinitiveRenderReference::ApiVersionV4;
}
CORE_API std::uint32_t MTA_ConfigureDERenderReferenceV4(std::uint32_t durationMs, std::uint32_t route, DefinitiveInputReference::CancelCallback cancel,
                                                        DefinitiveInputReference::PublishCallback publish)
{
    // A distinct export admits classification explicitly; historical callers
    // retain their existing field counts and cannot silently opt into API4.
    return route == 2 && g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure({durationMs, cancel, publish, false, true, route, 4}) ? 1u : 0u;
}
CORE_API std::uint32_t MTA_DERenderReferenceApiVersionV5()
{
    return DefinitiveRenderReference::ApiVersionV5;
}
CORE_API std::uint32_t MTA_ConfigureDERenderReferenceV5(std::uint32_t durationMs, std::uint32_t route, DefinitiveInputReference::CancelCallback cancel,
                                                        DefinitiveInputReference::PublishCallback publish)
{
    // Observing the later standard Present endpoint is an explicit opt-in;
    // older diagnostics keep their existing callback and scalar contracts.
    return route == 2 && g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure({durationMs, cancel, publish, false, true, route, 5}) ? 1u : 0u;
}

// The native callsite has its own version so API5 keeps its DXGI-entry meaning.
CORE_API std::uint32_t MTA_DERenderReferenceApiVersionV6()
{
    return DefinitiveRenderReference::ApiVersionV6;
}
CORE_API std::uint32_t MTA_ConfigureDERenderReferenceV6(std::uint32_t durationMs, std::uint32_t route, DefinitiveInputReference::CancelCallback cancel,
                                                        DefinitiveInputReference::PublishCallback publish)
{
    // Dispatch through the native callsite preserves an existing Present hook;
    // a separate version prevents that seam from relabeling the API5 endpoint.
    return route == 2 && g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure({durationMs, cancel, publish, false, true, route, 6}) ? 1u : 0u;
}

CORE_API std::uint32_t MTA_DEGuiStartupReferenceApiVersion()
{
    return DefinitiveGuiStartupReference::ApiVersion;
}
CORE_API std::uint32_t MTA_ConfigureDEGuiStartupReference(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                          DefinitiveInputReference::PublishCallback publish)
{
    // An independent export is required because ordinary read-only API6 must
    // never acquire a device or run MTA's offscreen GPU workload implicitly.
    DefinitiveInputReference::Request request{durationMs, cancel, publish};
    request.guiStartup = true;
    return g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure(request) ? 1u : 0u;
}
CORE_API std::uint32_t MTA_DEGuiSurfaceReferenceApiVersion()
{
    return DefinitiveGuiSurfaceReference::ApiVersion;
}
CORE_API std::uint32_t MTA_ConfigureDEGuiSurfaceReference(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                          DefinitiveInputReference::PublishCallback publish)
{
    // Surface presentation is explicit and remains separate from the proven
    // offscreen workload and every native frame observation contract.
    DefinitiveInputReference::Request request{durationMs, cancel, publish};
    request.guiSurface = true;
    return g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure(request) ? 1u : 0u;
}
CORE_API std::uint32_t MTA_DEGuiFrontendReferenceApiVersion()
{
    return DefinitiveGuiFrontendReference::ApiVersion;
}
CORE_API std::uint32_t MTA_ConfigureDEGuiFrontendReference(std::uint32_t version, std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                           DefinitiveInputReference::PublishCallback       publish,
                                                           DefinitiveGuiFrontendReference::ControlCallback control)
{
    // This independent workload cannot upgrade passive Surface96 into native
    // input capture. The pinned bridge keeps the scalar lease callback alive.
    DefinitiveInputReference::Request request{durationMs, cancel, publish};
    request.guiFrontend = true;
    request.controlLease = control;
    return version == DefinitiveGuiFrontendReference::ApiVersion && g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure(request) ? 1u : 0u;
}
CORE_API std::uint32_t MTA_DEGuiFrontendInputReferenceApiVersion()
{
    return DefinitiveGuiFrontendInputReference::ApiVersion;
}
CORE_API std::uint32_t MTA_ConfigureDEGuiFrontendInputReference(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                                                                DefinitiveInputReference::PublishCallback publish)
{
    // A separate export cannot grant native input to a passive or historical
    // script-owned frontend request. No control-mailbox callback is accepted.
    DefinitiveInputReference::Request request{durationMs, cancel, publish};
    request.guiFrontendInput = true;
    return g_isDefinitiveProcessCandidate && DefinitiveInputReference::Configure(request) ? 1u : 0u;
}
#endif

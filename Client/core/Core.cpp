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
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include "DefinitiveProcess.h"
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
#endif

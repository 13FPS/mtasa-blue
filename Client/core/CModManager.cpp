/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Client/core/CModManager.cpp
 *  PURPOSE:     Game mod loading manager
 *
 *  Multi Theft Auto is available from https://multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <SharedUtil.Misc.h>
#include "CModManager.h"
#define DECLARE_PROFILER_SECTION_CModManager
#include "profiler/SharedUtil.Profiler.h"

#ifdef MTA_DEBUG
    #define CLIENT_DLL_NAME "client_d.dll"
#else
    #define CLIENT_DLL_NAME "client.dll"
#endif

template <>
CModManager* CSingleton<CModManager>::m_pSingleton = NULL;

namespace
{
    // Callbacks can queue another session, but must finish using the current
    // client before Core unloads its module or resumes processing messages.
    class ModTransition
    {
    public:
        explicit ModTransition(bool& active) : m_active(active)
        {
            dassert(!m_active);
            m_active = true;
            CMessageLoopHook::GetSingleton().SetRefreshMsgQueueEnabled(false);
        }
        ~ModTransition()
        {
            m_active = false;
            CMessageLoopHook::GetSingleton().SetRefreshMsgQueueEnabled(true);
        }

    private:
        bool& m_active;
    };
}

void CModManager::RequestLoad(const char* arguments)
{
    m_state = State::PendingStart;
    m_arguments = arguments ? arguments : "";
}

void CModManager::RequestUnload()
{
    if (!IsLoaded())
    {
        m_state = State::Idle;
        return;
    }

    const bool notify = m_state == State::Idle && !m_inTransition;
    m_state = State::PendingStop;
    if (notify)
        CCore::GetSingletonPtr()->OnModUnload();
}

bool CModManager::TriggerCommand(const char* commandName, size_t commandNameLength, const void* userdata, size_t userdataSize) const
{
    if (m_client == nullptr || commandName == nullptr || commandNameLength == 0)
        return false;

    return m_client->ProcessCommand(commandName, commandNameLength, userdata, userdataSize);
}

void CModManager::DoPulsePreFrame()
{
    TIMING_GRAPH("+DoPulsePreFrame");
    CCore::GetSingleton().GetFPSLimiter()->OnFrameStart();  // Prepare FPS limiting for this frame

    if (m_client)
    {
        m_client->PreFrameExecutionHandler();
    }
    TIMING_GRAPH("-DoPulsePreFrame");
}

void CModManager::DoPulsePreHUDRender(bool bDidUnminimize, bool bDidRecreateRenderTargets)
{
    TIMING_GRAPH("+DoPulsePreHUDRender");
    if (m_client)
    {
        m_client->PreHUDRenderExecutionHandler(bDidUnminimize, bDidRecreateRenderTargets);
    }
    TIMING_GRAPH("-DoPulsePreHUDRender");
}

void CModManager::DoPulsePostFrame()
{
    auto handleStateChange = [&]()
    {
        if (m_state == State::PendingStart)
            Start();
        else if (m_state == State::PendingStop)
            Stop();
    };

    TIMING_GRAPH("+DoPulsePostFrame");

    handleStateChange();  // Handle state changes before pulse

    if (m_client)
    {
        m_client->PostFrameExecutionHandler();
    }
    else
    {
        CCore::GetSingleton().GetNetwork()->DoPulse();
    }

    CCore::GetSingleton().DoReliablePulse();  // Do reliable pulse

    handleStateChange();  // Handle state changes after pulse

    TIMING_GRAPH("-DoPulsePostFrame");
    TIMING_GRAPH("");
}

bool CModManager::Load(const char* arguments)
{
    if (m_inTransition)
        return false;
    RequestLoad(arguments);
    return Start();
}

void CModManager::Unload()
{
    if (m_inTransition || !IsLoaded())
    {
        RequestUnload();
        return;
    }
    m_state = State::PendingStop;
    Stop();
}

bool CModManager::Start()
{
    dassert(m_state == State::PendingStart);
    if (IsLoaded())
    {
        Stop();
        if (m_state != State::PendingStart || IsLoaded())
        {
            m_state = State::Idle;
            return false;
        }
    }

    // Consume this request before entering the client. Its argument buffer
    // must stay valid even if ClientInitialize queues a different session.
    std::string arguments;
    arguments.swap(m_arguments);
    m_state = State::Idle;
    bool success;
    {
        ModTransition transition(m_inTransition);
        success = TryStart(arguments);
    }

    // Deathmatch can request a disconnect from ClientInitialize and still
    // return success. Retire it after its initializer has returned, before
    // the caller can pulse the client or deliver another callback to it.
    if (success && m_state != State::Idle)
        Stop();
    else if (!IsLoaded() && m_state == State::PendingStop)
        m_state = State::Idle;
    return success;
}

bool CModManager::TryStart(const std::string& arguments)
{
    // Ensure DllDirectory has not been changed
    SString dllSearchPath = GetSystemDllDirectory();

    if (CalcMTASAPath("mta").CompareI(dllSearchPath) == false)
    {
        AddReportLog(3119, SString("DllDirectory wrong:  DllDirectory:'%s'  Path:'%s'", *dllSearchPath, *CalcMTASAPath("mta")));
        SetDllDirectory(CalcMTASAPath("mta"));
    }

    // The DE client is built beside the classic module, so select its explicit
    // directory before architecture admission instead of replacing the x86 DLL.
#if defined(MTA_WITH_DEFINITIVE_ADAPTER) && defined(_WIN64)
    const SString modulePath = CalcMTASAPath("mods\\deathmatch\\x64\\" CLIENT_DLL_NAME);
#else
    const SString modulePath = CalcMTASAPath("mods\\deathmatch\\" CLIENT_DLL_NAME);
#endif

    // Reuse Core's architecture admission and exact module ownership. Client
    // startup must not execute the x86 deathmatch DLL inside a DE x64 process.
    if (!m_module.LoadModule(modulePath))
    {
        CCore::GetSingleton().GetConsole()->Printf("Unable to load deathmatch's DLL (reason: %s)", *m_module.GetLastErrorMessage());
        return false;
    }

    using InitClientFn = CClientBase*(__cdecl*)();
    const auto initClient = reinterpret_cast<InitClientFn>(m_module.GetFunctionPointer("InitClient"));
    if (!initClient)
    {
        CCore::GetSingleton().GetConsole()->Printf("Unable to initialize deathmatch's DLL (missing init)");
        m_module.UnloadModule();
        return false;
    }

    CClientBase* client = initClient();

    if (client == nullptr || client->ClientInitialize(arguments.c_str(), CCore::GetSingletonPtr()) != 0)
    {
        CCore::GetSingleton().GetConsole()->Printf("Unable to initialize deathmatch's DLL (unable to init, bad version?)");
        m_module.UnloadModule();
        return false;
    }

    m_client = client;

    // A successful initialization still needs ClientShutdown, but a client
    // that already requested retirement must not take over console/chat input.
    if (m_state != State::Idle)
        return true;

    // HACK: make the console input active if its visible
    if (CLocalGUI::GetSingleton().IsConsoleVisible())
        CLocalGUI::GetSingleton().GetConsole()->ActivateInput();

    // Tell chat to start handling input
    CLocalGUI::GetSingleton().GetChat()->OnModLoad();

    return true;
}

void CModManager::Stop()
{
    dassert(m_state != State::Idle);
    dassert(m_module.IsOk());
    // A previous unload refusal can retain the DLL after ClientShutdown.
    // Retrying retirement must not require calling the client a second time.

    {
        ModTransition transition(m_inTransition);
        TryStop();
    }

    if (m_state == State::PendingStop)
        m_state = State::Idle;
}

void CModManager::TryStop()
{
    if (!m_client)
    {
        // A failed release keeps the DLL loaded after the session has already
        // shut down. Retrying the loader must not reset Core's services again.
        m_module.UnloadModule();
        return;
    }
    m_client->ClientShutdown();
    m_client = {};

    // Unregister the commands it had registered
    CCore::GetSingleton().GetCommands()->DeleteAll();

    // Stop all screen grabs
    CGraphics::GetSingleton().GetScreenGrabber()->ClearScreenShotQueue();

    // Free the Client DLL
    m_module.UnloadModule();

    // Call the on mod unload func
    CCore::GetSingletonPtr()->OnModUnload();

    // Reset chatbox status (so it won't prevent further input), and clear it
    /*CLocalGUI::GetSingleton ().GetChatBox ()->SetInputEnabled ( false );
    CLocalGUI::GetSingleton ().GetChatBox ()->Clear ();*/
    CLocalGUI::GetSingleton().GetChat()->SetInputVisible(false);
    CLocalGUI::GetSingleton().GetChat()->Clear();
    CLocalGUI::GetSingleton().SetChatBoxVisible(true);

    // Reset the debugview status
    CLocalGUI::GetSingleton().GetDebugView()->SetVisible(false, true);
    CLocalGUI::GetSingleton().GetDebugView()->Clear();
    CLocalGUI::GetSingleton().SetDebugViewVisible(false);

    // NULL the message processor and the unhandled command handler
    CCore::GetSingleton().SetClientMessageProcessor(NULL);
    CCore::GetSingleton().GetCommands()->SetExecuteHandler(NULL);

    // Reset cursor alpha
    CCore::GetSingleton().GetGUI()->SetCursorAlpha(1.0f, true);

    // Reset the modules
    CCore::GetSingleton().GetGame()->Reset();
    CCore::GetSingleton().GetMultiplayer()->Reset();
    CCore::GetSingleton().GetNetwork()->Reset();
    assert(CCore::GetSingleton().GetNetwork()->GetServerBitStreamVersion() == 0);

    // Enable the console again
    CCore::GetSingleton().GetConsole()->SetEnabled(true);

    // Force the mainmenu back
    CCore::GetSingleton().SetConnected(false);
    CLocalGUI::GetSingleton().GetMainMenu()->SetIsIngame(false);
    CLocalGUI::GetSingleton().GetMainMenu()->SetVisible(true, false);
}

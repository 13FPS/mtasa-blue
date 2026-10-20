/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CCore.h
 *  PURPOSE:     Header file for base core class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

class CCore;
class CDiscordRichPresence;
class CDiscordInterface;
class CSettings;

#pragma once

#include "version.h"

#include "CClientVariables.h"
#include "CCommands.h"
#include "CModuleLoader.h"
#ifdef MTA_WITH_DEFINITIVE_ADAPTER
    #include "CGameAdapterModule.h"
#endif
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include "DefinitiveFrontend.h"
    #include "DefinitiveFrontendConsole.h"
    #include "DefinitiveFrontendBindings.h"
    #include "DefinitiveFrontendStartup.h"
    #include "DefinitiveFrontendRuntime.h"
    #include "DefinitiveLocalInputRuntime.h"
    #include "DefinitivePersistentFrontend.h"
    #include "DefinitiveGuiFrontendInputReference.h"
#endif
#include "CSingleton.h"
#include "CGUI.h"
#include "CConnectManager.h"
#include <DXHook/CDirect3DHookManager.h>
#include <DXHook/CDirectInputHookManager.h>
#include <Graphics/CGraphics.h>
#include "CSetCursorPosHook.h"
#include "CMessageLoopHook.h"
#include "CConsoleLogger.h"
#include "CModManager.h"
#include <core/CClientEntityBase.h>
#include <core/CCoreInterface.h>
#include <DXHook/CDirect3DData.h>
#include "CClientVariables.h"
#include "CKeyBinds.h"
#include "CMouseControl.h"
#include "CScreenShot.h"
#include <xml/CXML.h>
#include <ijsify.h>
#include <core/CWebCoreInterface.h>
#include "CTrayIcon.h"
#include "FPSLimiter.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#define BLUE_VERSION_STRING   "Multi Theft Auto v" MTA_DM_BUILDTAG_LONG
#define BLUE_COPYRIGHT_STRING "Copyright (C) 2003 - %BUILD_YEAR% Multi Theft Auto"

// Configuration file path (relative to MTA install directory)
#define MTA_CONFIG_PATH            "mta/config/coreconfig.xml"
#define MTA_SERVER_CACHE_PATH      "mta/config/servercache.xml"
#define MTA_CONSOLE_LOG_PATH       "mta/logs/console.log"
#define MTA_CONSOLE_INPUT_LOG_PATH "mta/logs/console-input.log"
#define CONFIG_ROOT                "mainconfig"
#define CONFIG_NODE_CVARS          "settings"  // cvars node
#define CONFIG_NODE_KEYBINDS       "binds"     // keybinds node
#define CONFIG_NODE_JOYPAD         "joypad"
#define CONFIG_NODE_UPDATER        "updater"
#define CONFIG_NODE_SERVER_INT     "internet_servers"         // backup of last successful master server list query
#define CONFIG_NODE_SERVER_FAV     "favourite_servers"        // favourite servers list node
#define CONFIG_NODE_SERVER_REC     "recently_played_servers"  // recently played servers list node
#define CONFIG_NODE_SERVER_OPTIONS "serverbrowser_options"    // saved options for the server browser
#define CONFIG_NODE_SERVER_SAVED   "server_passwords"         // This contains saved passwords (as appose to save_server_passwords which is a setting)
#define CONFIG_NODE_SERVER_HISTORY "connect_history"
#define CONFIG_INTERNET_LIST_TAG   "internet_server"
#define CONFIG_FAVOURITE_LIST_TAG  "favourite_server"
#define CONFIG_RECENT_LIST_TAG     "recently_played_server"
#define CONFIG_HISTORY_LIST_TAG    "connected_server"
#define IDT_TIMER1                 1234

class CSteamClient;

extern class CCore*         g_pCore;
extern class CGraphics*     g_pGraphics;
extern class CLocalization* g_pLocalization;
bool                        UsingAltD3DSetup();
extern SString              g_strJingleBells;

class CCore : public CCoreInterface, public CSingleton<CCore>
{
public:
    ZERO_ON_NEW
    CCore();
    ~CCore();

    // Subsystems (query)
    eCoreVersion                       GetVersion();
    CConsoleInterface*                 GetConsole();
    CCommandsInterface*                GetCommands();
    CConnectManager*                   GetConnectManager() { return m_pConnectManager; };
    CGame*                             GetGame();
    CGUI*                              GetGUI();
    CGraphicsInterface*                GetGraphics();
    CModManagerInterface*              GetModManager();
    CMultiplayer*                      GetMultiplayer();
    CNet*                              GetNetwork();
    CXML*                              GetXML() { return m_pXML; };
    CXMLNode*                          GetConfig();
    CClientVariables*                  GetCVars() { return &m_ClientVariables; };
    CKeyBindsInterface*                GetKeyBinds();
    CMouseControl*                     GetMouseControl() { return m_pMouseControl; };
    CLocalGUI*                         GetLocalGUI();
    CLocalizationInterface*            GetLocalization() { return g_pLocalization; };
    CWebCoreInterface*                 GetWebCore();
    CWebCoreInterface*                 GetWebCoreUnchecked() { return m_pWebCore; }  // For cleanup in destructors only - bypasses initialization check
    CTrayIconInterface*                GetTrayIcon() { return m_pTrayIcon; };
    std::shared_ptr<CDiscordInterface> GetDiscord();
    CSteamClient*                      GetSteamClient() { return m_steamClient.get(); }
    FPSLimiter::FPSLimiterInterface*   GetFPSLimiter() const noexcept { return m_pFPSLimiter.get(); }

    void SaveConfig(bool bWaitUntilFinished = false);

    // Debug
    void DebugEcho(const char* szText);
    void DebugPrintf(const char* szFormat, ...);
    void SetDebugVisible(bool bVisible);
    bool IsDebugVisible();
    void DebugEchoColor(const char* szText, unsigned char R, unsigned char G, unsigned char B);
    void DebugPrintfColor(const char* szFormat, unsigned char R, unsigned char G, unsigned char B, ...);
    void DebugClear();

    // Chat
    void ChatEcho(const char* szText, bool bColorCoded = false);
    void ChatEchoColor(const char* szText, unsigned char R, unsigned char G, unsigned char B, bool bColorCoded = false);
    void ChatPrintf(const char* szFormat, bool bColorCoded, ...);
    void ChatPrintfColor(const char* szFormat, bool bColorCoded, unsigned char R, unsigned char G, unsigned char B, ...);
    void SetChatVisible(bool bVisible, bool bInputBlocked);
    bool IsChatVisible();
    bool IsChatInputBlocked();
    void EnableChatInput(char* szCommand, DWORD dwColor);
    bool IsChatInputEnabled();
    bool ClearChat();
    void OnGameTimerUpdate();
    bool SetChatboxCharacterLimit(int charLimit);
    void ResetChatboxCharacterLimit();
    int  GetChatboxCharacterLimit();
    int  GetChatboxMaxCharacterLimit();

    // Screenshot
    void InitiateScreenShot(bool bIsCameraShot);

    // GUI
    bool IsSettingsVisible();
    bool IsMenuVisible();
    bool IsCursorForcedVisible();
    bool IsCursorControlsToggled() { return m_bCursorToggleControls; }
    void HideMainMenu();
    void SetCenterCursor(bool bEnabled);

    void ShowServerInfo(unsigned int WindowType);

    // Configuration
    void ApplyConsoleSettings();
    void ApplyGameSettings();
    void UpdateRecentlyPlayed();

    // Net
    void SetConnected(bool bConnected);
    bool IsConnected();
    bool Reconnect(const char* szHost, unsigned short usPort, const char* szPassword, bool bSave = true);

    // Mod
    void        SetOfflineMod(bool bOffline);
    void        ForceCursorVisible(bool bVisible, bool bToggleControls = true);
    void        SetMessageProcessor(pfnProcessMessage pfnMessageProcessor);
    void        ShowMessageBox(const char* szTitle, const char* szText, unsigned int uiFlags, GUI_CALLBACK* ResponseHandler = NULL);
    void        RemoveMessageBox(bool bNextFrame = false);
    void        ShowErrorMessageBox(const SString& strTitle, SString strMessage, const SString& strTroubleLink = "");
    void        ShowNetErrorMessageBox(const SString& strTitle, SString strMessage, SString strTroubleLink = "", bool bLinkRequiresErrorCode = false);
    static void ErrorMessageBoxCallBack(void* pData, uint uiButton);
    bool        IsOfflineMod() { return m_bIsOfflineMod; }
    const char* GetModInstallRoot(const char* szModName);
    bool        CheckDiskSpace(uint uiResourcesPathMinMB = 10, uint uiDataPathMinMB = 10);

    // Subsystems
    void CreateStartupModules();
    void CreateGame();
    void CreateMultiplayer();
    void CreateNetwork();
    void CreateXML();
    void InitGUI(IDirect3DDevice9* pDevice);
    void CreateGUI();
    void DestroyGame();
    void DestroyMultiplayer();
    void DestroyNetwork();
    void DestroyXML();
    void DeinitGUI();
    void DestroyGUI();

    // Web
    bool IsWebCoreLoaded() { return m_pWebCore != nullptr && m_pWebCore->IsInitialised(); }
    void DestroyWeb();

    // Hooks
    void              ApplyHooks();
    void              ApplyHooks2();
    void              ApplyHooks3(bool bEnable);
    HWND              GetHookedWindow();
    void              SwitchRenderWindow(HWND hWnd, HWND hWndInput);
    void              CallSetCursorPos(int X, int Y);
    void              SetClientMessageProcessor(pfnProcessMessage pfnMessageProcessor) { m_pfnMessageProcessor = pfnMessageProcessor; };
    pfnProcessMessage GetClientMessageProcessor() { return m_pfnMessageProcessor; }
    void              ChangeResolution(long width, long height, long depth);

    bool IsFocused() { return (GetForegroundWindow() == GetHookedWindow()); };
    bool IsWindowMinimized();
    void UpdateIsWindowMinimized();

    // Pulse
    void DoPreFramePulse();
    void DoPostFramePulse();

    // Events
    void OnModUnload();

    // Misc
    void RegisterCommands();
    bool IsValidNick(const char* szNick);  // Move somewhere else
    void Quit(bool bInstantly = true);
    void InitiateUpdate(const char* szType, const char* szData, const char* szHost) { m_pLocalGUI->InitiateUpdate(szType, szData, szHost); }
    bool IsOptionalUpdateInfoRequired(const char* szHost) { return m_pLocalGUI->IsOptionalUpdateInfoRequired(szHost); }
    void InitiateDataFilesFix() { m_pLocalGUI->InitiateDataFilesFix(); }

    // FPS Limiter
    void OnFPSLimitChange(std::uint16_t fps);

    void DoReliablePulse();

    bool IsTimingCheckpoints();
    void OnTimingCheckpoint(const char* szTag);
    void OnTimingDetail(const char* szTag);

    void CalculateStreamingMemoryRange();
    uint GetMinStreamingMemory();
    uint GetMaxStreamingMemory();

    SString GetConnectCommandFromURI(const char* szURI);
    void    GetConnectParametersFromURI(const char* szURI, std::string& strHost, unsigned short& usPort, std::string& strNick, std::string& strPassword);
    std::map<std::string, std::string>& GetCommandLineOptions() { return m_CommandLineOptions; }
    const char*                         GetCommandLineOption(const char* szOption);
    const char*                         GetCommandLineArgs() { return m_szCommandLineArgs; }
    void                                RequestNewNickOnStart() { m_requestNewNickname = true; }
    bool                                WillRequestNewNickOnStart() { return m_requestNewNickname; }
    bool                                WasLaunchedWithConnectURI();
    void                                HandleCrashDumpEncryption();

    void                 OnPreFxRender();
    void                 OnPreHUDRender();
    void                 OnDeviceRestore();
    void                 OnCrashAverted(uint uiId);
    void                 OnEnterCrashZone(uint uiId);
    void                 UpdateWerCrashModuleBases();
    void                 LogEvent(uint uiDebugId, const char* szType, const char* szContext, const char* szBody, uint uiAddReportLogId = 0);
    bool                 GetDebugIdEnabled(uint uiDebugId);
    EDiagnosticDebugType GetDiagnosticDebug();
    void                 SetDiagnosticDebug(EDiagnosticDebugType value);
    CModelCacheManager*  GetModelCacheManager();

    static void StaticIdleHandler();
    void        IdleHandler();
    void        WindowsTimerHandler();
    void        HandleIdlePulse();
    void        SetModulesLoaded(bool bLoaded);
    bool        AreModulesLoaded();
    void        UpdateDummyProgress(int iValue = -1, const char* szType = "");
    void        SetDummyProgressUpdateAlways(bool bAlways) { m_bDummyProgressUpdateAlways = bAlways; }
    bool        GetDummyProgressUpdateAlways() { return m_bDummyProgressUpdateAlways; }

    void        OnPreCreateDevice(IDirect3D9* pDirect3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD& BehaviorFlags,
                                  D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT     OnPostCreateDevice(HRESULT hResult, IDirect3D9* pDirect3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
                                   D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
    bool        GetDeviceSelectionEnabled();
    bool        GetRequiredDisplayResolution(int& iOutWidth, int& iOutHeight, int& iOutColorBits, int& iOutAdapterIndex, bool& bOutAllowUnsafeResolutions);
    void        NotifyRenderingGrass(bool bIsRenderingGrass);
    bool        IsRenderingGrass() { return m_bIsRenderingGrass; }
    bool        GetRightSizeTxdEnabled();
    const char* GetProductRegistryPath() { return SharedUtil::GetProductRegistryPath(); }
    const char* GetProductCommonDataDir() { return SharedUtil::GetProductCommonDataDir(); }
    const char* GetProductVersion() { return SharedUtil::GetProductVersion(); }
    void        SetFakeLagCommandEnabled(bool bEnabled) { m_bFakeLagCommandEnabled = bEnabled; }
    bool        IsFakeLagCommandEnabled() { return m_bFakeLagCommandEnabled; }
    SString     GetBlueCopyrightString();

    bool IsNetworkReady() const noexcept { return m_isNetworkReady; }
    bool CanHandleKeyMessages() const noexcept { return m_menuFrame > 1; }

    void   SetCustomStreamingMemory(size_t szMB);
    bool   IsUsingCustomStreamingMemorySize();
    size_t GetStreamingMemory();

    const SString& GetLastConnectedServerName() const { return m_strLastConnectedServerName; }
    void           SetLastConnectedServerName(const SString& strServerName) { m_strLastConnectedServerName = strServerName; }

    void SetCurrentRefreshRate(uint uiRefreshRate) { m_uiCurrentRefreshRate = uiRefreshRate; }
    uint GetCurrentRefreshRate() const { return m_uiCurrentRefreshRate; }

    void OnPostColorFilterRender() override;

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    // Settings queries the retained owner dynamically; diagnostic/session-only
    // views must never acquire persistence merely by constructing the same UI.
    bool IsFrontendBindingPersistenceEnabled() const noexcept;
    bool ApplyFrontendBindingEdits(CSettings& settings) noexcept;
#endif

private:
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    friend struct DefinitiveFrontend::Controller;
    friend struct DefinitiveFrontendStartup::Controller;
    struct FrontendShellTag
    {
    };
    explicit CCore(FrontendShellTag);
    struct FrontendStartupTag
    {
    };
    explicit CCore(FrontendStartupTag);
    DefinitiveFrontend::Status RunFrontendStartupView(const DefinitiveInputReference::Request&, DefinitiveGuiFrontendInputReference::Snapshot&);
    DefinitiveFrontend::Status RunPersistentFrontend(const DefinitivePersistentFrontend::RequestV1&, DefinitivePersistentFrontend::Snapshot&);
    struct FrontendResources;
    struct FrontendRuntimeResources;
    struct LocalInputRuntimeResources;
    FrontendResources*                  m_frontendResources = nullptr;
    FrontendRuntimeResources*           m_frontendRuntimeResources = nullptr;
    LocalInputRuntimeResources*         m_localInputRuntimeResources = nullptr;
    bool                                m_frontendShellOnly = false;
    bool                                m_frontendStartupOwned = false;
    bool                                m_frontendStartupAccepting = false;
    DWORD                               m_frontendCoreOwnerThread = GetCurrentThreadId();
    bool                                StartFrontend(CGUI* borrowedGui);
    DefinitiveFrontend::Status          PulseFrontend(std::int32_t mouseX, std::int32_t mouseY, bool focused);
    DefinitiveFrontend::Status          StopFrontend();
    DefinitiveFrontend::Snapshot        ReadFrontend() const;
    static void                         FrontendInputOwnership(void*, bool) noexcept;
    static void                         FrontendConsoleInputOwnership(void*, bool) noexcept;
    static void                         FrontendCloseRequested(void*) noexcept;
    DefinitiveFrontend::Status          EnableFrontendConsole();
    DefinitiveFrontendConsole::Snapshot ReadFrontendConsole();
    DefinitiveFrontend::Status          SetFrontendConsoleVisible(bool visible);
    DefinitiveFrontend::Status          SetFrontendMenuVisible(bool visible);
    DefinitiveFrontend::Status ProcessFrontendConsoleMessage(std::uint32_t message, std::uintptr_t wparam, std::intptr_t lparam, std::uint32_t& handled);
    DefinitiveFrontend::Status ReadFrontendConsoleText(std::uint32_t field, char* output, std::uint32_t capacity, std::uint32_t& required);
    DefinitiveFrontend::Status EnableFrontendBindings();
    DefinitiveFrontend::Status EnableFrontendBindingPersistence(const wchar_t* path, std::uint32_t chars);
    DefinitiveFrontend::Status EnableFrontendRuntimeBindingPersistence(const wchar_t* path, std::uint32_t chars);
    DefinitiveFrontend::Status LoadFrontendBindingPersistence(const wchar_t* path, std::uint32_t chars);
    DefinitiveFrontendBindings::Snapshot  ReadFrontendBindings();
    DefinitiveFrontend::Status            ProduceFrontendNeutralInput(mta::game_de::MtaInputState& output, bool cleanup = false);
    DefinitiveFrontend::Status            EnableFrontendRuntimeServices() noexcept;
    DefinitiveFrontendRuntime::Snapshot   ReadFrontendRuntimeServices() const noexcept;
    DefinitiveFrontend::Status            StopFrontendRuntimeServices() noexcept;
    bool                                  FlushFrontendRuntimeBindings() noexcept;
    DefinitiveLocalInputRuntime::Status   ConfigureLocalInputRuntime(const DefinitiveLocalInputRuntime::RequestV1&) noexcept;
    DefinitiveLocalInputRuntime::Status   PulseLocalInputRuntime() noexcept;
    DefinitiveLocalInputRuntime::Snapshot ReadLocalInputRuntime() noexcept;
    DefinitiveLocalInputRuntime::Status   StopLocalInputRuntime(bool                                 disable,
                                                                DefinitiveLocalInputRuntime::Failure failure = DefinitiveLocalInputRuntime::Failure::None,
                                                                bool                                 fault = false) noexcept;
    bool                                  FlushLocalInputRuntime() noexcept;
    bool                                  LocalInputAllowsFrontend() const noexcept;
#endif
    void InitializeFrontendInfrastructure();
    void DestroyFrontendCommands();
    void DestroyFrontendConfiguration(bool saveConfig);
    void DestroyFrontendLogger();
    void ReleaseXML(bool saveConfig);
    void ApplyCoreInitSettings();

private:
    // Core devices.
    CXML*               m_pXML = nullptr;
    CLocalGUI*          m_pLocalGUI;
    CGraphics*          m_pGraphics;
    CCommands*          m_pCommands = nullptr;
    CDirect3DData*      m_pDirect3DData;
    CConnectManager*    m_pConnectManager;
    CModelCacheManager* m_pModelCacheManager;

    // Instances (put new classes here!)
    CXMLFile*                               m_pConfigFile = nullptr;
    CClientVariables                        m_ClientVariables;
    CWebCoreInterface*                      m_pWebCore = nullptr;
    CTrayIcon*                              m_pTrayIcon;
    std::unique_ptr<CSteamClient>           m_steamClient;
    std::shared_ptr<CDiscordRichPresence>   m_pDiscordRichPresence;
    std::unique_ptr<FPSLimiter::FPSLimiter> m_pFPSLimiter;

    // Hook interfaces.
    CMessageLoopHook*        m_pMessageLoopHook;
    CDirectInputHookManager* m_pDirectInputHookManager;
    CDirect3DHookManager*    m_pDirect3DHookManager;
    CSetCursorPosHook*       m_pSetCursorPosHook;

    bool m_bLastFocused;
    int  m_iUnminimizeFrameCounter;
    bool m_bDidRecreateRenderTargets;
    bool m_bIsWindowMinimized;
    uint m_uiNextRenderTargetRetryTime;

    // Module loader objects.
#ifdef MTA_WITH_DEFINITIVE_ADAPTER
    CGameAdapterModule m_DefinitiveAdapter;
#endif
    CModuleLoader m_GameModule;
    CModuleLoader m_MultiplayerModule;
    CModuleLoader m_NetModule;
    CModuleLoader m_XMLModule;
    CModuleLoader m_GUIModule;
    CModuleLoader m_WebCoreModule;

    // Mod manager
    CModManager* m_pModManager;

    // Classic factory ownership includes Terminate/delete; an adapter facade
    // remains owned by the adapter through its Shutdown/Release sequence.
    enum class GameInterfaceOwnership
    {
        None,
        ClassicModule,
        AdapterBorrowed
    };
    GameInterfaceOwnership m_GameInterfaceOwnership = GameInterfaceOwnership::None;

    // Module interfaces.
    CGame*        m_pGame = nullptr;
    CNet*         m_pNet;
    CMultiplayer* m_pMultiplayer;
    CGUI*         m_pGUI;

    // Logger utility interface.
    CConsoleLogger* m_pConsoleLogger = nullptr;

    CLocalization* m_pLocalization = nullptr;

    CKeyBinds*     m_pKeyBinds = nullptr;
    CMouseControl* m_pMouseControl;

    unsigned short    m_menuFrame{};
    bool              m_isNetworkReady{};
    bool              m_bCrashDumpEncryptionDone{};
    bool              m_bIsOfflineMod;
    bool              m_bCursorToggleControls;
    pfnProcessMessage m_pfnMessageProcessor;

    CGUIMessageBox* m_pMessageBox;

    // screen res
    DEVMODE m_Current;

    SString m_strModInstallRoot;

    bool m_bQuitOnPulse;
    bool m_bDestroyMessageBox;

    bool                 m_requestNewNickname{false};
    EDiagnosticDebugType m_DiagnosticDebug;

    // Below 2 are used for the UI only
    float m_fMinStreamingMemory{};
    float m_fMaxStreamingMemory{};

    // Custom streaming memory limit set by `engineStreamingSetMemorySize` - Reset on server connects (= set to 0), or by the scripter
    // `0` means "not set" [so the value should be ignored]
    size_t m_CustomStreamingMemoryLimitBytes{};

    bool    m_bGettingIdleCallsFromMultiplayer;
    bool    m_bWindowsTimerEnabled;
    bool    m_bModulesLoaded;
    int     m_iDummyProgressValue;
    HANDLE  m_DummyProgressTimerHandle;
    SString m_strDummyProgressType;
    bool    m_bDummyProgressUpdateAlways;
    bool    m_bIsRenderingGrass;
    bool    m_bFakeLagCommandEnabled;

    SString m_strLastConnectedServerName{};
    uint    m_uiCurrentRefreshRate{};

    // Command line
    static void                        ParseCommandLine(std::map<std::string, std::string>& options, const char*& szArgs, const char** pszNoValOptions = NULL);
    std::map<std::string, std::string> m_CommandLineOptions;  // e.g. "-o option" -> {"o" = "option"}
    const char*                        m_szCommandLineArgs;   // Everything that comes after the options

    long long m_timeDiscordAppLastUpdate;
};

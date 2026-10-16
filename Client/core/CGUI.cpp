/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CGUI.cpp
 *  PURPOSE:     Core graphical user interface container class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CNewsBrowser.h"
#include <game/CGame.h>
#include <windowsx.h>
#include <memory>
#include <stdexcept>

using std::string;

template <>
CLocalGUI* CSingleton<CLocalGUI>::m_pSingleton = NULL;

#ifndef HIWORD
    #define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#endif
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))

const char* const DEFAULT_SKIN_NAME = "Default 2023";  // TODO: Change to whatever the default skin is if it changes

CLocalGUI::CLocalGUI(const CMainMenu::FrontendHost* frontendHost)
{
    m_frontendOnly = frontendHost != nullptr;
    if (frontendHost)
        m_frontendHost = *frontendHost;
    m_pConsole = NULL;
    m_pMainMenu = NULL;
    m_pChat = NULL;
    m_pDebugView = NULL;
    m_pLabelVersionTag = nullptr;

    m_bForceCursorVisible = false;
    m_bChatboxVisible = true;
    m_bChatboxInputBlocked = false;
    m_pDebugViewVisible = false;
    m_bGUIHasInput = false;
    m_uiActiveCompositionSize = 0;

    m_pVersionUpdater = GetVersionUpdater();

    m_LastSettingsRevision = -1;
    m_LocaleChangeCounter = 0;
    m_bHasQueuedLocaleChange = false;
    m_bPendingRestartPrompt = false;
}

CLocalGUI::~CLocalGUI()
{
    // Destroy all GUI elements
    DestroyObjects();
    // This is needed after the local gui is deleted for config saving
    // delete m_pVersionUpdater;
}

void CLocalGUI::SetSkin(const char* szName)
{
    // CC51's pump runs pulse code on half-destroyed windows, so re-entrant
    // calls are skipped. The guard clears on throw.
    static bool s_bInSetSkin = false;
    if (s_bInSetSkin)
        return;
    struct SetSkinGuard
    {
        bool& flag;
        explicit SetSkinGuard(bool& inFlag) : flag(inFlag) { flag = true; }
        ~SetSkinGuard() { flag = false; }
    } guard(s_bInSetSkin);

    // A fatal fault dialog is pumping messages; dont rebuild windows from inside it.
    if (CLocalGUI::IsFaultDialogOpen())
        return;

    CVector2D consolePos, consoleSize;

    bool guiWasLoaded = m_pMainMenu != NULL;
    if (guiWasLoaded)
    {
        consolePos = m_pConsole->GetPosition();
        consoleSize = m_pConsole->GetSize();
        DestroyWindows();
    }

    std::string error;

    CGUI* pGUI = CCore::GetSingleton().GetGUI();

    try
    {
        pGUI->SetSkin(szName);
        m_LastSkinName = szName;
    }
    catch (...)
    {
        try
        {
            pGUI->SetSkin(DEFAULT_SKIN_NAME);

            error = "The skin '" + std::string(szName) + "' that you have selected could not be loaded. MTA is now using the default skin instead.";

            CVARS_SET("current_skin", std::string(DEFAULT_SKIN_NAME));
            m_LastSkinName = DEFAULT_SKIN_NAME;
        }
        catch (...)
        {
            // Mark both layers' dialog flags first, so a nested fault exits instead of stacking.
            CLocalGUI::SetFaultDialogOpen(true);
            pGUI->SetFatalFaultDialogOpen(true);
            MessageBoxUTF8(0, _("The skin you selected could not be loaded, and the default skin also could not be loaded, please reinstall MTA."),
                           _("Error") + _E("CC51"), MB_OK | MB_ICONERROR | MB_TOPMOST);
            TerminateProcess(GetCurrentProcess(), 9);
        }
    }

    CClientVariables* cvars = CCore::GetSingleton().GetCVars();
    m_LastSettingsRevision = cvars->GetRevision();

    if (guiWasLoaded)
    {
        CreateWindows(guiWasLoaded);
        m_pConsole->SetPosition(consolePos);
        m_pConsole->SetSize(consoleSize);
        // QuestionBox was destroyed with MainMenu; re-show if settings still need a restart
        TryShowRestartPrompt();
    }

    if (CCore::GetSingleton().GetConsole() && !error.empty())
        CCore::GetSingleton().GetConsole()->Echo(error.c_str());
}

void CLocalGUI::ChangeLocale(const char* szName)
{
    // Windows are gone mid-rebuild, or a fatal dialog is pumping messages.
    if (!m_pConsole) [[unlikely]]
        return;

    if (CLocalGUI::IsFaultDialogOpen())
        return;

    bool guiWasLoaded = m_pMainMenu != NULL;
    assert(guiWasLoaded);

    CVector2D consolePos = m_pConsole->GetPosition();
    CVector2D consoleSize = m_pConsole->GetSize();

    if (guiWasLoaded)
        DestroyWindows();

    CClientVariables* cvars = CCore::GetSingleton().GetCVars();
    m_LastSettingsRevision = cvars->GetRevision();

    g_pLocalization->SetCurrentLanguage(szName ? szName : "");

    SString strCanonicalLocale = g_pLocalization->GetLanguageCode();
    if (strCanonicalLocale.empty())
        strCanonicalLocale = CVARS_GET_VALUE<SString>("locale");
    if (strCanonicalLocale.empty() && szName)
        strCanonicalLocale = szName;

    m_LastLocaleName = strCanonicalLocale;

    // Attempt CEGUI cleanup
    if (CGUI* pGUI = CCore::GetSingleton().GetGUI())
        pGUI->Cleanup();

    if (guiWasLoaded)
    {
        CreateWindows(guiWasLoaded);
        m_pConsole->SetPosition(consolePos);
        m_pConsole->SetSize(consoleSize);
        // QuestionBox was destroyed with MainMenu; re-show if settings still need a restart
        TryShowRestartPrompt();
    }
}

void CLocalGUI::CreateWindows(bool bGameIsAlreadyLoaded)
{
    CGUI* pGUI = CCore::GetSingleton().GetGUI();

    if (!m_frontendOnly)
    {
        // Chat/debug fonts and console input still require the classic renderer
        // and multiplayer. A frontend owns the actual menu without those views.
        m_pChat = new CChat(pGUI, CVector2D(0.0125f, 0.015f));
        m_pChat->SetVisible(false, true);
        m_pDebugView = new CDebugView(pGUI, CVector2D(0.23f, 0.785f));
        m_pDebugView->SetVisible(false, true);
    }

    // Create the overlayed version labels
    CVector2D ScreenSize = pGUI->GetResolution();
    SString   strText = "MTA:SA " MTA_DM_BUILDTAG_SHORT;
    if (m_frontendOnly)
        strText += " / EXPERIMENTAL";
    if (_NETCODE_VERSION_BRANCH_ID != 0x04)
        strText += SString(" (%X)", _NETCODE_VERSION_BRANCH_ID);
    m_pLabelVersionTag = reinterpret_cast<CGUILabel*>(pGUI->CreateLabel(strText));
    m_pLabelVersionTag->SetSize(CVector2D(m_pLabelVersionTag->GetTextExtent() + 5, 18));
    m_pLabelVersionTag->SetPosition(CVector2D(ScreenSize.fX - m_pLabelVersionTag->GetTextExtent() - 5, ScreenSize.fY - 15));
    m_pLabelVersionTag->SetAlpha(0.5f);
    m_pLabelVersionTag->SetTextColor(255, 255, 255);
    m_pLabelVersionTag->SetZOrderingEnabled(false);
    m_pLabelVersionTag->MoveToBack();
    if (MTASA_VERSION_TYPE < VERSION_TYPE_UNTESTED)
        m_pLabelVersionTag->SetAlwaysOnTop(true);

    // Create mainmenu
    m_pMainMenu = new CMainMenu(pGUI, m_frontendOnly ? CMainMenu::SettingsCreation::Deferred : CMainMenu::SettingsCreation::Eager,
                                m_frontendOnly ? &m_frontendHost : nullptr);
    m_pMainMenu->SetVisible(m_frontendOnly || bGameIsAlreadyLoaded, m_frontendOnly || !bGameIsAlreadyLoaded, false);

    // Create console
    if (!m_frontendOnly)
    {
        m_pConsole = new CConsole(pGUI);
        m_pConsole->SetVisible(false);
    }

    // Create our news headlines if we're already ingame
    if (bGameIsAlreadyLoaded && !m_frontendOnly)
        m_pMainMenu->GetNewsBrowser()->CreateHeadlines();
}

void CLocalGUI::CreateObjects(IUnknown* pDevice)
{
    // Store the GUI manager pointer and create the GUI classes
    CGUI* pGUI = CCore::GetSingleton().GetGUI();

    // Set the CEGUI skin to whatever the user has selected
    SString           currentSkinName;
    CClientVariables* cvars = CCore::GetSingleton().GetCVars();
    cvars->Get("current_skin", currentSkinName);
    if (currentSkinName.empty())
    {
        currentSkinName = DEFAULT_SKIN_NAME;
        CVARS_SET("current_skin", currentSkinName);
    }

    SetSkin(currentSkinName);

    CreateWindows(false);
}

void CLocalGUI::DestroyWindows()
{
    SAFE_DELETE(m_pLabelVersionTag);
    SAFE_DELETE(m_pConsole);
    SAFE_DELETE(m_pMainMenu);
    SAFE_DELETE(m_pChat);
    SAFE_DELETE(m_pDebugView);
}

void CLocalGUI::DestroyObjects()
{
    DestroyWindows();

    // Destroy and NULL all elements
    SAFE_DELETE(m_pLabelVersionTag);
}

void CLocalGUI::RequestLocaleChange(const SString& strLocale)
{
    if (strLocale.empty())
        return;

    if (m_bHasQueuedLocaleChange && strLocale == m_QueuedLocaleChange)
        return;

    const SString strCurrentLocale = CVARS_GET_VALUE<SString>("locale");
    if (!m_bHasQueuedLocaleChange && strLocale == strCurrentLocale && strLocale == m_LastLocaleName)
        return;

    if (m_LocaleChangeCounter > 0)
        CCore::GetSingleton().RemoveMessageBox();

    m_QueuedLocaleChange = strLocale;
    m_bHasQueuedLocaleChange = true;
    m_LocaleChangeCounter = 0;

    // Keep the cvar at the last applied locale so dependent systems remain stable while we rebuild UI
    if (!m_LastLocaleName.empty() && strCurrentLocale != m_LastLocaleName)
        CVARS_SET("locale", m_LastLocaleName);
}

void CLocalGUI::RequestRestartPrompt()
{
    m_bPendingRestartPrompt = true;
    TryShowRestartPrompt();
}

void CLocalGUI::TryShowRestartPrompt()
{
    if (!m_bPendingRestartPrompt || !m_pMainMenu)
        return;

    // Wait until locale/skin rebuilds finish so the prompt is not destroyed with MainMenu
    if (m_bHasQueuedLocaleChange)
        return;

    SString strCurrentSkinName;
    CVARS_GET("current_skin", strCurrentSkinName);
    if (!strCurrentSkinName.empty() && strCurrentSkinName != m_LastSkinName)
        return;

    CQuestionBox* pQuestionBox = m_pMainMenu->GetQuestionWindow();
    if (pQuestionBox->IsVisible())
        return;

    SString strMessage = _("Some settings will be changed when you next start MTA");
    strMessage += _("\n\nDo you want to restart now?");
    pQuestionBox->Reset();
    pQuestionBox->SetTitle(_("RESTART REQUIRED"));
    pQuestionBox->SetMessage(strMessage);
    pQuestionBox->SetButton(0, _("No"));
    pQuestionBox->SetButton(1, _("Yes"));
    pQuestionBox->SetCallback(RestartPromptCallBack);
    pQuestionBox->Show();
}

void CLocalGUI::RestartPromptCallBack(void* pData, unsigned int uiButton)
{
    CLocalGUI* pLocalGUI = CLocalGUI::GetSingletonPtr();
    if (!pLocalGUI)
        return;

    if (CMainMenu* pMainMenu = pLocalGUI->GetMainMenu())
        pMainMenu->GetQuestionWindow()->Reset();

    pLocalGUI->ClearRestartPrompt();

    if (uiButton == 1)
    {
        SetOnQuitCommand("restart");
        CCore::GetSingleton().Quit();
    }
}

void CLocalGUI::ApplyQueuedLocale()
{
    if (!m_bHasQueuedLocaleChange)
        return;

    // Console is gone mid-rebuild; keep the request queued for a later pulse.
    if (!m_pConsole)
        return;

    CClientVariables* cvars = CCore::GetSingleton().GetCVars();

    if (m_QueuedLocaleChange.empty())
    {
        m_bHasQueuedLocaleChange = false;
        m_LocaleChangeCounter = 0;
        CCore::GetSingleton().RemoveMessageBox();
        return;
    }

    if (CCore::GetSingleton().GetModManager()->IsLoaded())
    {
        if (CConsoleInterface* pConsole = CCore::GetSingleton().GetConsole())
            pConsole->Printf("Please disconnect before changing language");
        if (cvars)
            cvars->Set("locale", m_LastLocaleName);

        m_bHasQueuedLocaleChange = false;
        m_QueuedLocaleChange.clear();
        m_LocaleChangeCounter = 0;
        CCore::GetSingleton().RemoveMessageBox();
        return;
    }

    ChangeLocale(m_QueuedLocaleChange);

    m_bHasQueuedLocaleChange = false;
    SString strAppliedLocale = m_LastLocaleName;
    m_QueuedLocaleChange.clear();
    m_LocaleChangeCounter = 0;

    if (!strAppliedLocale.empty())
    {
        SString strCurrentLocale = CVARS_GET_VALUE<SString>("locale");
        if (strCurrentLocale != strAppliedLocale)
            CVARS_SET("locale", strAppliedLocale);
    }

    if (cvars)
        m_LastSettingsRevision = cvars->GetRevision();

    CCore::GetSingleton().RemoveMessageBox();
}

void CLocalGUI::DoPulse()
{
    // The frontend's owner supplies its frame/input context explicitly. Never
    // accidentally start updater or mod-manager work through a classic pulse.
    if (m_frontendOnly)
        return;
    // A fatal dialog is pumping messages; pulse work touches CEGUI and would throw.
    if (CLocalGUI::IsFaultDialogOpen())
        return;

    m_pVersionUpdater->DoPulse();

    CClientVariables* cvars = CCore::GetSingleton().GetCVars();
    if (cvars->GetRevision() != m_LastSettingsRevision)
    {
        m_LastSettingsRevision = cvars->GetRevision();

        //
        // Check for skin change
        //
        SString currentSkinName;
        cvars->Get("current_skin", currentSkinName);

        if (currentSkinName != m_LastSkinName)
        {
            if (!CCore::GetSingleton().GetModManager()->IsLoaded())
                SetSkin(currentSkinName);
            else
            {
                if (CConsoleInterface* pConsole = CCore::GetSingleton().GetConsole())
                    pConsole->Printf("Please disconnect before changing skin");
                cvars->Set("current_skin", m_LastSkinName);
            }
        }

        //
        // Check for locale change
        //
        SString currentLocaleName;
        cvars->Get("locale", currentLocaleName);

        if (m_LastLocaleName.empty())
        {
            m_LastLocaleName = currentLocaleName;
        }
        else if (!m_bHasQueuedLocaleChange && currentLocaleName != m_LastLocaleName)
        {
            RequestLocaleChange(currentLocaleName);
        }
    }

    if (m_bHasQueuedLocaleChange)
    {
        m_LocaleChangeCounter++;

        if (m_LocaleChangeCounter == 2)
            CCore::GetSingleton().ShowMessageBox(_E("CC99"), ("Changing language, please wait..."), MB_ICON_INFO);

        if (m_LocaleChangeCounter >= 5)
            ApplyQueuedLocale();
    }

    // Show after any deferred locale/skin work so a prior prompt is not lost
    TryShowRestartPrompt();
}

void CLocalGUI::PulseFrontend(const CMainMenu::FrontendFrame& frame)
{
    if (m_frontendOnly && m_pMainMenu && !IsFaultDialogOpen())
        m_pMainMenu->Update(frame);
}

void CLocalGUI::EnableFrontendConsole(const CConsole::FrontendHost& host)
{
    if (!m_frontendOnly || m_pConsole)
        throw std::logic_error("Frontend console creation is not admitted");
    // Publish only a complete hidden console. Its destructor handles a failed
    // visibility setup while the borrowed GUI and input owner still exist.
    auto console = std::make_unique<CConsole>(CCore::GetSingleton().GetGUI(), nullptr, &host);
    console->SetVisible(false);
    m_pConsole = console.release();
}

void CLocalGUI::DestroyFrontendConsole()
{
    if (m_frontendOnly)
    {
        m_frontendConsoleF8Down = false;
        SAFE_DELETE(m_pConsole);
    }
}

bool CLocalGUI::ProcessFrontendConsoleMessage(UINT message, WPARAM wparam, LPARAM lparam)
{
    if (!m_frontendOnly || !m_pConsole)
        return false;
    if (message == WM_KILLFOCUS || message == WM_CANCELMODE || (message == WM_ACTIVATE && LOWORD(wparam) == WA_INACTIVE))
        m_frontendConsoleF8Down = false;
    if ((message == WM_KEYDOWN || message == WM_KEYUP) && wparam == VK_F8)
    {
        if (message == WM_KEYDOWN)
        {
            if (!m_frontendConsoleF8Down && !(static_cast<UINT_PTR>(lparam) & (UINT_PTR{1} << 30)))
                SetConsoleVisible(!IsConsoleVisible());
            m_frontendConsoleF8Down = true;
        }
        else
            m_frontendConsoleF8Down = false;
        return true;
    }
    if (!m_pConsole->IsInputActive())
        return false;
    // Reuse the same history/navigation methods as the classic window hook.
    // Ordinary text/Enter remain with the admitted host's existing translator,
    // rather than injecting a second stream into the borrowed GUI.
    if (message == WM_CHAR && wparam == VK_TAB)
        return true;
    if (message == WM_KEYDOWN)
    {
        if (wparam == VK_UP)
            m_pConsole->SetNextHistoryText();
        else if (wparam == VK_DOWN)
            m_pConsole->SetPreviousHistoryText();
        else if (wparam == VK_TAB)
            m_pConsole->SetNextAutoCompleteMatch();
        if (wparam != VK_TAB)
            m_pConsole->ResetAutoCompleteMatch();
        if (wparam == VK_UP || wparam == VK_DOWN || wparam == VK_TAB)
            return true;
    }
    if (message == WM_CHAR)
        m_pConsole->ResetAutoCompleteMatch();
    return message == WM_KEYUP && (wparam == VK_UP || wparam == VK_DOWN || wparam == VK_TAB);
}
// True while the fatal CC51 dialog is open: nested faults exit instead of stacking.
static bool s_bFaultDialogOpen = false;
bool        CLocalGUI::IsFaultDialogOpen() noexcept
{
    if (s_bFaultDialogOpen)
        return true;

    // CC54 does the same from the CEGUI layer.
    if (CGUI* pGUI = CCore::GetSingleton().GetGUI())
        return pGUI->IsFatalFaultDialogOpen();

    return false;
}
void CLocalGUI::SetFaultDialogOpen(bool bOpen) noexcept
{
    s_bFaultDialogOpen = bOpen;
}

// CEGUI faults are caught in CGUI_Impl::Draw; anything else goes to the crash handler.
void CLocalGUI::Draw()
{
    // Get the game interface
    CGame*            pGame = CCore::GetSingleton().GetGame();
    SystemState::Enum systemState = pGame->GetSystemState();
    CGUI*             pGUI = CCore::GetSingleton().GetGUI();

    // Windows are gone mid-rebuild; skip drawing until they are back.
    if (!m_pMainMenu) [[unlikely]]
        return;

    // A fatal dialog is pumping messages; dont redraw CEGUI or the same fault
    // would rethrow each frame and terminate before the dialog can be read.
    if (CLocalGUI::IsFaultDialogOpen())
        return;

    // Update mainmenu stuff
    m_pMainMenu->Update();

    // If we're ingame, make sure the chatbox is drawn
    bool bChatVisible =
        (systemState == SystemState::GS_PLAYING_GAME && m_pMainMenu->GetIsIngame() && m_bChatboxVisible && !CCore::GetSingleton().IsOfflineMod());
    if (m_pChat->IsVisible() != bChatVisible)
        m_pChat->SetVisible(bChatVisible, !bChatVisible);
    bool bDebugVisible =
        (systemState == SystemState::GS_PLAYING_GAME && m_pMainMenu->GetIsIngame() && m_pDebugViewVisible && !CCore::GetSingleton().IsOfflineMod());
    if (m_pDebugView->IsVisible() != bDebugVisible)
        m_pDebugView->SetVisible(bDebugVisible, true);

    // Make sure the cursor is displayed only when needed
    UpdateCursor();

    // Draw the chat
    m_pChat->Draw(true, true);
    // Draw the debugger
    m_pDebugView->Draw(true, false);

    // If we're not at the loadingscreen
    static bool bDelayedFrame = false;
    if (systemState != SystemState::GS_INIT_PLAYING_GAME || !bDelayedFrame)
    {
        // If we have a GUI manager, draw the GUI
        if (pGUI)
        {
            pGUI->Draw();
        }

        // If the system state was 8, make sure we don't do another delayed frame
        if (systemState == SystemState::GS_INIT_PLAYING_GAME)
        {
            bDelayedFrame = true;
        }
    }
}

void CLocalGUI::Invalidate()
{
    CGUI* pGUI = CCore::GetSingleton().GetGUI();

    // Invalidate the GUI
    if (pGUI)
    {
        pGUI->Invalidate();
    }
}

void CLocalGUI::Restore()
{
    CGUI* pGUI = CCore::GetSingleton().GetGUI();

    if (pGUI)
    {
        // Restore the GUI
        pGUI->Restore();
    }
}

void CLocalGUI::DrawMouseCursor()
{
    // Same reason as Draw: the fault dialog's pump must not re-enter CEGUI.
    if (CLocalGUI::IsFaultDialogOpen())
        return;

    CGUI* pGUI = CCore::GetSingleton().GetGUI();

    if (pGUI)
        pGUI->DrawMouseCursor();
}

CConsole* CLocalGUI::GetConsole()
{
    return m_pConsole;
}

void CLocalGUI::SetConsoleVisible(bool bVisible)
{
    if (m_pConsole)
    {
        // Only allow the console if the chat doesn't take input
        if (!m_frontendOnly && m_pChat->IsInputVisible())
            bVisible = false;

        // Set the visible state
        m_pConsole->SetVisible(bVisible);

        CGUI* pGUI = CCore::GetSingleton().GetGUI();
        if (bVisible)
            pGUI->SetCursorAlpha(1.0f);
        else if (!m_frontendOnly && !g_pCore->IsMenuVisible())
            pGUI->SetCursorAlpha(pGUI->GetCurrentServerCursorAlpha());
    }
}

bool CLocalGUI::IsConsoleVisible()
{
    if (m_pConsole)
    {
        return m_pConsole->IsVisible();
    }
    return false;
}

void CLocalGUI::EchoConsole(const char* szText)
{
    if (m_pConsole)
    {
        m_pConsole->Echo(szText);
    }
}

CMainMenu* CLocalGUI::GetMainMenu()
{
    return m_pMainMenu;
}

void CLocalGUI::SetMainMenuVisible(bool bVisible)
{
    if (m_pMainMenu)
    {
        // This code installs the original CCore input handlers when the ingame menu
        // is shown, and restores the mod input handlers when the menu is hidden again.
        // This is needed for things like pressing escape when changing a key bind

        m_pMainMenu->SetVisible(bVisible);

        if (m_frontendOnly)
        {
            // Hiding the menu must not switch a console's borrowed input
            // channel to INPUT_MOD or release its independent ownership.
            if (m_pConsole && m_pConsole->IsVisible())
                m_pConsole->ActivateInput();
            return;
        }

        CGUI* pGUI = CCore::GetSingleton().GetGUI();
        if (bVisible)
        {
            pGUI->SelectInputHandlers(INPUT_CORE);
        }
        else
        {
            pGUI->SelectInputHandlers(INPUT_MOD);
        }

        if (!bVisible)
            pGUI->SetCursorAlpha(pGUI->GetCurrentServerCursorAlpha());
    }
}

bool CLocalGUI::IsMainMenuVisible()
{
    if (m_pMainMenu)
    {
        return m_pMainMenu->IsVisible();
    }
    return false;
}

CChat* CLocalGUI::GetChat()
{
    return m_pChat;
}

float CLocalGUI::GetChatBottomPosition() const noexcept
{
    return m_pChat->GetChatBottomPosition();
}

CDebugView* CLocalGUI::GetDebugView()
{
    return m_pDebugView;
}

void CLocalGUI::SetChatBoxVisible(bool bVisible, bool bInputBlocked)
{
    if (m_pChat)
    {
        if (m_pChat->IsVisible() != bVisible || m_pChat->IsInputBlocked() != bInputBlocked)
            m_pChat->SetVisible(bVisible, bInputBlocked);
        m_bChatboxVisible = bVisible;
        m_bChatboxInputBlocked = bInputBlocked;
    }
}

void CLocalGUI::SetDebugViewVisible(bool bVisible)
{
    if (m_pDebugView)
    {
        if (m_pDebugView->IsVisible() != bVisible)
            m_pDebugView->SetVisible(bVisible, true);
        m_pDebugViewVisible = bVisible;
    }
}

bool CLocalGUI::IsChatBoxVisible()
{
    if (m_pChat)
    {
        return m_bChatboxVisible;
    }
    return false;
}

bool CLocalGUI::IsChatBoxInputBlocked()
{
    if (m_pChat)
    {
        return m_bChatboxInputBlocked;
    }
    return false;
}

bool CLocalGUI::IsDebugViewVisible()
{
    if (m_pDebugView)
    {
        return m_pDebugViewVisible;
    }
    return false;
}

void CLocalGUI::SetChatBoxInputEnabled(bool bInputEnabled)
{
    if (m_pChat)
    {
        m_pChat->SetInputVisible(bInputEnabled);
    }
}

bool CLocalGUI::IsChatBoxInputEnabled()
{
    if (m_pChat)
    {
        return m_pChat->IsInputVisible() && !m_pChat->IsInputBlocked();
    }
    return false;
}

void CLocalGUI::EchoChat(const char* szText, bool bColorCoded)
{
    if (m_pChat)
    {
        m_pChat->Output(szText, bColorCoded);
    }
}

bool CLocalGUI::IsWebRequestGUIVisible()
{
    if (g_pCore->IsWebCoreLoaded())
    {
        return g_pCore->GetWebCore()->IsRequestsGUIVisible();
    }
    return false;
}

void CLocalGUI::EchoDebug(const char* szText)
{
    if (m_pDebugView)
    {
        m_pDebugView->Output(szText, false);
    }
}

void CLocalGUI::ResetInputState()
{
    // The classic window hook consumes focus-loss messages before ordinary
    // GUI dispatch. Retire their holds here while the GUI is still available.
    if (IsFaultDialogOpen())
        return;
    if (auto* gui = CCore::GetSingleton().GetGUI())
        m_Win32Input.Reset(*gui);
}

bool CLocalGUI::ProcessMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CGUI* pGUI = CCore::GetSingleton().GetGUI();

    // If we have the focus, we handle the message
    if (InputGoesToGUI())
    {
        // Shared ordinary input keeps the classic GUI and an owner-thread DE
        // surface on the same MTA/CEGUI path. IME retains its existing context.
        switch (uMsg)
        {
            case WM_IME_COMPOSITION:
            {
                if (lParam & GCS_RESULTSTR)
                {
                    HIMC himc = ImmGetContext(hwnd);

                    // Get composition result
                    ushort buffer[256];
                    LONG   numBytes = ImmGetCompositionStringW(himc, GCS_RESULTSTR, buffer, sizeof(buffer) - 2);
                    int    iNumCharacters = numBytes / sizeof(ushort);

                    // Erase output from previous composition state
                    for (int i = 0; i < m_uiActiveCompositionSize; i++)
                    {
                        pGUI->ProcessCharacter('\x08');
                        pGUI->ProcessKeyboardInput(14, true);
                    }

                    // Output composition result
                    for (int i = 0; i < iNumCharacters; i++)
                        if (buffer[i])
                            pGUI->ProcessCharacter(buffer[i]);

                    ImmReleaseContext(hwnd, himc);

                    m_uiActiveCompositionSize = 0;
                }
                else if (lParam & GCS_COMPSTR)
                {
                    HIMC himc = ImmGetContext(hwnd);

                    // Get composition state
                    ushort buffer[256];
                    LONG   numBytes = ImmGetCompositionStringW(himc, GCS_COMPSTR, buffer, sizeof(buffer) - 2);
                    int    iNumCharacters = numBytes / sizeof(ushort);

                    // Erase output from previous composition state
                    for (int i = 0; i < m_uiActiveCompositionSize; i++)
                    {
                        pGUI->ProcessCharacter('\x08');
                        pGUI->ProcessKeyboardInput(14, true);
                    }

                    // Output new composition state
                    for (int i = 0; i < iNumCharacters; i++)
                        if (buffer[i])
                            pGUI->ProcessCharacter(buffer[i]);

                    ImmReleaseContext(hwnd, himc);

                    m_uiActiveCompositionSize = iNumCharacters;
                }
            }
            break;

            case WM_IME_CHAR:
                return true;
            case WM_IME_KEYDOWN:
            {
                // Handle space/return separately in this case
                if (wParam == VK_SPACE)
                    pGUI->ProcessCharacter(MapVirtualKey(wParam, MAPVK_VK_TO_CHAR));

                DWORD dwTemp = TranslateScanCodeToGUIKey(wParam);
                if (dwTemp > 0)
                    pGUI->ProcessKeyboardInput(dwTemp, true);
            }
            break;

            case WM_UNICHAR:
                // Classic input historically leaves this message unconsumed;
                // the owned Unicode child can opt into the helper's support.
                return false;
            default:
                return m_Win32Input.Process(*pGUI, uMsg, wParam, lParam);
        }
    }

    // The event wasn't handled
    return false;
}

bool CLocalGUI::InputGoesToGUI()
{
    // A fatal fault dialog is pumping messages; dont route input into CEGUI.
    if (CLocalGUI::IsFaultDialogOpen())
        return false;

    CGUI* pGUI = CCore::GetSingleton().GetGUI();
    if (!pGUI)
        return false;

    return (IsConsoleVisible() || IsMainMenuVisible() || IsChatBoxInputEnabled() || m_bForceCursorVisible || pGUI->GetGUIInputEnabled() ||
            !CCore::GetSingleton().IsFocused() || IsWebRequestGUIVisible());
}

void CLocalGUI::ForceCursorVisible(bool bVisible)
{
    m_bForceCursorVisible = bVisible;
}

void CLocalGUI::UpdateCursor()
{
    // Only called from Draw, which already returns while a fatal fault dialog
    // is open, so the CEGUI calls below never run against a broken GUI state.
    CGUI* pGUI = CCore::GetSingleton().GetGUI();

    static DWORD dwWidth = CDirect3DData::GetSingleton().GetViewportWidth();
    static DWORD dwHeight = CDirect3DData::GetSingleton().GetViewportHeight();
    static bool  bFirstRun = true;

    if (bFirstRun)
    {
        m_StoredMousePosition.x = dwWidth / 2;
        m_StoredMousePosition.y = dwHeight / 2;
        bFirstRun = false;
    }
    // Called in each frame to make sure the mouse is only visible when a GUI control that uses the
    // mouse requires it.
    if (InputGoesToGUI())
    {
        // We didn't have focus last pulse?
        if (!m_bGUIHasInput)
        {
            /* Store if the controller was enabled or not and disable it
            CCore::GetSingleton ().GetGame ()->GetPad ()->Disable ( true );
            CCore::GetSingleton ().GetGame ()->GetPad ()->Clear ();*/

            // Restore the mouse cursor to its old position
            SetCursorPos(m_StoredMousePosition.x, m_StoredMousePosition.y);

            // Enable our mouse cursor
            CSetCursorPosHook::GetSingleton().DisableSetCursorPos();
            pGUI->SetCursorEnabled(true);

            m_bGUIHasInput = true;
        }
    }
    else
    {
        // We had focus last frame?
        if (m_bGUIHasInput)
        {
            /* Restore the controller state
            CCore::GetSingleton ().GetGame ()->GetPad ()->Disable ( false );
            CCore::GetSingleton ().GetGame ()->GetPad ()->Clear ();*/

            // Update stored position
            POINT point;
            GetCursorPos(&point);

            m_StoredMousePosition.x = point.x;
            m_StoredMousePosition.y = point.y;

            // Set the mouse back to the center of the screen (to prevent the game from reacting to its movement)
            SetCursorPos(dwWidth / 2, dwHeight / 2, false, false);

            // Disable our mouse cursor
            CSetCursorPosHook::GetSingleton().EnableSetCursorPos();
            pGUI->SetCursorEnabled(false);

            // A GUI can relinquish input before Windows delivers releases.
            // Clear every hold injected by the shared translator, plus IME modifiers.
            m_Win32Input.Reset(*pGUI);

            m_bGUIHasInput = false;
        }
    }
}

DWORD CLocalGUI::TranslateScanCodeToGUIKey(DWORD dwCharacter)
{
    return GuiWin32Input::Translator::TranslateVirtualKey(dwCharacter);
}

void CLocalGUI::SetCursorPos(int iX, int iY, bool bForce, bool overrideStored)
{
    // Update the stored position
    if (overrideStored)
    {
        m_StoredMousePosition.x = iX;
        m_StoredMousePosition.y = iY;
    }

    // Apply the position
    if (bForce)
        CSetCursorPosHook::GetSingleton().CallSetCursorPos(iX, iY);
    else
        ::SetCursorPos(iX, iY);
}

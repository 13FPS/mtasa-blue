/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe real frontend widgets without replacing their implementations
 *****************************************************************************/
#pragma once

// The SDK intentionally exposes no widget-tree enumeration. This observer
// forwards the borrowed GUI interface and retains actual factory returns so
// owned-host tests can inspect the existing settings grid and its real binds.
// It never manufactures controls or replaces production callbacks.
class ObservedFrontendGui final : public CGUI
{
public:
    explicit ObservedFrontendGui(CGUI& gui) : actual(gui) {}
    CGUI&                    actual;
    CGUIWindow*              settingsWindow = nullptr;
    CGUIGridList*            settingsGrid = nullptr;
    CGUITabPanel*            settingsTabs = nullptr;
    CGUIButton*              settingsApply = nullptr;
    CGUIButton*              settingsCancel = nullptr;
    std::vector<std::string> settingsButtons;

    void ResetSettingsObservation()
    {
        // These are borrowed public widget addresses, not GUI ownership. The
        // completed frontend release destroys them while the renderer survives;
        // clear observations before any later frontend factory can inspect them.
        settingsWindow = nullptr;
        settingsGrid = nullptr;
        settingsTabs = nullptr;
        settingsApply = nullptr;
        settingsCancel = nullptr;
        settingsButtons.clear();
    }

    bool InSettings(CGUIElement* parent) const
    {
        // SDK factories return the public widget base while GetParent may
        // expose another CGUIElement base of the multiple-inheritance Impl.
        // Observe the actual factory relationships and public selected tab;
        // traversing implementation parent representations can miss this grid.
        return parent && (parent == settingsWindow || parent == settingsTabs || (settingsTabs && parent == settingsTabs->GetSelectedTab()));
    }
    void RecordButton(CGUIButton* button, CGUIElement* parent, const char* caption)
    {
        if (!InSettings(parent))
            return;
        settingsButtons.emplace_back(caption);
        if (std::strcmp(caption, "Apply (session only)") == 0)
            settingsApply = button;
        if (std::strcmp(caption, "Cancel") == 0)
            settingsCancel = button;
    }

    void SetSkin(const char* szName) override { actual.SetSkin(szName); }
    void SetBidiEnabled(bool bEnabled) override { actual.SetBidiEnabled(bEnabled); }
    void Draw() override { actual.Draw(); }
    void Invalidate() override { actual.Invalidate(); }
    void Restore() override { actual.Restore(); }
    void DrawMouseCursor() override { actual.DrawMouseCursor(); }
    void ProcessMouseInput(CGUIMouseInput eMouseInput, unsigned long ulX, unsigned long ulY, CGUIMouseButton eMouseButton) override
    {
        actual.ProcessMouseInput(eMouseInput, ulX, ulY, eMouseButton);
    }
    void            ProcessKeyboardInput(unsigned long ulKey, bool bIsDown) override { actual.ProcessKeyboardInput(ulKey, bIsDown); }
    void            ProcessCharacter(unsigned long ulCharacter) override { actual.ProcessCharacter(ulCharacter); }
    bool            GetGUIInputEnabled() override { return actual.GetGUIInputEnabled(); }
    void            SetGUIInputMode(eInputMode a_eMode) override { actual.SetGUIInputMode(a_eMode); }
    eInputMode      GetGUIInputMode() override { return actual.GetGUIInputMode(); }
    CGUIMessageBox* CreateMessageBox(const char* szTitle, const char* szMessage, unsigned int uiFlags) override
    {
        return actual.CreateMessageBox(szTitle, szMessage, uiFlags);
    }
    CGUIButton* CreateButton(CGUIElement* pParent, const char* szCaption) override
    {
        auto* element = actual.CreateButton(pParent, szCaption);
        RecordButton(element, pParent, szCaption);
        return element;
    }
    CGUIButton* CreateButton(CGUITab* pParent, const char* szCaption) override
    {
        auto* element = actual.CreateButton(pParent, szCaption);
        RecordButton(element, pParent, szCaption);
        return element;
    }
    CGUICheckBox* CreateCheckBox(CGUIElement* pParent, const char* szCaption, bool bChecked) override
    {
        return actual.CreateCheckBox(pParent, szCaption, bChecked);
    }
    CGUICheckBox* CreateCheckBox(CGUITab* pParent, const char* szCaption, bool bChecked) override
    {
        return actual.CreateCheckBox(pParent, szCaption, bChecked);
    }
    CGUIRadioButton* CreateRadioButton(CGUIElement* pParent, const char* szCaption) override { return actual.CreateRadioButton(pParent, szCaption); }
    CGUIRadioButton* CreateRadioButton(CGUITab* pParent, const char* szCaption) override { return actual.CreateRadioButton(pParent, szCaption); }
    CGUIEdit*        CreateEdit(CGUIElement* pParent, const char* szText) override { return actual.CreateEdit(pParent, szText); }
    CGUIEdit*        CreateEdit(CGUITab* pParent, const char* szText) override { return actual.CreateEdit(pParent, szText); }
    CGUIGridList*    CreateGridList(CGUIElement* pParent, bool bFrame) override
    {
        auto* element = actual.CreateGridList(pParent, bFrame);
        if (InSettings(pParent))
            settingsGrid = element;
        return element;
    }
    CGUIGridList* CreateGridList(CGUITab* pParent, bool bFrame) override
    {
        auto* element = actual.CreateGridList(pParent, bFrame);
        if (InSettings(pParent))
            settingsGrid = element;
        return element;
    }
    CGUILabel*       CreateLabel(CGUIElement* pParent, const char* szCaption) override { return actual.CreateLabel(pParent, szCaption); }
    CGUILabel*       CreateLabel(CGUITab* pParent, const char* szCaption) override { return actual.CreateLabel(pParent, szCaption); }
    CGUILabel*       CreateLabel(const char* szCaption) override { return actual.CreateLabel(szCaption); }
    CGUIProgressBar* CreateProgressBar(CGUIElement* pParent) override { return actual.CreateProgressBar(pParent); }
    CGUIProgressBar* CreateProgressBar(CGUITab* pParent) override { return actual.CreateProgressBar(pParent); }
    CGUIMemo*        CreateMemo(CGUIElement* pParent, const char* szText) override { return actual.CreateMemo(pParent, szText); }
    CGUIMemo*        CreateMemo(CGUITab* pParent, const char* szText) override { return actual.CreateMemo(pParent, szText); }
    CGUIStaticImage* CreateStaticImage(CGUIElement* pParent) override { return actual.CreateStaticImage(pParent); }
    CGUIStaticImage* CreateStaticImage(CGUITab* pParent) override { return actual.CreateStaticImage(pParent); }
    CGUIStaticImage* CreateStaticImage(CGUIGridList* pParent) override { return actual.CreateStaticImage(pParent); }
    CGUIStaticImage* CreateStaticImage() override { return actual.CreateStaticImage(); }
    CGUITabPanel*    CreateTabPanel(CGUIElement* pParent) override
    {
        auto* element = actual.CreateTabPanel(pParent);
        if (InSettings(pParent))
            settingsTabs = element;
        return element;
    }
    CGUITabPanel* CreateTabPanel(CGUITab* pParent) override
    {
        auto* element = actual.CreateTabPanel(pParent);
        if (InSettings(pParent))
            settingsTabs = element;
        return element;
    }
    CGUITabPanel* CreateTabPanel() override
    {
        auto* element = actual.CreateTabPanel();
        // A top-level tab panel cannot belong to the constrained settings window.
        return element;
    }
    CGUIScrollPane* CreateScrollPane(CGUIElement* pParent) override { return actual.CreateScrollPane(pParent); }
    CGUIScrollPane* CreateScrollPane(CGUITab* pParent) override { return actual.CreateScrollPane(pParent); }
    CGUIScrollPane* CreateScrollPane() override { return actual.CreateScrollPane(); }
    CGUIScrollBar*  CreateScrollBar(bool bHorizontal, CGUIElement* pParent) override { return actual.CreateScrollBar(bHorizontal, pParent); }
    CGUIScrollBar*  CreateScrollBar(bool bHorizontal, CGUITab* pParent) override { return actual.CreateScrollBar(bHorizontal, pParent); }
    CGUIComboBox*   CreateComboBox(CGUIElement* pParent, const char* szCaption) override { return actual.CreateComboBox(pParent, szCaption); }
    CGUIComboBox*   CreateComboBox(CGUIComboBox* pParent, const char* szCaption) override { return actual.CreateComboBox(pParent, szCaption); }
    CGUIWebBrowser* CreateWebBrowser(CGUIElement* pParent) override { return actual.CreateWebBrowser(pParent); }
    CGUIWebBrowser* CreateWebBrowser(CGUITab* pParent) override { return actual.CreateWebBrowser(pParent); }
    CGUIWindow*     CreateWnd(CGUIElement* pParent, const char* szCaption) override
    {
        auto* element = actual.CreateWnd(pParent, szCaption);
        if (std::strcmp(szCaption, "SETTINGS") == 0)
            settingsWindow = element;
        return element;
    }
    CGUIFont* CreateFnt(const char* szFontName, const char* szFontFile, unsigned int uSize, unsigned int uFlags, bool bAutoScale) override
    {
        return actual.CreateFnt(szFontName, szFontFile, uSize, uFlags, bAutoScale);
    }
    CGUITexture* CreateTexture() override { return actual.CreateTexture(); }
    void         SetCursorEnabled(bool bEnabled) override { actual.SetCursorEnabled(bEnabled); }
    bool         IsCursorEnabled() override { return actual.IsCursorEnabled(); }
    void         SetCursorAlpha(float fAlpha, bool bOnlyCurrentServer) override { actual.SetCursorAlpha(fAlpha, bOnlyCurrentServer); }
    void         SetCurrentServerCursorAlpha(float fAlpha) override { actual.SetCurrentServerCursorAlpha(fAlpha); }
    float        GetCurrentServerCursorAlpha() override { return actual.GetCurrentServerCursorAlpha(); }
    eCursorType  GetCursorType() override { return actual.GetCursorType(); }
    CVector2D    GetResolution() override { return actual.GetResolution(); }
    void         SetResolution(float fWidth, float fHeight) override { actual.SetResolution(fWidth, fHeight); }
    CGUIFont*    GetDefaultFont() override { return actual.GetDefaultFont(); }
    CGUIFont*    GetSmallFont() override { return actual.GetSmallFont(); }
    CGUIFont*    GetBoldFont() override { return actual.GetBoldFont(); }
    CGUIFont*    GetClearFont() override { return actual.GetClearFont(); }
    CGUIFont*    GetSansFont() override { return actual.GetSansFont(); }
    bool         IsFontPresent(const char* szFont) override { return actual.IsFontPresent(szFont); }
    float        GetTextExtent(const char* szText, const char* szFont) override { return actual.GetTextExtent(szText, szFont); }
    float        GetMaxTextExtent(SString strFont, SString arg, ...) override
    {
        (void)strFont;
        (void)arg;
        throw std::runtime_error("restricted frontend constructed an unsupported full-settings layout");
    }
    void PushGuiWorkingDirectory(const SString& strDir) override { actual.PushGuiWorkingDirectory(strDir); }
    void PopGuiWorkingDirectory(const SString& strDirCheck) override { actual.PopGuiWorkingDirectory(strDirCheck); }
    void SetCharacterKeyHandler(eInputChannel channel, const GUI_CALLBACK_KEY& Callback) override { actual.SetCharacterKeyHandler(channel, Callback); }
    void SetKeyDownHandler(eInputChannel channel, const GUI_CALLBACK_KEY& Callback) override { actual.SetKeyDownHandler(channel, Callback); }
    void SetMouseClickHandler(eInputChannel channel, const GUI_CALLBACK_MOUSE& Callback) override { actual.SetMouseClickHandler(channel, Callback); }
    void SetMouseDoubleClickHandler(eInputChannel channel, const GUI_CALLBACK_MOUSE& Callback) override
    {
        actual.SetMouseDoubleClickHandler(channel, Callback);
    }
    void SetMouseButtonDownHandler(eInputChannel channel, const GUI_CALLBACK_MOUSE& Callback) override { actual.SetMouseButtonDownHandler(channel, Callback); }
    void SetMouseButtonUpHandler(eInputChannel channel, const GUI_CALLBACK_MOUSE& Callback) override { actual.SetMouseButtonUpHandler(channel, Callback); }
    void SetMouseMoveHandler(eInputChannel channel, const GUI_CALLBACK_MOUSE& Callback) override { actual.SetMouseMoveHandler(channel, Callback); }
    void SetMouseEnterHandler(eInputChannel channel, const GUI_CALLBACK_MOUSE& Callback) override { actual.SetMouseEnterHandler(channel, Callback); }
    void SetMouseLeaveHandler(eInputChannel channel, const GUI_CALLBACK_MOUSE& Callback) override { actual.SetMouseLeaveHandler(channel, Callback); }
    void SetMouseWheelHandler(eInputChannel channel, const GUI_CALLBACK_MOUSE& Callback) override { actual.SetMouseWheelHandler(channel, Callback); }
    void SetMovedHandler(eInputChannel channel, const GUI_CALLBACK& Callback) override { actual.SetMovedHandler(channel, Callback); }
    void SetSizedHandler(eInputChannel channel, const GUI_CALLBACK& Callback) override { actual.SetSizedHandler(channel, Callback); }
    void SetFocusGainedHandler(eInputChannel channel, const GUI_CALLBACK_FOCUS& Callback) override { actual.SetFocusGainedHandler(channel, Callback); }
    void SetFocusLostHandler(eInputChannel channel, const GUI_CALLBACK_FOCUS& Callback) override { actual.SetFocusLostHandler(channel, Callback); }
    void SelectInputHandlers(eInputChannel channel) override { actual.SelectInputHandlers(channel); }
    void ClearInputHandlers(eInputChannel channel) override { actual.ClearInputHandlers(channel); }
    void ClearSystemKeys() override { actual.ClearSystemKeys(); }
    void CleanDeadPool() override { actual.CleanDeadPool(); }
    void DestroyElementRecursive(CGUIElement* pElement) override { actual.DestroyElementRecursive(pElement); }
    CGUIWindow*  LoadLayout(CGUIElement* pParent, const SString& strFilename) override { return actual.LoadLayout(pParent, strFilename); }
    bool         LoadImageset(const SString& strFilename) override { return actual.LoadImageset(strFilename); }
    void         Cleanup() override { actual.Cleanup(); }
    CGUIElement* GetScriptRoot() override { return actual.GetScriptRoot(); }
    void         SetFatalFaultDialogOpen(bool bOpen) override { actual.SetFatalFaultDialogOpen(bOpen); }
    bool         IsFatalFaultDialogOpen() const override { return actual.IsFatalFaultDialogOpen(); }
};

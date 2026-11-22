#pragma once

#include "resource.h"
#include "Interfaces.h"
#include <NativeCustomTabView.h>
#include <TabViewHelper.h>

// the frame's client: one tab per open file or folder, each with a close button; the image list is the frame's
class CMainTabView : public CNativeCustomTabViewImpl<CMainTabView> {
public:
	DECLARE_WND_CLASS_EX(_T("NtfsStreams_TabView"), 0, COLOR_APPWORKSPACE)

	CMainTabView() {
		m_bDestroyImageList = false;
	}

	// the tabs' text: a tab control takes '&' for a mnemonic, so it's doubled ("c&d.txt" showed as "cd.txt", the d
	// underlined). The frame sets its own caption (UpdateCaption), as the title bar text comes through here too
	void ShortenTitle(LPCTSTR lpstrTitle, LPTSTR lpstrShortTitle, int cchShortTitle) {
		std::wstring escaped;
		for (auto p = lpstrTitle; *p; p++) {
			if (*p == L'&')
				escaped += L'&';
			escaped += *p;
		}
		if ((int)escaped.size() < cchShortTitle) {
			::wcscpy_s(lpstrShortTitle, cchShortTitle, escaped.c_str());
			return;
		}
		escaped.resize(cchShortTitle - 4);
		// not half of a "&&"
		size_t amps = 0;
		for (auto it = escaped.rbegin(); it != escaped.rend() && *it == L'&'; ++it)
			amps++;
		if (amps % 2)
			escaped.pop_back();
		::wcscpy_s(lpstrShortTitle, cchShortTitle, (escaped + L"...").c_str());
	}
};

//
// The main window. Tabs are matched to views by their windows (GetView), never by index: closing and dragging tabs
// shift the indices. A view is forgotten (ForgetView) before its page is removed, as removing it destroys the view's
// window and the view deletes itself.
//
class CMainFrame :
	public CFrameWindowImpl<CMainFrame>,
	public CAutoUpdateUI<CMainFrame>,
	public IMainFrame,
	public CMessageFilter,
	public CIdleHandler {
public:
	DECLARE_FRAME_WND_CLASS(L"NtfsStreamsFrame", IDR_MAINFRAME)

	BOOL PreTranslateMessage(MSG* pMsg) override;
	BOOL OnIdle() override;

	// IMainFrame
	HWND GetHwnd() const override {
		return m_hWnd;
	}
	CUpdateUIBase& GetUI() override {
		return *this;
	}
	void SetStatusText(PCWSTR text) override;
	BOOL ShowContextMenu(HMENU hMenu, POINT const& pt, DWORD flags = 0) override;
	HIMAGELIST GetImageList() const override {
		return m_Images;
	}
	int GetIconIndex(UINT iconId) const override;
	bool OpenFile(std::wstring const& path, bool activate = true) override;

	bool OpenFolder(std::wstring const& path);

	BEGIN_MSG_MAP(CMainFrame)
		COMMAND_TABVIEW_HANDLER(m_Tabs, 1)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_ENDSESSION, OnEndSession)
		MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
		NOTIFY_CODE_HANDLER(TBVN_PAGEACTIVATED, OnPageActivated)
		NOTIFY_CODE_HANDLER(TBVN_TABCLOSEBTN, OnTabClose)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_OPENFILE, OnOpenFile)
		COMMAND_ID_HANDLER(ID_FILE_OPENFOLDER, OnOpenFolder)
		COMMAND_RANGE_HANDLER(ID_RECENT_FILE_FIRST, ID_RECENT_FILE_LAST, OnRecentFile)
		COMMAND_RANGE_HANDLER(ID_RECENT_FOLDER_FIRST, ID_RECENT_FOLDER_LAST, OnRecentFolder)
		COMMAND_ID_HANDLER(ID_FILE_CLOSE, OnCloseTab)
		COMMAND_ID_HANDLER(ID_VIEW_CLOSEALL, OnCloseAll)
		COMMAND_ID_HANDLER(ID_VIEW_CLOSEALLBUTTHIS, OnCloseAllButThis)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_OPTIONS_DARKMODE, OnDarkMode)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		CHAIN_MSG_MAP(CAutoUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnEndSession(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDpiChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnPageActivated(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTabClose(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRecentFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRecentFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCloseTab(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCloseAllButThis(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void InitToolBar();
	void InitMenu();
	void InitStatusBar();
	void InitImageList();
	void ShowToolBar(bool show);
	void ShowStatusBar(bool show);
	void SetDarkMode(bool dark);
	void SaveSettings();

	// tabs
	IView* GetView(int page) const;
	IView* FindView(ViewType type, std::wstring const& path, int* page = nullptr) const;
	// adds the view's page (the view is created here); activate: otherwise the active page stays
	bool AddView(IView* view, std::function<HWND(HWND)> const& create, bool activate);
	void ForgetView(IView* view);
	void CloseTab(int page);

	// the File menu's Recent Files and Recent Folders, from the settings
	void UpdateRecentMenus();
	void AddRecent(bool folder, std::wstring const& path);
	void RemoveRecent(bool folder, std::wstring const& path);

	void ShowError(std::wstring const& message) const;
	// "<active tab> - NTFS Streams (Administrator)"
	void UpdateCaption();

	static constexpr int ToolBarIconSize = 24;	// at 96 DPI
	static constexpr size_t MaxRecent = 9;

	CMainTabView m_Tabs;
	CToolBarCtrl m_ToolBar;
	CMultiPaneStatusBarCtrl m_StatusBar;
	CImageList m_Images;
	std::vector<IView*> m_Views;	// the open views, in no particular order
	IView* m_ActiveView{ nullptr };
	CString m_Caption;	// the app's own, without a tab's title
};

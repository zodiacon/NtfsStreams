#include "pch.h"
#include "MainFrm.h"
#include "AboutDlg.h"
#include "AppSettings.h"
#include <WTLHelper.h>
#include <ToolbarHelper.h>
#include <DpiHelper.h>

namespace {
	const ToolBarButtonInfo s_ToolBarButtons[] = {
		{ ID_FILE_OPENFILE, IDI_FILE_VIEW },
		{ ID_FILE_OPENFOLDER, IDI_FOLDER_VIEW },
		{ 0 },
		{ ID_VIEW_VIEWFILES, IDI_VIEW },
		{ ID_VIEW_CLOSEALL, IDI_CLOSE },
	};

	bool IsRunningElevated() {
		wil::unique_handle hToken;
		if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, hToken.addressof()))
			return false;
		TOKEN_ELEVATION elevation{};
		DWORD len;
		return ::GetTokenInformation(hToken.get(), TokenElevation, &elevation, sizeof(elevation), &len) && elevation.TokenIsElevated;
	}
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	InitToolBar();
	CreateSimpleStatusBar();

	m_hWndClient = m_Tabs.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

	if (IsRunningElevated()) {
		CString title;
		GetWindowText(title);
		SetWindowText(title + L" (Administrator)");
	}

	UIAddMenu(GetMenu());
	// the commands that need open tabs (or a folder tab's selection) come with them
	UIEnable(ID_VIEW_VIEWFILES, false);
	UIEnable(ID_VIEW_CLOSEALL, false);
	UIEnable(ID_VIEW_CLOSEALLBUTTHIS, false);
	// not yet implemented
	UIEnable(ID_FILE_OPENFILE, false);
	UIEnable(ID_FILE_OPENFOLDER, false);

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	auto& settings = AppSettings::Get();
	ShowToolBar(settings.ToolBar());
	ShowStatusBar(settings.StatusBar());
	UISetCheck(ID_OPTIONS_DARKMODE, settings.DarkMode());
	SetDarkMode(settings.DarkMode());

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	SaveSettings();

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnEndSession(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	// at logoff or shutdown no window is destroyed
	if (wParam)
		SaveSettings();
	return 0;
}

LRESULT CMainFrame::OnDpiChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	auto rc = reinterpret_cast<RECT const*>(lParam);
	SetWindowPos(nullptr, rc, SWP_NOZORDER | SWP_NOACTIVATE);

	ToolbarHelper::UpdateToolBarImages(m_ToolBar, s_ToolBarButtons, _countof(s_ToolBarButtons), ToolBarIconSize);
	m_ToolBar.AutoSize();
	InitMenu();
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool show = !::IsWindowVisible(m_hWndToolBar);
	ShowToolBar(show);
	auto& settings = AppSettings::Get();
	settings.ToolBar(show);
	settings.Save();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool show = !::IsWindowVisible(m_hWndStatusBar);
	ShowStatusBar(show);
	auto& settings = AppSettings::Get();
	settings.StatusBar(show);
	settings.Save();
	return 0;
}

LRESULT CMainFrame::OnDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto& settings = AppSettings::Get();
	bool dark = !settings.DarkMode();
	settings.DarkMode(dark);
	settings.Save();
	UISetCheck(ID_OPTIONS_DARKMODE, dark);
	SetDarkMode(dark);
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

void CMainFrame::InitToolBar() {
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	// the button images' size is at 96 DPI, scaled to the frame's
	m_ToolBar = ToolbarHelper::CreateAndInitToolBar(m_hWnd, s_ToolBarButtons, _countof(s_ToolBarButtons), ToolBarIconSize);
	AddSimpleReBarBand(m_ToolBar);
	UIAddToolBar(m_ToolBar);
}

void CMainFrame::InitMenu() {
	static const MenuItemData commands[] = {
		{ ID_FILE_OPENFILE, IDI_FILE_VIEW },
		{ ID_FILE_OPENFOLDER, IDI_FOLDER_VIEW },
		{ ID_VIEW_VIEWFILES, IDI_VIEW },
		{ ID_VIEW_CLOSEALL, IDI_CLOSE },
	};
	WTLHelper::InitMenu(GetMenu(), commands, _countof(commands), DpiHelper::SmallIconSize(m_hWnd));
}

void CMainFrame::ShowToolBar(bool show) {
	// m_hWndToolBar is the rebar hosting the toolbar
	::ShowWindow(m_hWndToolBar, show ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_TOOLBAR, show);
	UpdateLayout();
}

void CMainFrame::ShowStatusBar(bool show) {
	::ShowWindow(m_hWndStatusBar, show ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, show);
	UpdateLayout();
}

void CMainFrame::SetDarkMode(bool dark) {
	WTLHelper::SwitchToMode(dark ? DarkModeKind::Dark : DarkModeKind::Light, m_hWnd);

	// menu icon bitmaps are drawn on the current menu background, so rebuild them for the new mode
	InitMenu();

	CStatusBarCtrl(m_hWndStatusBar).SetBkColor(dark ? DarkMode::getBackgroundColor() : CLR_DEFAULT);

	::EnumThreadWindows(::GetCurrentThreadId(), [](auto h, auto) {
		::RedrawWindow(h, nullptr, nullptr, RDW_ERASENOW | RDW_INTERNALPAINT | RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
		return TRUE;
		}, 0);

	DrawMenuBar();
}

void CMainFrame::SaveSettings() {
	auto& settings = AppSettings::Get();
	settings.SaveWindowPosition(m_hWnd, AppSettings::MainWindowPlacement);
	settings.Save();
}

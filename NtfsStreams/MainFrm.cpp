#include "pch.h"
#include "MainFrm.h"
#include "AboutDlg.h"
#include "AppSettings.h"
#include "Helpers.h"
#include "Streams.h"
#include "FileStreamsView.h"
#include "FolderView.h"
#include <WTLHelper.h>
#include <ToolbarHelper.h>
#include <IconHelper.h>
#include <DpiHelper.h>

namespace {
	const ToolBarButtonInfo s_ToolBarButtons[] = {
		{ ID_FILE_OPENFILE, IDI_FILE_VIEW },
		{ ID_FILE_OPENFOLDER, IDI_FOLDER_VIEW },
		{ 0 },
		{ ID_VIEW_VIEWFILES, IDI_VIEW },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ ID_VIEW_CLOSEALL, IDI_CLOSE },
	};

	// the shared image list's icons, in their order there
	const UINT s_Icons[] = { IDI_FILE, IDI_FOLDER, IDI_FILE_VIEW, IDI_FOLDER_VIEW, IDI_VIEW, IDI_CLOSE, IDI_REFRESH, IDI_STOP,
		IDI_SUBFOLDERS, IDI_COPY, IDI_DELETE };

	int s_Panes[] = { ID_DEFAULT_PANE, ID_PANE_VIEW };

	bool IsRunningElevated() {
		wil::unique_handle hToken;
		if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, hToken.addressof()))
			return false;
		TOKEN_ELEVATION elevation{};
		DWORD len;
		return ::GetTokenInformation(hToken.get(), TokenElevation, &elevation, sizeof(elevation), &len) && elevation.TokenIsElevated;
	}

	bool IsNotFound(DWORD error) {
		return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_BAD_NETPATH || error == ERROR_INVALID_DRIVE;
	}
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	int count = m_Tabs.GetPageCount();
	UIEnable(ID_FILE_CLOSE, count > 0);
	UIEnable(ID_VIEW_CLOSEALL, count > 0);
	UIEnable(ID_VIEW_CLOSEALLBUTTHIS, count > 1);
	// the active view enables its own
	for (auto id : { ID_VIEW_VIEWFILES, ID_VIEW_REFRESH, ID_EDIT_COPY, ID_STREAM_EXPORT })
		UIEnable(id, false);
	if (m_ActiveView)
		m_ActiveView->UpdateUI(*this);
	UIUpdateToolBar();
	return FALSE;
}

void CMainFrame::SetStatusText(PCWSTR text) {
	m_StatusBar.SetPaneText(ID_PANE_VIEW, text);
}

BOOL CMainFrame::ShowContextMenu(HMENU hMenu, POINT const& pt, DWORD flags) {
	// commands land in the frame and are routed to the active view
	return ::TrackPopupMenuEx(hMenu, flags, pt.x, pt.y, m_hWnd, nullptr);
}

int CMainFrame::GetIconIndex(UINT iconId) const {
	auto it = std::ranges::find(s_Icons, iconId);
	return it == std::end(s_Icons) ? -1 : int(it - std::begin(s_Icons));
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	InitImageList();
	InitToolBar();
	InitStatusBar();

	m_hWndClient = m_Tabs.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
	m_Tabs.SetImageList(m_Images);

	GetWindowText(m_Caption);
	if (IsRunningElevated())
		m_Caption += L" (Administrator)";
	UpdateCaption();

	UIAddMenu(GetMenu());
	UpdateRecentMenus();

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

	// the views delete themselves as their windows are destroyed, and the tab view activates pages while removing
	// them (TBVN_PAGEACTIVATED), so stop talking to the views now
	m_Views.clear();
	m_ActiveView = nullptr;

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

	InitImageList();
	m_Tabs.SetImageList(m_Images);
	auto dpi = DpiHelper::GetDpi(m_hWnd);
	for (auto view : m_Views)
		view->OnDpiChanged(dpi);

	ToolbarHelper::UpdateToolBarImages(m_ToolBar, s_ToolBarButtons, _countof(s_ToolBarButtons), ToolBarIconSize);
	m_ToolBar.AutoSize();
	InitMenu();
	m_StatusBar.SetPaneWidth(ID_PANE_VIEW, DpiHelper::Scale(400, m_hWnd));
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnPageActivated(int /*idCtrl*/, LPNMHDR hdr, BOOL& bHandled) {
	if (hdr->hwndFrom != m_Tabs) {
		bHandled = FALSE;
		return 0;
	}
	auto view = GetView((int)hdr->idFrom);
	if (view != m_ActiveView) {
		if (m_ActiveView)
			m_ActiveView->OnActivate(false);
		m_ActiveView = view;
		if (view)
			view->OnActivate(true);
	}
	// also with no view: the last tab closed (its view was forgotten first)
	SetStatusText(view ? view->GetStatusText().c_str() : L"");
	UpdateCaption();
	return 0;
}

LRESULT CMainFrame::OnTabClose(int /*idCtrl*/, LPNMHDR hdr, BOOL& bHandled) {
	if (hdr->hwndFrom != m_Tabs) {
		bHandled = FALSE;
		return 0;
	}
	// the tab view removes the page (0), which destroys the view's window
	ForgetView(GetView((int)hdr->idFrom));
	return 0;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CShellFileOpenDialog dlg(nullptr, FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT);
	dlg.GetPtr()->SetTitle(L"Open File");
	if (dlg.DoModal(m_hWnd) != IDOK)
		return 0;

	CComPtr<IShellItemArray> items;
	if (FAILED(dlg.GetPtr()->GetResults(&items)))
		return 0;
	DWORD count = 0;
	items->GetCount(&count);
	bool first = true;
	for (DWORD i = 0; i < count; i++) {
		CComPtr<IShellItem> item;
		wil::unique_cotaskmem_string path;
		if (SUCCEEDED(items->GetItemAt(i, &item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && OpenFile(path.get(), first))
			first = false;
	}
	return 0;
}

LRESULT CMainFrame::OnOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CShellFileOpenDialog dlg(nullptr, FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_PICKFOLDERS);
	dlg.GetPtr()->SetTitle(L"Open Folder");
	if (dlg.DoModal(m_hWnd) != IDOK)
		return 0;
	CString path;
	if (SUCCEEDED(dlg.GetFilePath(path)))
		OpenFolder((PCWSTR)path);
	return 0;
}

LRESULT CMainFrame::OnRecentFile(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto files = AppSettings::Get().RecentFiles();
	size_t index = wID - ID_RECENT_FILE_FIRST;
	if (index < files.size())
		OpenFile(files[index]);
	return 0;
}

LRESULT CMainFrame::OnRecentFolder(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto folders = AppSettings::Get().RecentFolders();
	size_t index = wID - ID_RECENT_FOLDER_FIRST;
	if (index < folders.size())
		OpenFolder(folders[index]);
	return 0;
}

LRESULT CMainFrame::OnCloseTab(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (auto page = m_Tabs.GetActivePage(); page >= 0)
		CloseTab(page);
	return 0;
}

LRESULT CMainFrame::OnCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_Views.clear();
	m_ActiveView = nullptr;
	m_Tabs.RemoveAllPages();
	return 0;
}

LRESULT CMainFrame::OnCloseAllButThis(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto active = GetView(m_Tabs.GetActivePage());
	if (!active)
		return 0;
	for (int page = m_Tabs.GetPageCount() - 1; page >= 0; page--)
		if (GetView(page) != active)
			CloseTab(page);
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

bool CMainFrame::OpenFile(std::wstring const& file, bool activate) {
	auto path = Helpers::NormalizePath(file);
	if (int page; FindView(ViewType::File, path, &page)) {
		if (activate)
			m_Tabs.SetActivePage(page);
		return true;
	}

	auto streams = Streams::GetStreams(path);
	if (!streams) {
		ShowError(std::format(L"{}\n\n{}", path, Helpers::ErrorText(streams.error())));
		if (IsNotFound(streams.error()))
			RemoveRecent(false, path);
		return false;
	}
	if (streams->empty()) {
		AtlMessageBox(m_hWnd, std::format(L"{}\n\nhas no alternate streams.", path).c_str(), IDR_MAINFRAME, MB_ICONINFORMATION);
		return false;
	}

	bool dir = (::GetFileAttributes(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0;
	auto view = new CFileStreamsView(this, Streams::FileStreams{ path, std::move(*streams), dir });
	if (!AddView(view, [=](HWND hParent) { return view->DoCreate(hParent); }, activate))
		return false;
	AddRecent(false, path);
	return true;
}

bool CMainFrame::OpenFolder(std::wstring const& folder) {
	auto path = Helpers::NormalizePath(folder);
	if (int page; FindView(ViewType::Folder, path, &page)) {
		m_Tabs.SetActivePage(page);
		return true;
	}

	auto attributes = ::GetFileAttributes(path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		auto error = attributes == INVALID_FILE_ATTRIBUTES ? ::GetLastError() : (DWORD)ERROR_DIRECTORY;
		ShowError(std::format(L"{}\n\n{}", path, Helpers::ErrorText(error)));
		if (IsNotFound(error) || error == ERROR_DIRECTORY)
			RemoveRecent(true, path);
		return false;
	}

	auto view = new CFolderView(this, path);
	if (!AddView(view, [=](HWND hParent) { return view->DoCreate(hParent); }, true))
		return false;
	AddRecent(true, path);
	return true;
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
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ ID_EDIT_COPY, IDI_COPY },
	};
	WTLHelper::InitMenu(GetMenu(), commands, _countof(commands), DpiHelper::SmallIconSize(m_hWnd));
}

void CMainFrame::InitStatusBar() {
	CreateSimpleStatusBar();
	m_StatusBar.SubclassWindow(m_hWndStatusBar);
	m_StatusBar.SetPanes(s_Panes, _countof(s_Panes), false);
	m_StatusBar.SetPaneWidth(ID_PANE_VIEW, DpiHelper::Scale(400, m_hWnd));
}

void CMainFrame::InitImageList() {
	auto size = DpiHelper::SmallIconSize(m_hWnd);
	if (m_Images)
		m_Images.Destroy();
	m_Images.Create(size, size, ILC_COLOR32 | ILC_MASK, _countof(s_Icons), 4);
	for (auto id : s_Icons)
		m_Images.AddIcon(IconHelper::LoadCached(id, size));
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

	m_StatusBar.SetBkColor(dark ? DarkMode::getBackgroundColor() : CLR_DEFAULT);

	for (auto view : m_Views)
		view->OnThemeChanged(dark);

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

IView* CMainFrame::GetView(int page) const {
	if (page < 0 || page >= m_Tabs.GetPageCount())
		return nullptr;
	auto hWnd = m_Tabs.GetPageHWND(page);
	auto it = std::ranges::find_if(m_Views, [=](auto v) { return v->GetHwnd() == hWnd; });
	return it == m_Views.end() ? nullptr : *it;
}

IView* CMainFrame::FindView(ViewType type, std::wstring const& path, int* page) const {
	for (int i = 0; i < m_Tabs.GetPageCount(); i++) {
		auto view = GetView(i);
		if (view && view->GetType() == type && _wcsicmp(view->GetPath().c_str(), path.c_str()) == 0) {
			if (page)
				*page = i;
			return view;
		}
	}
	return nullptr;
}

bool CMainFrame::AddView(IView* view, std::function<HWND(HWND)> const& create, bool activate) {
	auto hWnd = create(m_Tabs);
	if (!hWnd) {
		// never shown: the view's window doesn't exist to delete it
		delete view;
		return false;
	}
	m_Views.push_back(view);
	int previous = m_Tabs.GetActivePage();
	// the page data is what COMMAND_TABVIEW_HANDLER routes the commands to
	m_Tabs.AddPage(hWnd, view->GetTitle(), GetIconIndex(view->GetIcon()), dynamic_cast<CMessageMap*>(view));
	if (!activate && previous >= 0)
		m_Tabs.SetActivePage(previous);
	return true;
}

void CMainFrame::ForgetView(IView* view) {
	if (!view)
		return;
	if (view == m_ActiveView) {
		view->OnActivate(false);
		m_ActiveView = nullptr;
	}
	std::erase(m_Views, view);
}

void CMainFrame::CloseTab(int page) {
	ForgetView(GetView(page));
	// destroys the view's window, and the view deletes itself
	m_Tabs.RemovePage(page);
}

void CMainFrame::UpdateRecentMenus() {
	auto file = CMenuHandle(GetMenu()).GetSubMenu(0);
	auto& settings = AppSettings::Get();
	auto fill = [](CMenuHandle menu, std::vector<std::wstring> const& paths, UINT firstId) {
		while (menu.GetMenuItemCount() > 0)
			menu.DeleteMenu(0, MF_BYPOSITION);
		for (size_t i = 0; i < paths.size() && i < MaxRecent; i++)
			menu.AppendMenu(MF_STRING, firstId + (UINT)i, Helpers::RecentMenuText(i, paths[i]).c_str());
		if (menu.GetMenuItemCount() == 0)
			menu.AppendMenu(MF_STRING | MF_GRAYED, firstId, L"(None)");
	};
	// File: Open File, Open Folder, separator, Recent Files, Recent Folders
	fill(file.GetSubMenu(3), settings.RecentFiles(), ID_RECENT_FILE_FIRST);
	fill(file.GetSubMenu(4), settings.RecentFolders(), ID_RECENT_FOLDER_FIRST);
}

void CMainFrame::AddRecent(bool folder, std::wstring const& path) {
	auto& settings = AppSettings::Get();
	auto list = folder ? settings.RecentFolders() : settings.RecentFiles();
	Helpers::AddRecent(list, path, MaxRecent);
	folder ? settings.RecentFolders(list) : settings.RecentFiles(list);
	settings.Save();
	UpdateRecentMenus();
}

void CMainFrame::RemoveRecent(bool folder, std::wstring const& path) {
	auto& settings = AppSettings::Get();
	auto list = folder ? settings.RecentFolders() : settings.RecentFiles();
	if (!Helpers::RemoveRecent(list, path))
		return;
	folder ? settings.RecentFolders(list) : settings.RecentFiles(list);
	settings.Save();
	UpdateRecentMenus();
}

void CMainFrame::UpdateCaption() {
	SetWindowText(m_ActiveView ? m_ActiveView->GetTitle() + L" - " + m_Caption : m_Caption);
}

void CMainFrame::ShowError(std::wstring const& message) const {
	AtlMessageBox(m_hWnd, message.c_str(), IDR_MAINFRAME, MB_ICONERROR);
}

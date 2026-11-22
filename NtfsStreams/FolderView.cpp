#include "pch.h"
#include "FolderView.h"
#include "Helpers.h"
#include "AppSettings.h"
#include <SortHelper.h>
#include <DpiHelper.h>
#include <ToolbarHelper.h>
#include <ClipboardHelper.h>
#include <unordered_set>

namespace {
	const ToolBarButtonInfo s_BarButtons[] = {
		{ ID_FOLDER_SUBFOLDERS, IDI_SUBFOLDERS, BTNS_CHECK | BTNS_AUTOSIZE, L"Include Subfolders" },
		{ ID_VIEW_REFRESH, IDI_REFRESH, BTNS_BUTTON | BTNS_AUTOSIZE, L"Refresh" },
		{ ID_FOLDER_STOP, IDI_STOP, BTNS_BUTTON | BTNS_AUTOSIZE, L"Stop" },
	};
	const int BarImageSize = 16;	// at 96 DPI

	// lower case, for comparing paths
	std::wstring Key(std::wstring const& path) {
		std::wstring key(path);
		::CharLowerBuff(key.data(), (DWORD)key.size());
		return key;
	}
}

CFolderView::CFolderView(IMainFrame* frame, std::wstring folder) : CViewBase(frame), m_Folder(std::move(folder)),
	m_Recursive(AppSettings::Get().FolderRecursive() != 0) {
}

CString CFolderView::GetTitle() const {
	// long paths keep their end, the part that tells them apart
	constexpr size_t MaxTitle = 50;
	if (m_Folder.size() <= MaxTitle)
		return m_Folder.c_str();
	return (L"..." + m_Folder.substr(m_Folder.size() - MaxTitle)).c_str();
}

std::wstring CFolderView::GetStatusText() const {
	auto count = m_Files.size();
	auto found = std::format(L"{} file{} with streams", Helpers::FormatNumber(count), count == 1 ? L"" : L"s");
	if (m_Scanning)
		return std::format(L"Scanning... {} ({} scanned)", found, Helpers::FormatNumber(m_State->Scanned.load()));
	auto text = std::format(L"{} ({} scanned", found, Helpers::FormatNumber(m_Result.Files));
	if (m_Result.Errors)
		text += std::format(L", {} not readable", Helpers::FormatNumber(m_Result.Errors));
	if (m_Result.Canceled)
		text += L", stopped";
	return text + L")";
}

void CFolderView::OnActivate(bool active) {
	m_Active = active;
}

void CFolderView::UpdateUI(CUpdateUIBase& ui) {
	bool selected = m_List.GetSelectedCount() > 0;
	ui.UIEnable(ID_VIEW_VIEWFILES, selected);
	ui.UIEnable(ID_EDIT_COPY, selected);
	ui.UIEnable(ID_VIEW_REFRESH, true);
	ui.UIEnable(ID_FOLDER_UNBLOCK, selected);
	ui.UIEnable(ID_FOLDER_DELETESTREAMS, selected);
}

LRESULT CFolderView::OnDeleteStreams(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	constexpr std::wstring_view ZoneIdentifier = L"Zone.Identifier";
	bool all = wID == ID_FOLDER_DELETESTREAMS;
	auto paths = GetSelectedPaths();

	// the streams to go, by file (as the rows say)
	std::vector<std::pair<std::wstring, std::vector<std::wstring>>> targets;
	size_t count = 0;
	for (auto& path : paths) {
		auto it = std::ranges::find(m_Files, path, &Streams::FileStreams::Path);
		if (it == m_Files.end())
			continue;
		std::vector<std::wstring> names;
		for (auto& s : it->Streams)
			if (all || _wcsicmp(s.Name.c_str(), ZoneIdentifier.data()) == 0)
				names.push_back(s.Name);
		count += names.size();
		if (!names.empty())
			targets.emplace_back(path, std::move(names));
	}
	auto hWnd = Frame()->GetHwnd();
	if (targets.empty()) {
		AtlMessageBox(hWnd, L"None of the selected files has a Zone.Identifier stream.", IDR_MAINFRAME, MB_ICONINFORMATION);
		return 0;
	}
	auto question = all ?
		std::format(L"Delete all {} streams of {} file{}?", count, targets.size(), targets.size() == 1 ? L"" : L"s") :
		std::format(L"Remove the Zone.Identifier stream (where the file came from) from {} file{}?", targets.size(), targets.size() == 1 ? L"" : L"s");
	if (AtlMessageBox(hWnd, question.c_str(), IDR_MAINFRAME, MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON2) != IDOK)
		return 0;

	CWaitCursor wait;
	bool keepTimes = AppSettings::Get().KeepFileTimes() != 0;
	size_t failed = 0;
	std::wstring firstError;
	for (auto& [path, names] : targets) {
		Streams::FileTimesKeeper keep(path, keepTimes);
		for (auto& name : names) {
			auto error = Streams::DeleteStream(path, name);
			if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND) {
				if (failed++ == 0)
					firstError = std::format(L"{}:{}\n{}", path, name, Helpers::ErrorText(error));
			}
		}
	}

	// the rows of those files as they are now: gone if no streams are left
	for (auto& [path, names] : targets) {
		auto it = std::ranges::find(m_Files, path, &Streams::FileStreams::Path);
		auto streams = Streams::GetStreams(path);
		if (streams && !streams->empty())
			it->Streams = std::move(*streams);
		else
			m_Files.erase(it);
	}
	m_List.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	m_List.SetItemCountEx((int)m_Files.size(), LVSICF_NOSCROLL);
	m_List.Invalidate();
	UpdateStatus();

	if (failed)
		AtlMessageBox(hWnd, std::format(L"{} stream{} couldn't be deleted. The first:\n\n{}", failed, failed == 1 ? L"" : L"s", firstError).c_str(),
			IDR_MAINFRAME, MB_ICONERROR);
	return 0;
}

void CFolderView::OnDpiChanged(UINT /*dpi*/) {
	m_List.SetImageList(Frame()->GetImageList(), LVSIL_SMALL);
	ToolbarHelper::UpdateToolBarImages(m_Bar, s_BarButtons, _countof(s_BarButtons), BarImageSize);
}

CString CFolderView::GetColumnText(HWND, int row, int col) const {
	auto& file = m_Files[row];
	switch (static_cast<Column>(col)) {
		case Column::Name: return RelativeName(file.Path).c_str();
		case Column::Streams: return std::to_wstring(file.Streams.size()).c_str();
		case Column::TotalSize: return Helpers::FormatSize(file.TotalSize()).c_str();
	}
	return L"";
}

int CFolderView::GetRowImage(HWND, int row, int) const {
	return Frame()->GetIconIndex(m_Files[row].IsDirectory ? IDI_FOLDER : IDI_FILE);
}

void CFolderView::DoSort(SortInfo const* si) {
	auto selected = GetSelectedPaths();
	auto asc = si->SortAscending;
	std::ranges::stable_sort(m_Files, [&](auto& f1, auto& f2) {
		switch (static_cast<Column>(si->SortColumn)) {
			case Column::Name: return SortHelper::Sort(f1.Path, f2.Path, asc);
			case Column::Streams: return SortHelper::Sort(f1.Streams.size(), f2.Streams.size(), asc);
			case Column::TotalSize: return SortHelper::Sort(f1.TotalSize(), f2.TotalSize(), asc);
		}
		return false;
		});
	SelectPaths(selected);
}

bool CFolderView::OnDoubleClickList(HWND, int row, int, POINT const&) {
	if (row >= 0)
		OpenSelected();
	return true;
}

LRESULT CFolderView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	m_Bar = ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, s_BarButtons, _countof(s_BarButtons), BarImageSize);
	AddSimpleReBarBand(m_Bar);
	UIAddToolBar(m_Bar);

	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_List.SetImageList(Frame()->GetImageList(), LVSIL_SMALL);
	SuspendHookWhilePainting(m_List);

	auto scale = [this](int width) { return DpiHelper::Scale(width, m_hWnd); };
	m_List.InsertColumn(0, L"File Name", LVCFMT_LEFT, scale(350));
	m_List.InsertColumn(1, L"Streams", LVCFMT_RIGHT, scale(70));
	m_List.InsertColumn(2, L"Total Size", LVCFMT_RIGHT, scale(120));

	UISetCheck(ID_FOLDER_SUBFOLDERS, m_Recursive);
	StartScan();
	return 0;
}

LRESULT CFolderView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// the thread posts to this window; it's done before the window goes
	StopScan(true);
	bHandled = FALSE;
	return 0;
}

LRESULT CFolderView::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
	if (wParam != ProgressTimer) {
		bHandled = FALSE;
		return 0;
	}
	UpdateStatus();
	return 0;
}

LRESULT CFolderView::OnContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	if ((HWND)wParam != m_List) {
		bHandled = FALSE;
		return 0;
	}
	CPoint pt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	if (pt.x == -1 && pt.y == -1) {
		// from the keyboard: under the focused row (or the list's corner)
		CRect rc;
		int focused = m_List.GetNextItem(-1, LVNI_FOCUSED);
		if (focused < 0 || !m_List.GetItemRect(focused, &rc, LVIR_LABEL))
			rc.SetRect(0, 0, 0, 0);
		pt.SetPoint(rc.left, rc.bottom);
		m_List.ClientToScreen(&pt);
	}
	if (m_List.GetSelectedCount() == 0)
		return 0;

	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	CMenuHandle popup = menu.GetSubMenu(0);
	popup.SetMenuDefaultItem(ID_VIEW_VIEWFILES);
	Frame()->ShowContextMenu(popup, pt);
	return 0;
}

LRESULT CFolderView::OnScanFound(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (wParam != m_ScanId)
		return 0;
	TakePending();
	UpdateStatus();
	return 0;
}

LRESULT CFolderView::OnScanDone(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (wParam != m_ScanId)
		return 0;
	TakePending();
	{
		std::lock_guard lock(m_State->Lock);
		m_Result = m_State->Result;
	}
	m_Scanning = false;
	if (m_Scan.joinable())
		m_Scan.join();
	KillTimer(ProgressTimer);
	UIEnable(ID_FOLDER_STOP, false);
	UpdateStatus();
	return 0;
}

LRESULT CFolderView::OnReturn(int /*idCtrl*/, LPNMHDR hdr, BOOL& bHandled) {
	if (hdr->hwndFrom != m_List) {
		bHandled = FALSE;
		return 0;
	}
	OpenSelected();
	return 0;
}

LRESULT CFolderView::OnViewFiles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	OpenSelected();
	return 0;
}

LRESULT CFolderView::OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	StartScan();
	return 0;
}

LRESULT CFolderView::OnSubfolders(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_Recursive = !m_Recursive;
	UISetCheck(ID_FOLDER_SUBFOLDERS, m_Recursive);
	// the choice for the folders opened next
	auto& settings = AppSettings::Get();
	settings.FolderRecursive(m_Recursive);
	settings.Save();
	StartScan();
	return 0;
}

LRESULT CFolderView::OnStop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// WM_SCAN_DONE follows, with what was found so far
	StopScan(false);
	return 0;
}

LRESULT CFolderView::OnOpenInExplorer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int focused = m_List.GetNextItem(-1, LVNI_FOCUSED | LVNI_SELECTED);
	if (focused < 0)
		focused = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (focused < 0)
		return 0;
	// the folder the file is in, with the file selected
	if (auto pidl = ::ILCreateFromPath(m_Files[focused].Path.c_str()); pidl) {
		::SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
		::ILFree(pidl);
	}
	return 0;
}

LRESULT CFolderView::OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	std::wstring text;
	for (auto& path : GetSelectedPaths())
		text += path + L"\r\n";
	if (!text.empty())
		ClipboardHelper::CopyText(m_hWnd, text.c_str());
	return 0;
}

void CFolderView::StartScan() {
	StopScan(true);
	m_Files.clear();
	m_List.SetItemCount(0);
	m_Result = {};

	auto state = m_State = std::make_shared<ScanState>();
	m_Scanning = true;
	UIEnable(ID_FOLDER_STOP, true);
	SetTimer(ProgressTimer, 250);
	auto id = ++m_ScanId;
	m_Scan = std::jthread([state, id, folder = m_Folder, recursive = m_Recursive, hWnd = m_hWnd](std::stop_token stop) {
		Streams::ScanOptions options{ .Recursive = recursive, .Scanned = &state->Scanned };
		auto result = Streams::ScanFolder(folder, options, [&](Streams::FileStreams&& file) {
			std::lock_guard lock(state->Lock);
			state->Pending.push_back(std::move(file));
			if (!state->Posted) {
				state->Posted = true;
				::PostMessage(hWnd, WM_SCAN_FOUND, id, 0);
			}
			}, stop);
		{
			std::lock_guard lock(state->Lock);
			state->Result = result;
		}
		::PostMessage(hWnd, WM_SCAN_DONE, id, 0);
		});
	UpdateStatus();
}

void CFolderView::StopScan(bool wait) {
	if (!m_Scan.joinable())
		return;
	m_Scan.request_stop();
	if (wait) {
		m_Scan.join();
		// what it posted and is still on its way carries its id, which the next scan ignores
		m_Scanning = false;
		KillTimer(ProgressTimer);
	}
}

void CFolderView::TakePending() {
	std::vector<Streams::FileStreams> found;
	{
		std::lock_guard lock(m_State->Lock);
		found.swap(m_State->Pending);
		m_State->Posted = false;
	}
	if (found.empty())
		return;

	m_Files.insert(m_Files.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
	m_List.SetItemCountEx((int)m_Files.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
	// sorted: the new rows go to their places (DoSort keeps the selection)
	Sort(m_List);
}

void CFolderView::OpenSelected() {
	// the paths first: opening a tab deactivates this view (and may run a message loop for an error message)
	bool first = true;
	for (auto& path : GetSelectedPaths()) {
		// the first one opened is the one shown
		if (Frame()->OpenFile(path, first))
			first = false;
	}
}

std::vector<std::wstring> CFolderView::GetSelectedPaths() const {
	std::vector<std::wstring> paths;
	// the focused one first
	int focused = m_List.GetNextItem(-1, LVNI_FOCUSED | LVNI_SELECTED);
	if (focused >= 0 && focused < (int)m_Files.size())
		paths.push_back(m_Files[focused].Path);
	for (int i = m_List.GetNextItem(-1, LVNI_SELECTED); i >= 0; i = m_List.GetNextItem(i, LVNI_SELECTED))
		if (i != focused && i < (int)m_Files.size())
			paths.push_back(m_Files[i].Path);
	return paths;
}

void CFolderView::SelectPaths(std::vector<std::wstring> const& paths) {
	if (paths.empty())
		return;
	std::unordered_set<std::wstring> keys;
	for (auto& p : paths)
		keys.insert(Key(p));
	auto focused = Key(paths[0]);

	m_List.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	for (int i = 0; i < (int)m_Files.size(); i++) {
		auto key = Key(m_Files[i].Path);
		if (!keys.contains(key))
			continue;
		UINT state = LVIS_SELECTED;
		if (key == focused) {
			state |= LVIS_FOCUSED;
			m_List.SetSelectionMark(i);
		}
		m_List.SetItemState(i, state, state);
	}
}

std::wstring CFolderView::RelativeName(std::wstring const& path) const {
	if (path.size() > m_Folder.size() && _wcsnicmp(path.c_str(), m_Folder.c_str(), m_Folder.size()) == 0) {
		auto start = m_Folder.size();
		if (path[start] == L'\\')
			start++;
		return path.substr(start);
	}
	return path;
}

void CFolderView::UpdateStatus() const {
	if (m_Active)
		Frame()->SetStatusText(GetStatusText().c_str());
}

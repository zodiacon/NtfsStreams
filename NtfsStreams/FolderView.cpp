#include "pch.h"
#include "FolderView.h"
#include "Helpers.h"
#include <SortHelper.h>
#include <DpiHelper.h>

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
		return L"Scanning... " + found;
	auto text = std::format(L"{} ({} scanned", found, Helpers::FormatNumber(m_Result.Files));
	if (m_Result.Errors)
		text += std::format(L", {} not readable", Helpers::FormatNumber(m_Result.Errors));
	if (m_Result.Canceled)
		text += L", canceled";
	return text + L")";
}

void CFolderView::OnActivate(bool active) {
	m_Active = active;
}

void CFolderView::UpdateUI(CUpdateUIBase& ui) {
	ui.UIEnable(ID_VIEW_VIEWFILES, m_List.GetSelectedCount() > 0);
}

void CFolderView::OnDpiChanged(UINT /*dpi*/) {
	m_List.SetImageList(Frame()->GetImageList(), LVSIL_SMALL);
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
	auto asc = si->SortAscending;
	std::ranges::stable_sort(m_Files, [&](auto& f1, auto& f2) {
		switch (static_cast<Column>(si->SortColumn)) {
			case Column::Name: return SortHelper::Sort(f1.Path, f2.Path, asc);
			case Column::Streams: return SortHelper::Sort(f1.Streams.size(), f2.Streams.size(), asc);
			case Column::TotalSize: return SortHelper::Sort(f1.TotalSize(), f2.TotalSize(), asc);
		}
		return false;
		});
}

bool CFolderView::OnDoubleClickList(HWND, int row, int, POINT const&) {
	if (row >= 0)
		OpenSelected();
	return true;
}

LRESULT CFolderView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_List.SetImageList(Frame()->GetImageList(), LVSIL_SMALL);
	SuspendHookWhilePainting(m_List);

	auto scale = [this](int width) { return DpiHelper::Scale(width, m_hWnd); };
	m_List.InsertColumn(0, L"File Name", LVCFMT_LEFT, scale(350));
	m_List.InsertColumn(1, L"Streams", LVCFMT_RIGHT, scale(70));
	m_List.InsertColumn(2, L"Total Size", LVCFMT_RIGHT, scale(120));

	StartScan();
	return 0;
}

LRESULT CFolderView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// the thread posts to this window; it's done before the window goes
	if (m_Scan.joinable()) {
		m_Scan.request_stop();
		m_Scan.join();
	}
	bHandled = FALSE;
	return 0;
}

void CFolderView::StartScan() {
	m_State = std::make_shared<ScanState>();
	m_Scanning = true;
	m_Scan = std::jthread([state = m_State, folder = m_Folder, hWnd = m_hWnd](std::stop_token stop) {
		auto result = Streams::ScanFolder(folder, {}, [&](Streams::FileStreams&& file) {
			std::lock_guard lock(state->Lock);
			state->Pending.push_back(std::move(file));
			if (!state->Posted) {
				state->Posted = true;
				::PostMessage(hWnd, WM_SCAN_FOUND, 0, 0);
			}
			}, stop);
		{
			std::lock_guard lock(state->Lock);
			state->Result = result;
		}
		::PostMessage(hWnd, WM_SCAN_DONE, 0, 0);
		});
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
	Sort(m_List);
}

LRESULT CFolderView::OnScanFound(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	TakePending();
	UpdateStatus();
	return 0;
}

LRESULT CFolderView::OnScanDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	TakePending();
	{
		std::lock_guard lock(m_State->Lock);
		m_Result = m_State->Result;
	}
	m_Scanning = false;
	m_Scan.join();
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

void CFolderView::OpenSelected() {
	// the paths first: opening a tab deactivates this view (and may run a message loop for an error message)
	std::vector<std::wstring> paths;
	for (int i = m_List.GetNextItem(-1, LVNI_SELECTED); i >= 0; i = m_List.GetNextItem(i, LVNI_SELECTED))
		paths.push_back(m_Files[i].Path);

	bool first = true;
	for (auto& path : paths) {
		// the first one opened is the one shown
		if (Frame()->OpenFile(path, first))
			first = false;
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

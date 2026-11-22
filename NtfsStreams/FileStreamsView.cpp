#include "pch.h"
#include "FileStreamsView.h"
#include "Helpers.h"
#include <SortHelper.h>
#include <DpiHelper.h>
#include <ClipboardHelper.h>

CString CFileStreamsView::GetTitle() const {
	return Helpers::FileName(m_File.Path).c_str();
}

std::wstring CFileStreamsView::GetStatusText() const {
	if (!m_Error.empty())
		return m_Error;
	auto count = m_File.Streams.size();
	return std::format(L"{} stream{}, {} in all", count, count == 1 ? L"" : L"s", Helpers::FormatSize(m_File.TotalSize()));
}

void CFileStreamsView::UpdateUI(CUpdateUIBase& ui) {
	ui.UIEnable(ID_VIEW_REFRESH, true);
	ui.UIEnable(ID_EDIT_COPY, m_List.GetSelectedCount() > 0);
}

LRESULT CFileStreamsView::OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	Refresh();
	return 0;
}

LRESULT CFileStreamsView::OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// the selected streams' names
	std::wstring text;
	for (int i = m_List.GetNextItem(-1, LVNI_SELECTED); i >= 0; i = m_List.GetNextItem(i, LVNI_SELECTED))
		text += m_File.Streams[i].Name + L"\r\n";
	if (!text.empty())
		ClipboardHelper::CopyText(m_hWnd, text.c_str());
	return 0;
}

void CFileStreamsView::Refresh() {
	int selected = m_List.GetNextItem(-1, LVNI_SELECTED);
	auto name = selected >= 0 ? m_File.Streams[selected].Name : std::wstring();

	auto streams = Streams::GetStreams(m_File.Path);
	if (streams) {
		m_File.Streams = std::move(*streams);
		m_Error.clear();
	}
	else {
		m_File.Streams.clear();
		m_Error = Helpers::ErrorText(streams.error());
	}
	m_List.SetItemCountEx((int)m_File.Streams.size(), LVSICF_NOSCROLL);
	Sort(m_List);

	auto it = std::ranges::find(m_File.Streams, name, &Streams::StreamInfo::Name);
	int index = it == m_File.Streams.end() ? (m_File.Streams.empty() ? -1 : 0) : int(it - m_File.Streams.begin());
	m_List.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	if (index >= 0) {
		m_List.SetItemState(index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_List.EnsureVisible(index, FALSE);
	}
	m_List.Invalidate();
	if (m_Active)
		Frame()->SetStatusText(GetStatusText().c_str());
}

void CFileStreamsView::OnDpiChanged(UINT /*dpi*/) {
	m_List.SetImageList(Frame()->GetImageList(), LVSIL_SMALL);
}

CString CFileStreamsView::GetColumnText(HWND, int row, int col) const {
	auto& stream = m_File.Streams[row];
	switch (static_cast<Column>(col)) {
		case Column::Name: return stream.Name.c_str();
		case Column::Size: return Helpers::FormatSize(stream.Size).c_str();
	}
	return L"";
}

void CFileStreamsView::DoSort(SortInfo const* si) {
	auto asc = si->SortAscending;
	std::ranges::sort(m_File.Streams, [&](auto& s1, auto& s2) {
		switch (static_cast<Column>(si->SortColumn)) {
			case Column::Name: return SortHelper::Sort(s1.Name, s2.Name, asc);
			case Column::Size: return SortHelper::Sort(s1.Size, s2.Size, asc);
		}
		return false;
		});
}

LRESULT CFileStreamsView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_SHAREIMAGELISTS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	SuspendHookWhilePainting(m_List);

	auto scale = [this](int width) { return DpiHelper::Scale(width, m_hWnd); };
	m_List.InsertColumn(0, L"Stream Name", LVCFMT_LEFT, scale(250));
	m_List.InsertColumn(1, L"Size", LVCFMT_RIGHT, scale(120));

	m_List.SetItemCount((int)m_File.Streams.size());
	if (!m_File.Streams.empty())
		m_List.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	return 0;
}

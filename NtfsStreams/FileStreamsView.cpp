#include "pch.h"
#include "FileStreamsView.h"
#include "Helpers.h"
#include <SortHelper.h>
#include <DpiHelper.h>

CString CFileStreamsView::GetTitle() const {
	return Helpers::FileName(m_File.Path).c_str();
}

std::wstring CFileStreamsView::GetStatusText() const {
	auto count = m_File.Streams.size();
	return std::format(L"{} stream{}, {} in all", count, count == 1 ? L"" : L"s", Helpers::FormatSize(m_File.TotalSize()));
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

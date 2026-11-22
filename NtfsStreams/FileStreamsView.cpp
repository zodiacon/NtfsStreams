#include "pch.h"
#include "FileStreamsView.h"
#include "Helpers.h"
#include "AppSettings.h"
#include <SortHelper.h>
#include <DpiHelper.h>
#include <ClipboardHelper.h>
#include <ToolbarHelper.h>

namespace {
	// no images: text only, each a drop-down but Export
	const ToolBarButtonInfo s_BarButtons[] = {
		{ ID_HEX_BYTESPERLINE, 0, BTNS_WHOLEDROPDOWN | BTNS_AUTOSIZE, L"16 Bytes per Line" },
		{ ID_HEX_DATASIZE, 0, BTNS_WHOLEDROPDOWN | BTNS_AUTOSIZE, L"Byte" },
		{ ID_TEXT_ENCODING, 0, BTNS_WHOLEDROPDOWN | BTNS_AUTOSIZE, L"Text: Auto" },
		{ 0 },
		{ ID_STREAM_EXPORT, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Export..." },
	};

	// the context menu's submenus (IDR_CONTEXT)
	enum ContextMenu { FolderMenu, StreamMenu, BytesPerLineMenu, DataSizeMenu, EncodingMenu };

	PCWSTR DataSizeName(int size) {
		switch (size) {
			case 2: return L"Word";
			case 4: return L"DWord";
			case 8: return L"QWord";
		}
		return L"Byte";
	}

	int Clamp(int value, std::initializer_list<int> allowed, int fallback) {
		return std::ranges::find(allowed, value) != allowed.end() ? value : fallback;
	}
}

CString CFileStreamsView::GetTitle() const {
	return Helpers::FileName(m_File.Path).c_str();
}

std::wstring CFileStreamsView::GetStatusText() const {
	if (!m_Error.empty())
		return m_Error;
	auto count = m_File.Streams.size();
	auto text = std::format(L"{} stream{}, {} in all", count, count == 1 ? L"" : L"s", Helpers::FormatSize(m_File.TotalSize()));
	if (m_Shown >= 0 && m_StreamError.empty() && m_Encoding == TextDecode::Encoding::Auto && !m_TextData.empty())
		text += std::format(L" | text: {}", TextDecode::Name(TextDecode::Detect(m_TextData)));
	return text;
}

void CFileStreamsView::UpdateUI(CUpdateUIBase& ui) {
	ui.UIEnable(ID_VIEW_REFRESH, true);
	bool stream = GetSelectedRow() >= 0;
	ui.UIEnable(ID_STREAM_EXPORT, stream);
	UIEnable(ID_STREAM_EXPORT, stream);
	auto focus = ::GetFocus();
	if (focus == m_Hex)
		ui.UIEnable(ID_EDIT_COPY, m_Hex.CanCopy());
	else if (focus == m_Text) {
		int start, end;
		m_Text.GetSel(start, end);
		ui.UIEnable(ID_EDIT_COPY, start != end);
	}
	else
		ui.UIEnable(ID_EDIT_COPY, stream);
}

void CFileStreamsView::OnThemeChanged(bool dark) {
	SetHexColors(dark);
	m_Hex.Invalidate();
}

void CFileStreamsView::OnDpiChanged(UINT /*dpi*/) {
	SetTextFont();
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
	int selected = GetSelectedRow();
	auto name = selected >= 0 ? m_File.Streams[selected].Name : std::wstring();
	auto asc = si->SortAscending;
	std::ranges::sort(m_File.Streams, [&](auto& s1, auto& s2) {
		switch (static_cast<Column>(si->SortColumn)) {
			case Column::Name: return SortHelper::Sort(s1.Name, s2.Name, asc);
			case Column::Size: return SortHelper::Sort(s1.Size, s2.Size, asc);
		}
		return false;
		});
	// the same stream stays selected (and shown)
	if (selected >= 0) {
		auto it = std::ranges::find(m_File.Streams, name, &Streams::StreamInfo::Name);
		int row = int(it - m_File.Streams.begin());
		m_Shown = row;
		m_List.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
		m_List.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_List.EnsureVisible(row, FALSE);
	}
}

void CFileStreamsView::OnStateChanged(HWND hList, int from, int to, UINT oldState, UINT newState) {
	if (hList != m_List || IsSorting())
		return;
	if ((oldState ^ newState) & LVIS_SELECTED) {
		int row = GetSelectedRow();
		if (row != m_Shown)
			ShowStream(row);
	}
}

LRESULT CFileStreamsView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	auto& settings = AppSettings::Get();
	m_BytesPerLine = Clamp(settings.HexBytesPerLine(), { 8, 16, 32 }, 16);
	m_DataSize = Clamp(settings.HexDataSize(), { 1, 2, 4, 8 }, 1);
	m_Encoding = (TextDecode::Encoding)Clamp(settings.TextEncoding(), { 0, 1, 2, 3 }, 0);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	m_Bar = ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, s_BarButtons, _countof(s_BarButtons), 16);
	AddSimpleReBarBand(m_Bar);
	UIAddToolBar(m_Bar);

	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_List.Create(m_Splitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_SHAREIMAGELISTS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	SuspendHookWhilePainting(m_List);

	auto scale = [this](int width) { return DpiHelper::Scale(width, m_hWnd); };
	m_List.InsertColumn(0, L"Stream Name", LVCFMT_LEFT, scale(180));
	m_List.InsertColumn(1, L"Size", LVCFMT_RIGHT, scale(100));

	m_Panes.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_Hex.Create(m_Panes, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP);
	m_Hex.SetReadOnly(true);
	m_Hex.SetOptions(HexControlOptions::DisableUndoRedo | HexControlOptions::GrayOutZeros);
	m_Hex.SetBytesPerLine(m_BytesPerLine);
	m_Hex.SetDataSize(m_DataSize);
	SetHexColors(WTLHelper::IsDarkMode());

	m_Text.Create(m_Panes, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
		ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL);
	m_Text.SetLimitText(0);
	SetTextFont();

	m_Panes.SetSplitterPanes(m_Hex, m_Text);
	m_Splitter.SetSplitterPanes(m_List, m_Panes);
	m_Splitter.SetSplitterPosPct(std::clamp(settings.StreamListPos(), 10, 80), false);
	m_Panes.SetSplitterPosPct(std::clamp(settings.HexPanePos(), 10, 90), false);

	UpdateBarText();
	m_List.SetItemCount((int)m_File.Streams.size());
	if (!m_File.Streams.empty())
		m_List.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	else
		ShowStream(-1);
	return 0;
}

LRESULT CFileStreamsView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	auto& settings = AppSettings::Get();
	settings.StreamListPos(m_Splitter.GetSplitterPosPct());
	settings.HexPanePos(m_Panes.GetSplitterPosPct());
	// the hex view goes with the window; it mustn't paint from a freed buffer meanwhile
	m_Hex.SetBufferManager(nullptr);
	bHandled = FALSE;
	return 0;
}

LRESULT CFileStreamsView::OnContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	if ((HWND)wParam != m_List) {
		bHandled = FALSE;
		return 0;
	}
	int row = GetSelectedRow();
	if (row < 0)
		return 0;
	CPoint pt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	if (pt.x == -1 && pt.y == -1) {
		CRect rc;
		m_List.GetItemRect(row, &rc, LVIR_LABEL);
		pt.SetPoint(rc.left, rc.bottom);
		m_List.ClientToScreen(&pt);
	}
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	Frame()->ShowContextMenu(menu.GetSubMenu(StreamMenu), pt);
	return 0;
}

LRESULT CFileStreamsView::OnDropDown(int /*idCtrl*/, LPNMHDR hdr, BOOL& bHandled) {
	if (hdr->hwndFrom != m_Bar) {
		bHandled = FALSE;
		return 0;
	}
	ShowDropDown(reinterpret_cast<NMTOOLBAR*>(hdr)->iItem);
	return TBDDRET_DEFAULT;
}

LRESULT CFileStreamsView::OnDropDownButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// from the keyboard (the bar's buttons drop down on a click: TBN_DROPDOWN)
	ShowDropDown(wID);
	return 0;
}

LRESULT CFileStreamsView::OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	Refresh();
	return 0;
}

LRESULT CFileStreamsView::OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto focus = ::GetFocus();
	if (focus == m_Hex) {
		m_Hex.Copy();
		return 0;
	}
	if (focus == m_Text) {
		m_Text.Copy();
		return 0;
	}
	// the selected stream's name
	if (int row = GetSelectedRow(); row >= 0)
		ClipboardHelper::CopyText(m_hWnd, m_File.Streams[row].Name.c_str());
	return 0;
}

LRESULT CFileStreamsView::OnExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int row = GetSelectedRow();
	if (row < 0)
		return 0;
	auto& name = m_File.Streams[row].Name;

	CShellFileSaveDialog dlg(Streams::ExportFileName(m_File.Path, name).c_str(),
		FOS_OVERWRITEPROMPT | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
	dlg.GetPtr()->SetTitle(std::format(L"Export Stream {}", name).c_str());
	if (dlg.DoModal(Frame()->GetHwnd()) != IDOK)
		return 0;
	CString target;
	if (FAILED(dlg.GetFilePath(target)))
		return 0;

	CWaitCursor wait;
	if (auto error = Streams::ExportStream(m_File.Path, name, (PCWSTR)target); error != ERROR_SUCCESS)
		AtlMessageBox(Frame()->GetHwnd(), std::format(L"Export failed.\n\n{}", Helpers::ErrorText(error)).c_str(), IDR_MAINFRAME, MB_ICONERROR);
	else
		Frame()->SetStatusText(std::format(L"Exported {} to {}", name, (PCWSTR)target).c_str());
	return 0;
}

void CFileStreamsView::Refresh() {
	int selected = GetSelectedRow();
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
	// the stream is read again whichever row it lands on
	m_Shown = -1;
	SetIsSorting(true);
	m_List.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	m_List.SetItemCountEx((int)m_File.Streams.size(), LVSICF_NOSCROLL);
	Sort(m_List);
	SetIsSorting(false);

	auto it = std::ranges::find(m_File.Streams, name, &Streams::StreamInfo::Name);
	int index = it == m_File.Streams.end() ? (m_File.Streams.empty() ? -1 : 0) : int(it - m_File.Streams.begin());
	if (index >= 0) {
		m_List.SetItemState(index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_List.EnsureVisible(index, FALSE);
	}
	else {
		ShowStream(-1);
	}
	m_List.Invalidate();
	UpdateStatus();
}

void CFileStreamsView::ShowStream(int row) {
	m_Shown = row;
	m_StreamError.clear();
	m_TextData.clear();

	std::unique_ptr<IBufferManager> buffer;
	if (row >= 0) {
		auto& stream = m_File.Streams[row];
		DWORD error;
		buffer = StreamBuffer::Open(m_File.Path, stream.Name, error);
		if (buffer) {
			if (auto data = Streams::ReadStream(m_File.Path, stream.Name, TextLimit))
				m_TextData = std::move(*data);
		}
		else {
			m_StreamError = Helpers::ErrorText(error);
		}
	}
	if (!buffer)
		buffer = std::make_unique<MemoryBuffer>();
	// the new buffer is the hex view's before the old one goes
	m_Hex.SetBufferManager(buffer.get());
	m_Buffer = std::move(buffer);
	ShowText();
	UpdateStatus();
}

void CFileStreamsView::ShowText() {
	std::wstring text;
	if (!m_StreamError.empty())
		text = L"The stream can't be read: " + m_StreamError;
	else {
		text = TextDecode::Decode(m_TextData, m_Encoding);
		if (m_Buffer && m_Buffer->GetSize() > (int64_t)m_TextData.size())
			text += std::format(L"\r\n\r\n[the first {} of {} shown]", Helpers::FormatSize(m_TextData.size()), Helpers::FormatSize(m_Buffer->GetSize()));
	}
	m_Text.SetWindowText(text.c_str());
}

int CFileStreamsView::GetSelectedRow() const {
	return m_List.GetNextItem(-1, LVNI_SELECTED);
}

void CFileStreamsView::ShowDropDown(UINT buttonId) {
	int submenu;
	UINT checked;
	switch (buttonId) {
		case ID_HEX_BYTESPERLINE:
			submenu = BytesPerLineMenu;
			checked = m_BytesPerLine == 8 ? ID_HEX_LINE8 : m_BytesPerLine == 32 ? ID_HEX_LINE32 : ID_HEX_LINE16;
			break;
		case ID_HEX_DATASIZE:
			submenu = DataSizeMenu;
			checked = ID_HEX_SIZE1 + (m_DataSize == 2 ? 1 : m_DataSize == 4 ? 2 : m_DataSize == 8 ? 3 : 0);
			break;
		case ID_TEXT_ENCODING:
			submenu = EncodingMenu;
			checked = ID_TEXT_AUTO + (UINT)m_Encoding;
			break;
		default:
			return;
	}
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	CMenuHandle popup = menu.GetSubMenu(submenu);
	// the items are consecutive command IDs, in the menu's order
	auto first = popup.GetMenuItemID(0), last = popup.GetMenuItemID(popup.GetMenuItemCount() - 1);
	::CheckMenuRadioItem(popup, first, last, checked, MF_BYCOMMAND);

	auto pt = ToolbarHelper::GetDropdownMenuPoint(m_Bar, buttonId);
	auto id = (UINT)popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, m_hWnd);
	switch (id) {
		case ID_HEX_LINE8: SetBytesPerLine(8); break;
		case ID_HEX_LINE16: SetBytesPerLine(16); break;
		case ID_HEX_LINE32: SetBytesPerLine(32); break;
		case ID_HEX_SIZE1: SetDataSize(1); break;
		case ID_HEX_SIZE2: SetDataSize(2); break;
		case ID_HEX_SIZE4: SetDataSize(4); break;
		case ID_HEX_SIZE8: SetDataSize(8); break;
		case ID_TEXT_AUTO: case ID_TEXT_ASCII: case ID_TEXT_UTF8: case ID_TEXT_UTF16:
			SetEncoding((TextDecode::Encoding)(id - ID_TEXT_AUTO));
			break;
	}
}

void CFileStreamsView::SetBytesPerLine(int bytes) {
	m_BytesPerLine = bytes;
	m_Hex.SetBytesPerLine(bytes);
	auto& settings = AppSettings::Get();
	settings.HexBytesPerLine(bytes);
	settings.Save();
	UpdateBarText();
}

void CFileStreamsView::SetDataSize(int size) {
	m_DataSize = size;
	m_Hex.SetDataSize(size);
	auto& settings = AppSettings::Get();
	settings.HexDataSize(size);
	settings.Save();
	UpdateBarText();
}

void CFileStreamsView::SetEncoding(TextDecode::Encoding encoding) {
	m_Encoding = encoding;
	auto& settings = AppSettings::Get();
	settings.TextEncoding((int)encoding);
	settings.Save();
	UpdateBarText();
	ShowText();
	UpdateStatus();
}

void CFileStreamsView::UpdateBarText() {
	auto setText = [this](UINT id, std::wstring const& text) {
		TBBUTTONINFO info{ sizeof(info), TBIF_TEXT };
		info.pszText = const_cast<PWSTR>(text.c_str());
		m_Bar.SetButtonInfo(id, &info);
	};
	setText(ID_HEX_BYTESPERLINE, std::format(L"{} Bytes per Line", m_BytesPerLine));
	setText(ID_HEX_DATASIZE, DataSizeName(m_DataSize));
	setText(ID_TEXT_ENCODING, std::format(L"Text: {}", TextDecode::Name(m_Encoding)));
	m_Bar.AutoSize();

	// the band as wide as the buttons now are
	CReBarCtrl rebar(m_hWndToolBar);
	CRect rc;
	m_Bar.GetItemRect(m_Bar.GetButtonCount() - 1, &rc);
	for (UINT i = 0; i < rebar.GetBandCount(); i++) {
		REBARBANDINFO band{ sizeof(band), RBBIM_CHILD };
		if (rebar.GetBandInfo(i, &band) && band.hwndChild == m_Bar) {
			band.fMask = RBBIM_CHILDSIZE | RBBIM_IDEALSIZE;
			band.cxMinChild = rc.right;
			band.cyMinChild = rc.Height();
			band.cxIdeal = rc.right;
			rebar.SetBandInfo(i, &band);
		}
	}
}

void CFileStreamsView::SetHexColors(bool dark) {
	auto& colors = m_Hex.GetColors();
	colors = HexControlColors{};
	if (dark) {
		// the defaults are dark red and blue, for a light background
		colors.Text = DarkMode::getTextColor();
		colors.Background = DarkMode::getViewBackgroundColor();
		colors.Ascii = RGB(206, 145, 120);
		colors.Offset = colors.Ruler = RGB(86, 156, 214);
	}
}

void CFileStreamsView::SetTextFont() {
	if (!m_TextFont.IsNull())
		m_TextFont.DeleteObject();
	auto dpi = DpiHelper::GetDpi(m_hWnd);
	m_TextFont.CreateFont(-::MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
	m_Text.SetFont(m_TextFont);
}

void CFileStreamsView::UpdateStatus() const {
	if (m_Active)
		Frame()->SetStatusText(GetStatusText().c_str());
}

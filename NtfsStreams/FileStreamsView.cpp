#include "pch.h"
#include "FileStreamsView.h"
#include "Helpers.h"
#include "AppSettings.h"
#include "StreamDialogs.h"
#include <SortHelper.h>
#include <DpiHelper.h>
#include <ClipboardHelper.h>
#include <ToolbarHelper.h>

namespace {
	// no images: text only; the first three drop down
	const ToolBarButtonInfo s_BarButtons[] = {
		{ ID_HEX_BYTESPERLINE, 0, BTNS_WHOLEDROPDOWN | BTNS_AUTOSIZE, L"16 Bytes per Line" },
		{ ID_HEX_DATASIZE, 0, BTNS_WHOLEDROPDOWN | BTNS_AUTOSIZE, L"Byte" },
		{ ID_TEXT_ENCODING, 0, BTNS_WHOLEDROPDOWN | BTNS_AUTOSIZE, L"Text: Auto" },
		{ 0 },
		{ ID_STREAM_NEW, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"New..." },
		{ ID_STREAM_EDITTEXT, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Edit Text..." },
		{ ID_STREAM_EDITBYTES, 0, BTNS_CHECK | BTNS_AUTOSIZE, L"Edit Bytes" },
		{ ID_STREAM_SAVE, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Save" },
		{ ID_STREAM_DISCARD, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Discard" },
		{ 0 },
		{ ID_STREAM_EXPORT, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Export..." },
	};

	int Ask(HWND hWnd, std::wstring const& text, UINT type) {
		return AtlMessageBox(hWnd, text.c_str(), IDR_MAINFRAME, type);
	}

	// stream names compare without case, as NTFS does
	bool SameName(std::wstring const& a, std::wstring const& b) {
		return _wcsicmp(a.c_str(), b.c_str()) == 0;
	}

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
	if (m_Editing)
		return std::format(L"Editing {}{} ({}) - Insert switches between inserting and overwriting; Save Changes (Ctrl+S) writes them",
			m_File.Streams[m_Shown].Name, m_Changed ? L" (changed)" : L"", Helpers::FormatSize(m_Buffer->GetSize()));
	auto count = m_File.Streams.size();
	auto text = std::format(L"{} stream{}, {} in all", count, count == 1 ? L"" : L"s", Helpers::FormatSize(m_File.TotalSize()));
	if (m_Shown >= 0 && m_StreamError.empty() && m_Encoding == TextDecode::Encoding::Auto && !m_TextData.empty())
		text += std::format(L" | text: {}", TextDecode::Name(TextDecode::Detect(m_TextData)));
	return text;
}

void CFileStreamsView::UpdateUI(CUpdateUIBase& ui) {
	bool stream = GetSelectedRow() >= 0;
	// the frame's menus and this view's tool bar alike
	auto enable = [&](UINT id, bool enabled) {
		ui.UIEnable(id, enabled);
		UIEnable(id, enabled);
	};
	enable(ID_VIEW_REFRESH, true);
	enable(ID_STREAM_EXPORT, stream && !m_Editing);
	enable(ID_STREAM_NEW, !m_Editing);
	enable(ID_STREAM_IMPORT, !m_Editing);
	enable(ID_STREAM_EDITTEXT, stream && !m_Editing);
	enable(ID_STREAM_EDITBYTES, stream);
	enable(ID_STREAM_SAVE, m_Editing && IsDirty());
	enable(ID_STREAM_DISCARD, m_Editing);
	enable(ID_STREAM_RENAME, stream && !m_Editing);
	enable(ID_STREAM_DELETE, stream && !m_Editing);
	ui.UISetCheck(ID_STREAM_EDITBYTES, m_Editing);
	UISetCheck(ID_STREAM_EDITBYTES, m_Editing);
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
	if (hList != m_List || IsSorting() || m_Restoring)
		return;
	if ((oldState ^ newState) & LVIS_SELECTED) {
		int row = GetSelectedRow();
		if (row == m_Shown)
			return;
		// edits to the shown stream: saved, discarded, or the selection goes back to it
		if (m_Editing && !ConfirmStopEditing()) {
			m_Restoring = true;
			m_List.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			m_List.SetItemState(m_Shown, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_Restoring = false;
			return;
		}
		// saving refreshed the list: the row may be another now
		ShowStream(GetSelectedRow());
	}
}

bool CFileStreamsView::CanClose() {
	return ConfirmStopEditing();
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

void CFileStreamsView::Refresh(std::wstring const& select) {
	if (!ConfirmStopEditing())
		return;
	int selected = GetSelectedRow();
	auto name = !select.empty() ? select : selected >= 0 ? m_File.Streams[selected].Name : std::wstring();

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

	auto it = std::ranges::find_if(m_File.Streams, [&](auto& s) { return SameName(s.Name, name); });
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

LRESULT CFileStreamsView::OnListKeyDown(int /*idCtrl*/, LPNMHDR hdr, BOOL& bHandled) {
	if (hdr->hwndFrom != m_List) {
		bHandled = FALSE;
		return 0;
	}
	// in the list only: the keys mean other things in the hex view and the text pane
	BOOL handled;
	switch (reinterpret_cast<NMLVKEYDOWN*>(hdr)->wVKey) {
		case VK_DELETE:
			if (!m_Editing)
				OnDelete(0, ID_STREAM_DELETE, nullptr, handled);
			break;
		case VK_F2:
			if (!m_Editing)
				OnRename(0, ID_STREAM_RENAME, nullptr, handled);
			break;
	}
	return 0;
}

bool CFileStreamsView::StreamExists(std::wstring const& name) const {
	return std::ranges::any_of(m_File.Streams, [&](auto& s) { return SameName(s.Name, name); });
}

bool CFileStreamsView::Change(std::function<DWORD()> const& change, PCWSTR what) {
	DWORD error;
	{
		Streams::FileTimesKeeper keep(m_File.Path, AppSettings::Get().KeepFileTimes() != 0);
		error = change();
	}
	if (error == ERROR_SUCCESS)
		return true;
	Ask(Frame()->GetHwnd(), std::format(L"{} failed.\n\n{}", what, Helpers::ErrorText(error)), MB_ICONERROR);
	return false;
}

LRESULT CFileStreamsView::OnNewStream(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CTextStreamDlg dlg(L"New Stream", L"", false, L"", TextDecode::Encoding::Utf8, false, [this](auto& name) {
		return StreamExists(name) ? std::wstring(L"The file has a stream with this name (Edit Text changes it).") : std::wstring();
		});
	if (dlg.DoModal(Frame()->GetHwnd()) != IDOK)
		return 0;
	auto data = TextDecode::Encode(dlg.GetText(), dlg.GetEncoding(), dlg.GetBom());
	if (Change([&] { return Streams::WriteStream(m_File.Path, dlg.GetName(), data); }, L"Adding the stream"))
		Refresh(dlg.GetName());
	return 0;
}

LRESULT CFileStreamsView::OnImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CShellFileOpenDialog open(nullptr, FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
	open.GetPtr()->SetTitle(L"Import File as Stream");
	if (open.DoModal(Frame()->GetHwnd()) != IDOK)
		return 0;
	CString source;
	if (FAILED(open.GetFilePath(source)))
		return 0;

	// the file's name, as a stream may have it
	auto name = Helpers::FileName((PCWSTR)source);
	std::ranges::replace(name, L':', L'_');
	CNameDlg dlg(L"Import File as Stream", std::format(L"The stream for {}:", (PCWSTR)source), name);
	if (dlg.DoModal(Frame()->GetHwnd()) != IDOK)
		return 0;
	if (StreamExists(dlg.GetName()) &&
		Ask(Frame()->GetHwnd(), std::format(L"Replace the stream {} with the file's contents?", dlg.GetName()), MB_ICONQUESTION | MB_OKCANCEL) != IDOK)
		return 0;

	CWaitCursor wait;
	if (Change([&] { return Streams::ImportFile(m_File.Path, dlg.GetName(), (PCWSTR)source); }, L"Importing the file"))
		Refresh(dlg.GetName());
	return 0;
}

LRESULT CFileStreamsView::OnEditText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int row = GetSelectedRow();
	if (row < 0 || m_Editing)
		return 0;
	auto name = m_File.Streams[row].Name;
	auto hWnd = Frame()->GetHwnd();
	if (m_File.Streams[row].Size > TextEditLimit) {
		Ask(hWnd, std::format(L"The stream is too large to edit as text (more than {}).", Helpers::FormatSize(TextEditLimit)), MB_ICONINFORMATION);
		return 0;
	}
	auto data = Streams::ReadStream(m_File.Path, name);
	if (!data) {
		Ask(hWnd, Helpers::ErrorText(data.error()), MB_ICONERROR);
		return 0;
	}

	auto encoding = m_Encoding == TextDecode::Encoding::Auto ? TextDecode::Detect(*data) : m_Encoding;
	size_t replaced = 0;
	auto text = TextDecode::Decode(*data, encoding, &replaced);
	if (replaced) {
		// written back, those characters would be '.'
		Ask(hWnd, std::format(L"The stream has {} byte{} that aren't text in {}. Editing it as text would change them: use Edit Bytes, "
			L"or pick another encoding (Text: on the tool bar).", replaced, replaced == 1 ? L"" : L"s", TextDecode::Name(encoding)), MB_ICONINFORMATION);
		return 0;
	}
	// line breaks are CR LF in the editor; a stream that had LF alone keeps it
	bool lfOnly = std::ranges::find(*data, '\r') == data->end() && std::ranges::find(*data, '\n') != data->end();

	CTextStreamDlg dlg(std::format(L"Edit Text: {}", name), name, true, text, encoding, TextDecode::HasBom(*data));
	if (dlg.DoModal(hWnd) != IDOK)
		return 0;
	auto edited = dlg.GetText();
	if (lfOnly) {
		std::wstring lf;
		lf.reserve(edited.size());
		for (size_t i = 0; i < edited.size(); i++)
			if (!(edited[i] == L'\r' && i + 1 < edited.size() && edited[i + 1] == L'\n'))
				lf += edited[i];
		edited = std::move(lf);
	}
	auto bytes = TextDecode::Encode(edited, dlg.GetEncoding(), dlg.GetBom());
	if (Change([&] { return Streams::WriteStream(m_File.Path, name, bytes); }, L"Writing the stream"))
		Refresh(name);
	return 0;
}

LRESULT CFileStreamsView::OnEditBytes(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_Editing)
		ConfirmStopEditing();
	else
		StartEditing();
	return 0;
}

LRESULT CFileStreamsView::OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_Editing && SaveEdits())
		StopEditing();
	return 0;
}

LRESULT CFileStreamsView::OnDiscard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_Editing)
		StopEditing();
	return 0;
}

LRESULT CFileStreamsView::OnRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int row = GetSelectedRow();
	if (row < 0 || m_Editing)
		return 0;
	auto name = m_File.Streams[row].Name;
	CNameDlg dlg(L"Rename Stream", std::format(L"The new name for {}:", name), name, [&](auto& newName) {
		// another case of its own name is a rename too
		return !SameName(newName, name) && StreamExists(newName) ? std::wstring(L"The file has a stream with this name.") : std::wstring();
		});
	if (dlg.DoModal(Frame()->GetHwnd()) != IDOK || dlg.GetName() == name)
		return 0;
	if (Change([&] { return Streams::RenameStream(m_File.Path, name, dlg.GetName()); }, L"Renaming the stream"))
		Refresh(dlg.GetName());
	return 0;
}

LRESULT CFileStreamsView::OnDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int row = GetSelectedRow();
	if (row < 0 || m_Editing)
		return 0;
	auto name = m_File.Streams[row].Name;
	if (Ask(Frame()->GetHwnd(), std::format(L"Delete the stream {} ({})?", name, Helpers::FormatSize(m_File.Streams[row].Size)),
		MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON2) != IDOK)
		return 0;
	// the next one in the list gets the selection
	auto next = row + 1 < (int)m_File.Streams.size() ? m_File.Streams[row + 1].Name : row > 0 ? m_File.Streams[row - 1].Name : std::wstring();
	// the hex view lets go of the stream first
	ShowStream(-1);
	if (Change([&] { return Streams::DeleteStream(m_File.Path, name); }, L"Deleting the stream"))
		Refresh(next);
	else
		ShowStream(GetSelectedRow());
	return 0;
}

void CFileStreamsView::OnHexNotify(LPNMHDR hdr) {
	if (!m_Editing)
		return;
	switch (hdr->code) {
		case NMHX_VALUE_CHANGED:
		case NMHX_UNDO:
		case NMHX_REDO:
		{
			m_Changed = true;
			// the text pane follows the edits
			auto size = (size_t)(std::min<int64_t>)(m_Buffer->GetSize(), TextLimit);
			m_TextData.resize(size);
			if (size)
				m_TextData.resize(m_Buffer->GetData(0, m_TextData.data(), (uint32_t)size));
			ShowText();
			UpdateStatus();
			break;
		}
	}
}

bool CFileStreamsView::StartEditing() {
	int row = m_Shown;
	if (row < 0)
		return false;
	auto& stream = m_File.Streams[row];
	auto hWnd = Frame()->GetHwnd();
	if (stream.Size > ByteEditLimit) {
		Ask(hWnd, std::format(L"The stream is too large to edit (more than {}).", Helpers::FormatSize(ByteEditLimit)), MB_ICONINFORMATION);
		return false;
	}
	auto data = Streams::ReadStream(m_File.Path, stream.Name);
	if (!data) {
		Ask(hWnd, Helpers::ErrorText(data.error()), MB_ICONERROR);
		return false;
	}
	auto buffer = std::make_unique<MemoryBuffer>(data->data(), (uint32_t)data->size(), true);
	m_Hex.SetBufferManager(buffer.get());
	m_Buffer = std::move(buffer);
	m_EditedSize = (int64_t)data->size();
	m_Hex.SetReadOnly(false);
	m_Hex.SetOptions(HexControlOptions::GrayOutZeros);
	m_Editing = true;
	m_Changed = false;
	m_Hex.SetFocus();
	UpdateStatus();
	return true;
}

void CFileStreamsView::StopEditing() {
	m_Editing = false;
	m_Changed = false;
	m_Hex.SetReadOnly(true);
	m_Hex.SetOptions(HexControlOptions::DisableUndoRedo | HexControlOptions::GrayOutZeros);
	// the stream as it is
	ShowStream(m_Shown);
}

bool CFileStreamsView::IsDirty() const {
	return m_Editing && (m_Changed || m_Hex.IsModified() || m_Buffer->GetSize() != m_EditedSize);
}

bool CFileStreamsView::SaveEdits() {
	auto& stream = m_File.Streams[m_Shown];
	std::vector<uint8_t> data((size_t)m_Buffer->GetSize());
	if (!data.empty())
		m_Buffer->GetData(0, data.data(), (uint32_t)data.size());
	if (!Change([&] { return Streams::WriteStream(m_File.Path, stream.Name, data); }, L"Saving the stream"))
		return false;
	// the list as it is, but for the stream's new size (a refresh would select rows, maybe while one is being selected)
	stream.Size = (int64_t)data.size();
	m_List.RedrawItems(m_Shown, m_Shown);
	m_Changed = false;
	m_EditedSize = stream.Size;
	return true;
}

bool CFileStreamsView::ConfirmStopEditing() {
	if (!m_Editing)
		return true;
	if (!IsDirty()) {
		StopEditing();
		return true;
	}
	auto answer = Ask(Frame()->GetHwnd(), std::format(L"Save the changes to the stream {}?", m_File.Streams[m_Shown].Name),
		MB_ICONQUESTION | MB_YESNOCANCEL);
	if (answer == IDCANCEL)
		return false;
	if (answer == IDYES && !SaveEdits())
		return false;
	StopEditing();
	return true;
}

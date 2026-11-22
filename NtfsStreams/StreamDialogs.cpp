#include "pch.h"
#include "StreamDialogs.h"
#include "Streams.h"
#include <DpiHelper.h>

namespace {
	// what's wrong with the name, or empty
	std::wstring CheckName(std::wstring const& name, NameCheck const& check) {
		if (name.empty())
			return L"A stream needs a name.";
		if (!Streams::IsValidStreamName(name))
			return L"A stream name can't have ':', '\\', '/' or control characters, or be longer than 255 characters.";
		return check ? check(name) : std::wstring();
	}

	std::wstring WindowText(CWindow window) {
		std::wstring text(window.GetWindowTextLength(), 0);
		window.GetWindowText(text.data(), (int)text.size() + 1);
		return text;
	}

	// the combo box's items, in order
	const TextDecode::Encoding s_Encodings[] = { TextDecode::Encoding::Utf8, TextDecode::Encoding::Utf16, TextDecode::Encoding::Ascii };
}

LRESULT CNameDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	SetWindowText(m_Title.c_str());
	SetDlgItemText(IDC_PROMPT, m_Prompt.c_str());
	SetDlgItemText(IDC_NAME, m_Name.c_str());
	CEdit name(GetDlgItem(IDC_NAME));
	name.SetSelAll();
	name.SetFocus();
	CenterWindow(GetParent());
	BOOL handled;
	OnNameChanged(0, 0, nullptr, handled);
	return FALSE;
}

LRESULT CNameDlg::OnNameChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto error = CheckName(WindowText(GetDlgItem(IDC_NAME)), m_Check);
	SetDlgItemText(IDC_NAME_ERROR, error.c_str());
	GetDlgItem(IDOK).EnableWindow(error.empty());
	return 0;
}

LRESULT CNameDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (wID == IDOK)
		m_Name = WindowText(GetDlgItem(IDC_NAME));
	EndDialog(wID);
	return 0;
}

LRESULT CTextStreamDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	SetWindowText(m_Title.c_str());
	SetDlgItemText(IDC_NAME, m_Name.c_str());
	CEdit name(GetDlgItem(IDC_NAME));
	name.SetReadOnly(m_NameFixed);

	CComboBox encodings(GetDlgItem(IDC_ENCODING));
	for (auto e : s_Encodings) {
		int i = encodings.AddString(TextDecode::Name(e));
		if (e == m_Encoding)
			encodings.SetCurSel(i);
	}
	CheckDlgButton(IDC_BOM, m_Bom ? BST_CHECKED : BST_UNCHECKED);
	GetDlgItem(IDC_BOM).EnableWindow(m_Encoding != TextDecode::Encoding::Ascii);

	CEdit text(GetDlgItem(IDC_TEXT));
	auto dpi = DpiHelper::GetDpi(m_hWnd);
	m_Font.CreateFont(-::MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
	text.SetFont(m_Font);
	text.SetLimitText(0);
	text.SetWindowText(m_Text.c_str());

	DlgResize_Init(true);
	CenterWindow(GetParent());
	BOOL handled;
	OnNameChanged(0, 0, nullptr, handled);
	if (m_NameFixed)
		text.SetFocus();
	else
		name.SetFocus();
	return FALSE;
}

LRESULT CTextStreamDlg::OnNameChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto error = m_NameFixed ? std::wstring() : CheckName(WindowText(GetDlgItem(IDC_NAME)), m_Check);
	SetDlgItemText(IDC_NAME_ERROR, error.c_str());
	GetDlgItem(IDOK).EnableWindow(error.empty());
	return 0;
}

LRESULT CTextStreamDlg::OnEncodingChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	GetDlgItem(IDC_BOM).EnableWindow(GetSelectedEncoding() != TextDecode::Encoding::Ascii);
	return 0;
}

LRESULT CTextStreamDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (wID == IDOK) {
		m_Name = WindowText(GetDlgItem(IDC_NAME));
		m_Text = WindowText(GetDlgItem(IDC_TEXT));
		m_Encoding = GetSelectedEncoding();
		m_Bom = m_Encoding != TextDecode::Encoding::Ascii && IsDlgButtonChecked(IDC_BOM) == BST_CHECKED;
	}
	EndDialog(wID);
	return 0;
}

TextDecode::Encoding CTextStreamDlg::GetSelectedEncoding() const {
	int sel = CComboBox(GetDlgItem(IDC_ENCODING)).GetCurSel();
	return sel >= 0 && sel < _countof(s_Encodings) ? s_Encodings[sel] : TextDecode::Encoding::Utf8;
}

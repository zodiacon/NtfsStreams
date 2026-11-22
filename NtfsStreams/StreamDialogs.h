#pragma once

#include "resource.h"
#include "TextDecode.h"

// says what's wrong with a stream name the user typed (empty: nothing); the dialogs check it's a name a stream can have
using NameCheck = std::function<std::wstring(std::wstring const& name)>;

//
// A stream's name: Rename, and the name for Import File as Stream. OK is enabled while the name is valid.
//
class CNameDlg : public CDialogImpl<CNameDlg> {
public:
	enum { IDD = IDD_NAME };

	CNameDlg(std::wstring title, std::wstring prompt, std::wstring name, NameCheck check = {}) :
		m_Title(std::move(title)), m_Prompt(std::move(prompt)), m_Name(std::move(name)), m_Check(std::move(check)) {}

	std::wstring const& GetName() const {
		return m_Name;
	}

	BEGIN_MSG_MAP(CNameDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_NAME, EN_CHANGE, OnNameChanged)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNameChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	std::wstring m_Title, m_Prompt, m_Name;
	NameCheck m_Check;
};

//
// A stream's text: New Stream (the name, the encoding and the text) and Edit Text (the name can't change). Resizable.
//
class CTextStreamDlg :
	public CDialogImpl<CTextStreamDlg>,
	public CDialogResize<CTextStreamDlg> {
public:
	enum { IDD = IDD_TEXTSTREAM };

	CTextStreamDlg(std::wstring title, std::wstring name, bool nameFixed, std::wstring text, TextDecode::Encoding encoding, bool bom,
		NameCheck check = {}) :
		m_Title(std::move(title)), m_Name(std::move(name)), m_Text(std::move(text)), m_Check(std::move(check)),
		m_Encoding(encoding == TextDecode::Encoding::Auto ? TextDecode::Encoding::Utf8 : encoding), m_Bom(bom), m_NameFixed(nameFixed) {}

	std::wstring const& GetName() const {
		return m_Name;
	}
	std::wstring const& GetText() const {
		return m_Text;
	}
	TextDecode::Encoding GetEncoding() const {
		return m_Encoding;
	}
	bool GetBom() const {
		return m_Bom;
	}

	BEGIN_DLGRESIZE_MAP(CTextStreamDlg)
		DLGRESIZE_CONTROL(IDC_NAME, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_TEXT, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_NAME_ERROR, DLSZ_MOVE_Y | DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()

	BEGIN_MSG_MAP(CTextStreamDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_NAME, EN_CHANGE, OnNameChanged)
		COMMAND_HANDLER(IDC_ENCODING, CBN_SELCHANGE, OnEncodingChanged)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CDialogResize<CTextStreamDlg>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNameChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEncodingChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	TextDecode::Encoding GetSelectedEncoding() const;

	std::wstring m_Title, m_Name, m_Text;
	NameCheck m_Check;
	CFont m_Font;
	TextDecode::Encoding m_Encoding;
	bool m_Bom;
	bool m_NameFixed;
};

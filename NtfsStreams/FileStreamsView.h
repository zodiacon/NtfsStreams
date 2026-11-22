#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "StreamPanes.h"

//
// A file's (or directory's) tab: its streams in the stream panes (CStreamPanes), which do the work; the frame's commands
// are routed on to them.
//
class CFileStreamsView :
	public CViewBase<CFileStreamsView>,
	public IStreamPanesHost {
public:
	CFileStreamsView(IMainFrame* frame, std::wstring path, bool isDirectory) :
		CViewBase(frame), m_Path(std::move(path)), m_IsDirectory(isDirectory), m_Panes(frame, this) {}

	// IView
	ViewType GetType() const override {
		return ViewType::File;
	}
	std::wstring const& GetPath() const override {
		return m_Path;
	}
	CString GetTitle() const override;
	UINT GetIcon() const override {
		return m_IsDirectory ? IDI_FOLDER : IDI_FILE;
	}
	std::wstring GetStatusText() const override {
		return m_Panes.GetStatusText();
	}
	void OnActivate(bool active) override {
		m_Active = active;
	}
	void UpdateUI(CUpdateUIBase& ui) override {
		m_Panes.UpdateUI(ui);
	}
	bool CanClose() override {
		return m_Panes.CanLeave();
	}
	void OnThemeChanged(bool dark) override {
		m_Panes.OnThemeChanged(dark);
	}
	void OnDpiChanged(UINT dpi) override {
		m_Panes.OnDpiChanged(dpi);
	}

	// IStreamPanesHost
	void OnPanesStatusChanged() override;

	BEGIN_MSG_MAP(CFileStreamsView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CViewBase<CFileStreamsView>)
	ALT_MSG_MAP(1)
		if (uMsg == WM_COMMAND && m_Panes.m_hWnd && m_Panes.ProcessWindowMessage(m_Panes, uMsg, wParam, lParam, lResult, 1))
			return TRUE;
		CHAIN_MSG_MAP_ALT(CViewBase<CFileStreamsView>, 1)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	std::wstring m_Path;
	bool m_IsDirectory;
	CStreamPanes m_Panes;
	bool m_Active{ false };
};

#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "Streams.h"
#include <VirtualListView.h>

//
// One file's (or directory's) alternate streams: their names and sizes.
//
class CFileStreamsView :
	public CViewBase<CFileStreamsView>,
	public CVirtualListView<CFileStreamsView> {
public:
	CFileStreamsView(IMainFrame* frame, Streams::FileStreams file) : CViewBase(frame), m_File(std::move(file)) {}

	// IView
	ViewType GetType() const override {
		return ViewType::File;
	}
	std::wstring const& GetPath() const override {
		return m_File.Path;
	}
	CString GetTitle() const override;
	UINT GetIcon() const override {
		return m_File.IsDirectory ? IDI_FOLDER : IDI_FILE;
	}
	std::wstring GetStatusText() const override;
	void OnDpiChanged(UINT /*dpi*/) override;

	// CVirtualListView
	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);

	BEGIN_MSG_MAP(CFileStreamsView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CFileStreamsView>)
		CHAIN_MSG_MAP(CViewBase<CFileStreamsView>)
	ALT_MSG_MAP(1)
		CHAIN_MSG_MAP_ALT(CViewBase<CFileStreamsView>, 1)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class Column { Name, Size };

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	Streams::FileStreams m_File;
};

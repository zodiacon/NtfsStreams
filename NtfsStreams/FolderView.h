#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "Streams.h"
#include <VirtualListView.h>
#include <mutex>
#include <thread>

//
// The files in a folder that have alternate streams. The folder is scanned on a thread of its own (Streams::ScanFolder),
// whose finds are posted to the view in batches, so the list fills while the scan goes on. Enter, a double click or
// View > View Selected Files opens the selected files' streams in tabs of their own.
//
class CFolderView :
	public CViewBase<CFolderView>,
	public CVirtualListView<CFolderView> {
public:
	CFolderView(IMainFrame* frame, std::wstring folder) : CViewBase(frame), m_Folder(std::move(folder)) {}

	// IView
	ViewType GetType() const override {
		return ViewType::Folder;
	}
	std::wstring const& GetPath() const override {
		return m_Folder;
	}
	CString GetTitle() const override;
	UINT GetIcon() const override {
		return IDI_FOLDER;
	}
	std::wstring GetStatusText() const override;
	void OnActivate(bool active) override;
	void UpdateUI(CUpdateUIBase& ui) override;
	void OnDpiChanged(UINT /*dpi*/) override;

	// CVirtualListView
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt);

	BEGIN_MSG_MAP(CFolderView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SCAN_FOUND, OnScanFound)
		MESSAGE_HANDLER(WM_SCAN_DONE, OnScanDone)
		NOTIFY_CODE_HANDLER(NM_RETURN, OnReturn)
		CHAIN_MSG_MAP(CVirtualListView<CFolderView>)
		CHAIN_MSG_MAP(CViewBase<CFolderView>)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_VIEWFILES, OnViewFiles)
		CHAIN_MSG_MAP_ALT(CViewBase<CFolderView>, 1)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	// posted by the scanning thread: files are waiting in the shared state (at most one posted at a time), the scan ended
	static constexpr UINT WM_SCAN_FOUND = WM_APP + 1;
	static constexpr UINT WM_SCAN_DONE = WM_APP + 2;

	enum class Column { Name, Streams, TotalSize };

	// shared with the scanning thread, which may outlive nothing: the view joins it before its window goes
	struct ScanState {
		std::mutex Lock;
		std::vector<Streams::FileStreams> Pending;	// found, not yet taken by the view
		bool Posted{ false };		// WM_SCAN_FOUND is on its way
		Streams::ScanResult Result;	// when WM_SCAN_DONE is posted
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnScanFound(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnScanDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnReturn(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnViewFiles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void StartScan();
	void TakePending();
	void OpenSelected();
	// the path relative to the scanned folder
	std::wstring RelativeName(std::wstring const& path) const;
	void UpdateStatus() const;

	CListViewCtrl m_List;
	std::wstring m_Folder;
	std::vector<Streams::FileStreams> m_Files;
	std::shared_ptr<ScanState> m_State;
	std::jthread m_Scan;
	Streams::ScanResult m_Result;
	bool m_Scanning{ false };
	bool m_Active{ false };
};

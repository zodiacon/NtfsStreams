#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "Streams.h"
#include "StreamPanes.h"
#include <VirtualListView.h>
#include <CustomSplitterWindow.h>
#include <mutex>
#include <thread>

//
// The files in a folder (and with Include Subfolders, its subfolders) that have alternate streams. The folder is scanned
// on a thread of its own (Streams::ScanFolder), whose finds are posted to the view in batches, so the list fills while
// the scan goes on; a timer shows how many files were looked at so far. Refresh scans again, Stop ends the scan early.
// Enter, a double click or View Streams opens the selected files' streams in tabs of their own. The selection is kept
// by path, so sorting and rows coming in don't move it to other files. At the right, the stream panes (CStreamPanes) show
// the selected file's streams, a moment (SelectDelay) after the selection settles, so arrow keys don't read every file
// on the way; the frame's commands go to the folder view first, then to the panes.
//
class CFolderView :
	public CViewBase<CFolderView>,
	public CVirtualListView<CFolderView>,
	public IStreamPanesHost {
public:
	CFolderView(IMainFrame* frame, std::wstring folder);

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
	bool CanClose() override {
		return m_Panes.CanLeave();
	}
	void OnThemeChanged(bool dark) override {
		m_Panes.OnThemeChanged(dark);
	}
	void OnDpiChanged(UINT dpi) override;

	// IStreamPanesHost
	void OnStreamsChanged(Streams::FileStreams const& file) override;
	void OnPanesStatusChanged() override {
		UpdateStatus();
	}

	// CVirtualListView
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);

	BEGIN_MSG_MAP(CFolderView)
		// the view's own tool bar's commands, as those the frame routes here (ALT_MSG_MAP(1))
		if (uMsg == WM_COMMAND && ProcessWindowMessage(hWnd, uMsg, wParam, lParam, lResult, 1))
			return TRUE;
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		MESSAGE_HANDLER(WM_SCAN_FOUND, OnScanFound)
		MESSAGE_HANDLER(WM_SCAN_DONE, OnScanDone)
		NOTIFY_CODE_HANDLER(NM_RETURN, OnReturn)
		CHAIN_MSG_MAP(CVirtualListView<CFolderView>)
		CHAIN_MSG_MAP(CViewBase<CFolderView>)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_VIEWFILES, OnViewFiles)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_FOLDER_SUBFOLDERS, OnSubfolders)
		COMMAND_ID_HANDLER(ID_FOLDER_STOP, OnStop)
		COMMAND_ID_HANDLER(ID_FOLDER_OPENINEXPLORER, OnOpenInExplorer)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnCopy)
		COMMAND_ID_HANDLER(ID_FOLDER_UNBLOCK, OnDeleteStreams)
		COMMAND_ID_HANDLER(ID_FOLDER_DELETESTREAMS, OnDeleteStreams)
		// the rest are the panes' (the stream commands)
		if (uMsg == WM_COMMAND && m_Panes.m_hWnd && m_Panes.ProcessWindowMessage(m_Panes, uMsg, wParam, lParam, lResult, 1))
			return TRUE;
		CHAIN_MSG_MAP_ALT(CViewBase<CFolderView>, 1)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	// posted by the scanning thread (wParam: the scan's m_ScanId, so a scan's leftovers are ignored by the next one):
	// files are waiting in the shared state (at most one posted at a time), the scan ended
	static constexpr UINT WM_SCAN_FOUND = WM_APP + 1;
	static constexpr UINT WM_SCAN_DONE = WM_APP + 2;
	static constexpr UINT_PTR ProgressTimer = 1;
	static constexpr UINT_PTR SelectTimer = 2;
	static constexpr UINT SelectDelay = 100;	// ms

	enum class Column { Name, Streams, TotalSize };

	// shared with the scanning thread; the view joins it before its window goes
	struct ScanState {
		std::mutex Lock;
		std::vector<Streams::FileStreams> Pending;	// found, not yet taken by the view
		bool Posted{ false };		// WM_SCAN_FOUND is on its way
		Streams::ScanResult Result;	// when WM_SCAN_DONE is posted
		std::atomic<uint32_t> Scanned{ 0 };	// files looked at so far
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnScanFound(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnScanDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnReturn(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnViewFiles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSubfolders(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOpenInExplorer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// Unblock (the Zone.Identifier streams) and Delete All Streams, of the selected files; their rows are read again
	LRESULT OnDeleteStreams(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// a new scan, after the running one (if any) ended; the list starts empty
	void StartScan();
	void StopScan(bool wait);
	void TakePending();
	void OpenSelected();
	std::vector<std::wstring> GetSelectedPaths() const;
	// selects the rows of those paths (the first one focused) after the rows moved
	void SelectPaths(std::vector<std::wstring> const& paths);
	// the path relative to the scanned folder
	std::wstring RelativeName(std::wstring const& path) const;
	void UpdateStatus() const;
	// the panes show the selected file (one selected), or say what to do; a file with unsaved edits the user keeps
	// stays, and gets the selection back
	void ShowSelected();
	// the panes with nothing in them (false: the user kept unsaved edits)
	bool ClearPanes();

	CCustomSplitterWindow m_Splitter;	// the files | the panes
	CListViewCtrl m_List;
	CStreamPanes m_Panes;
	std::wstring m_ShownPath;	// the file in the panes
	CToolBarCtrl m_Bar;
	std::wstring m_Folder;
	std::vector<Streams::FileStreams> m_Files;
	std::shared_ptr<ScanState> m_State;
	std::jthread m_Scan;
	Streams::ScanResult m_Result;
	WPARAM m_ScanId{ 0 };	// the running (or last) scan's
	bool m_Recursive;
	bool m_Scanning{ false };
	bool m_Active{ false };
	bool m_Restoring{ false };	// putting the selection back on the shown file
};

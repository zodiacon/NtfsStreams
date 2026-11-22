#pragma once

#include "resource.h"
#include "Interfaces.h"
#include "Streams.h"
#include "StreamBuffer.h"
#include "TextDecode.h"
#include <VirtualListView.h>
#include <CustomSplitterWindow.h>
#include <HexControl/SimpleHexControl.h>
#include <HexControl/MemoryBuffer.h>

// what the panes tell the view they're in
struct IStreamPanesHost abstract {
	// the file's streams changed (the panes changed them, or read them again): the view's row for it can follow
	virtual void OnStreamsChanged(Streams::FileStreams const& /*file*/) {}
	// GetStatusText says something else now
	virtual void OnPanesStatusChanged() {}
};

//
// One file's (or directory's) alternate streams, top to bottom: the list of them; the selected one in the hex view (read
// on demand through a StreamBuffer, so its size doesn't matter); its start decoded as text (TextLimit bytes at most).
// A child window with a tool bar of its own (the hex view's bytes per line and item size and the text's encoding, which
// are saved for the next panes, and the editing commands). The file tab is these panes alone; the folder tab shows them
// for the selected file. The view routes the frame's commands to them (ProcessWindowMessage with ALT_MSG_MAP(1)) and
// asks them before the shown file goes (CanLeave): Edit Bytes may have unsaved changes.
//
class CStreamPanes :
	public CFrameWindowImpl<CStreamPanes, CWindow, CControlWinTraits>,
	public CAutoUpdateUI<CStreamPanes>,
	public CVirtualListView<CStreamPanes>,
	public CIdleHandler {
public:
	using BaseFrame = CFrameWindowImpl<CStreamPanes, CWindow, CControlWinTraits>;

	CStreamPanes(IMainFrame* frame, IStreamPanesHost* host) : m_Frame(frame), m_Host(host) {}

	HWND DoCreate(HWND hParent);

	// the file's streams read and shown (the same file: read again, its selected stream kept). false: the user kept
	// unsaved edits of the shown one
	bool ShowFile(std::wstring const& path, bool isDirectory);
	// nothing shown (text: what the text pane says instead)
	bool Clear(std::wstring const& text = {});
	bool HasFile() const {
		return m_HasFile;
	}
	Streams::FileStreams const& GetFile() const {
		return m_File;
	}
	// the streams read again (the list keeps its sorting and its selection by name: select, if given, else the one selected)
	void Refresh(std::wstring const& select = {});
	// unsaved byte edits: save them, discard them, or stay (false)
	bool CanLeave();

	std::wstring GetStatusText() const;
	// the frame's menus' states for the panes' commands (and the panes' tool bar's)
	void UpdateUI(CUpdateUIBase& ui);
	void OnThemeChanged(bool dark);
	void OnDpiChanged(UINT dpi);
	// the focus is in one of the panes
	bool HasFocus() const;

	// CIdleHandler
	BOOL OnIdle() override;

	// CVirtualListView
	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);

	// the bytes of a stream the text pane shows at most
	static constexpr size_t TextLimit = 1 << 20;
	// the largest stream Edit Text and Edit Bytes take (read into memory)
	static constexpr int64_t TextEditLimit = 16 << 20;
	static constexpr int64_t ByteEditLimit = 64 << 20;

	BEGIN_MSG_MAP(CStreamPanes)
		// the panes' own tool bar's commands, as those the view routes here (ALT_MSG_MAP(1))
		if (uMsg == WM_COMMAND && ProcessWindowMessage(hWnd, uMsg, wParam, lParam, lResult, 1))
			return TRUE;
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		// the hex view's (its codes are its own, so by window)
		if (uMsg == WM_NOTIFY && reinterpret_cast<LPNMHDR>(lParam)->hwndFrom == m_Hex.m_hWnd) {
			OnHexNotify(reinterpret_cast<LPNMHDR>(lParam));
			lResult = 0;
			return TRUE;
		}
		NOTIFY_CODE_HANDLER(TBN_DROPDOWN, OnDropDown)
		NOTIFY_CODE_HANDLER(LVN_KEYDOWN, OnListKeyDown)
		CHAIN_MSG_MAP(CVirtualListView<CStreamPanes>)
		CHAIN_MSG_MAP(CAutoUpdateUI<CStreamPanes>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnCopy)
		COMMAND_ID_HANDLER(ID_STREAM_EXPORT, OnExport)
		COMMAND_ID_HANDLER(ID_HEX_BYTESPERLINE, OnDropDownButton)
		COMMAND_ID_HANDLER(ID_HEX_DATASIZE, OnDropDownButton)
		COMMAND_ID_HANDLER(ID_TEXT_ENCODING, OnDropDownButton)
		COMMAND_ID_HANDLER(ID_STREAM_NEW, OnNewStream)
		COMMAND_ID_HANDLER(ID_STREAM_IMPORT, OnImport)
		COMMAND_ID_HANDLER(ID_STREAM_EDITTEXT, OnEditText)
		COMMAND_ID_HANDLER(ID_STREAM_EDITBYTES, OnEditBytes)
		COMMAND_ID_HANDLER(ID_STREAM_SAVE, OnSave)
		COMMAND_ID_HANDLER(ID_STREAM_DISCARD, OnDiscard)
		COMMAND_ID_HANDLER(ID_STREAM_RENAME, OnRename)
		COMMAND_ID_HANDLER(ID_STREAM_DELETE, OnDelete)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class Column { Name, Size };

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDropDown(int /*idCtrl*/, LPNMHDR hdr, BOOL& bHandled);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDropDownButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListKeyDown(int /*idCtrl*/, LPNMHDR hdr, BOOL& bHandled);
	LRESULT OnNewStream(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditBytes(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDiscard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void OnHexNotify(LPNMHDR hdr);

	// Edit Bytes: the shown stream read into memory, the hex view editable
	bool StartEditing();
	// back to the stream as it is (the edits, if any, gone)
	void StopEditing();
	// the edited bytes written to the stream; false (and a message) if that failed
	bool SaveEdits();
	bool IsDirty() const;
	// before the shown stream goes away (another one shown, a refresh, the file or the tab gone): saves or discards unsaved
	// edits as the user says; false: the user canceled
	bool ConfirmStopEditing();
	bool StreamExists(std::wstring const& name) const;
	// a change to the file's streams, keeping its times as the option says; error: what failed, shown
	bool Change(std::function<DWORD()> const& change, PCWSTR what);
	// the stream at the list's row in the hex view and the text pane (-1: none)
	void ShowStream(int row);
	void ShowText();
	int GetSelectedRow() const;
	// the drop-down's menu under its button; applies the item picked
	void ShowDropDown(UINT buttonId);
	void SetBytesPerLine(int bytes);
	void SetDataSize(int size);
	void SetEncoding(TextDecode::Encoding encoding);
	// the drop-downs' text from the current choices, and the tool bar's band for their new widths
	void UpdateBarText();
	void SetHexColors(bool dark);
	void SetTextFont();
	void UpdateStatus() const;

	IMainFrame* m_Frame;
	IStreamPanesHost* m_Host;
	CListViewCtrl m_List;
	CToolBarCtrl m_Bar;
	CCustomHorSplitterWindow m_Splitter;	// the list above the data
	CCustomHorSplitterWindow m_Data;		// the hex view above the text
	CHexControl m_Hex;
	CEdit m_Text;
	CFont m_TextFont;
	Streams::FileStreams m_File;
	std::unique_ptr<IBufferManager> m_Buffer;	// the shown stream's, or an empty one
	std::vector<uint8_t> m_TextData;	// the shown stream's first TextLimit bytes
	std::wstring m_Placeholder;	// the text pane's with no file
	int m_Shown{ -1 };		// the list's row in the panes
	int m_BytesPerLine{ 16 }, m_DataSize{ 1 };
	TextDecode::Encoding m_Encoding{ TextDecode::Encoding::Auto };
	std::wstring m_Error;	// reading the file's streams', shown in the status bar instead
	std::wstring m_StreamError;	// opening the shown stream's, shown in the text pane
	int64_t m_EditedSize{ 0 };	// the stream's when its editing started
	bool m_HasFile{ false };
	bool m_Editing{ false };	// Edit Bytes: the hex view holds the stream in memory and can change it
	bool m_Changed{ false };	// a byte changed while editing
	bool m_Restoring{ false };	// putting the selection back after a canceled switch
};

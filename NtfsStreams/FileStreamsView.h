#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "Streams.h"
#include "StreamBuffer.h"
#include "TextDecode.h"
#include <VirtualListView.h>
#include <CustomSplitterWindow.h>
#include <HexControl/SimpleHexControl.h>
#include <HexControl/MemoryBuffer.h>

//
// One file's (or directory's) alternate streams: the list of them at the left; at the right the selected stream in the
// hex view (read on demand through a StreamBuffer, so its size doesn't matter) above a text pane with its start decoded
// as text (TextLimit bytes at most). The tool bar's drop-downs set the hex view's bytes per line and item size and the
// text pane's encoding, which are saved for the next tabs; Export saves the stream to a file.
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
	void OnActivate(bool active) override {
		m_Active = active;
	}
	void UpdateUI(CUpdateUIBase& ui) override;
	// unsaved byte edits: save them, discard them, or stay (false)
	bool CanClose() override;
	void OnThemeChanged(bool dark) override;
	void OnDpiChanged(UINT dpi) override;

	// CVirtualListView
	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);

	// the bytes of a stream the text pane shows at most
	static constexpr size_t TextLimit = 1 << 20;
	// the largest stream Edit Text and Edit Bytes take (read into memory)
	static constexpr int64_t TextEditLimit = 16 << 20;
	static constexpr int64_t ByteEditLimit = 64 << 20;

	BEGIN_MSG_MAP(CFileStreamsView)
		// the view's own tool bar's commands, as those the frame routes here (ALT_MSG_MAP(1))
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
		CHAIN_MSG_MAP(CVirtualListView<CFileStreamsView>)
		CHAIN_MSG_MAP(CViewBase<CFileStreamsView>)
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
		CHAIN_MSG_MAP_ALT(CViewBase<CFileStreamsView>, 1)
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

	// the streams read again (the list keeps its sorting and its selection by name: select, if given, else the one selected)
	void Refresh(std::wstring const& select = {});
	// Edit Bytes: the shown stream read into memory, the hex view editable
	bool StartEditing();
	// back to the stream as it is (the edits, if any, gone)
	void StopEditing();
	// the edited bytes written to the stream; false (and a message) if that failed
	bool SaveEdits();
	bool IsDirty() const;
	// before the shown stream goes away (another one shown, a refresh, the tab closed): saves or discards unsaved edits
	// as the user says; false: the user canceled
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

	CListViewCtrl m_List;
	CToolBarCtrl m_Bar;
	CCustomSplitterWindow m_Splitter;		// the list | the panes
	CCustomHorSplitterWindow m_Panes;		// the hex view above the text
	CHexControl m_Hex;
	CEdit m_Text;
	CFont m_TextFont;
	Streams::FileStreams m_File;
	std::unique_ptr<IBufferManager> m_Buffer;	// the shown stream's, or an empty one
	std::vector<uint8_t> m_TextData;	// the shown stream's first TextLimit bytes
	int m_Shown{ -1 };		// the list's row in the panes
	int m_BytesPerLine{ 16 }, m_DataSize{ 1 };
	TextDecode::Encoding m_Encoding{ TextDecode::Encoding::Auto };
	std::wstring m_Error;	// the last refresh's, shown in the status bar instead
	std::wstring m_StreamError;	// opening the shown stream's, shown in the text pane
	int64_t m_EditedSize{ 0 };	// the stream's when its editing started
	bool m_Active{ false };
	bool m_Editing{ false };	// Edit Bytes: the hex view holds the stream in memory and can change it
	bool m_Changed{ false };	// a byte changed while editing
	bool m_Restoring{ false };	// putting the selection back after a canceled switch
};

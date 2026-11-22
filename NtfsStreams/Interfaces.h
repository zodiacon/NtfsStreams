#pragma once

enum class ViewType {
	File,		// one file's (or directory's) streams
	Folder,		// the files with streams in a folder
};

// what the views see of the frame
struct IMainFrame abstract {
	virtual HWND GetHwnd() const = 0;
	virtual CUpdateUIBase& GetUI() = 0;
	// the status bar's second pane: the active view's summary
	virtual void SetStatusText(PCWSTR text) = 0;
	virtual BOOL ShowContextMenu(HMENU hMenu, POINT const& pt, DWORD flags = 0) = 0;

	// the shared 16x16 image list (views should create their lists with LVS_SHAREIMAGELISTS), and an icon's index in it
	virtual HIMAGELIST GetImageList() const = 0;
	virtual int GetIconIndex(UINT iconId) const = 0;

	// opens a tab with the file's streams, or activates the open one; activate: make it the active tab
	virtual bool OpenFile(std::wstring const& path, bool activate = true) = 0;
};

struct IView abstract {
	virtual ViewType GetType() const = 0;
	// the full path of the file or folder: the view's identity (one tab per path and type)
	virtual std::wstring const& GetPath() const = 0;
	virtual CString GetTitle() const = 0;
	virtual UINT GetIcon() const = 0;
	virtual HWND GetHwnd() const = 0;
	// the status bar text while the view is active
	virtual std::wstring GetStatusText() const = 0;

	// before the view's tab closes (or the app exits): false keeps it open (unsaved changes the user wants to keep)
	virtual bool CanClose() {
		return true;
	}
	virtual void OnActivate(bool /*active*/) {}
	// on idle while active: the frame's commands that depend on the view (View Files...)
	virtual void UpdateUI(CUpdateUIBase& /*ui*/) {}
	virtual void OnThemeChanged(bool /*dark*/) {}
	virtual void OnDpiChanged(UINT /*dpi*/) {}
};

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

NTFS Streams: a native Win32 GUI app (C++23, ATL/WTL, Unicode) that shows and edits NTFS alternate data streams. The app
is in `NtfsStreams/`; its UI libraries come from the `wtlhelper/` git submodule (read `wtlhelper/CLAUDE.md` before
changing it): `WTLHelper` (static lib: dark mode, `Settings`, `VirtualListView.h`, tab view, splitters, tool bar helpers,
vendored WTL headers in `WTL/`) and `HexControl` (`<HexControl/SimpleHexControl.h>`), both projects in the solution's
Controls folder with project references from the app. The design follows `C:\dev\winsyslive` (WinSysLive), by the
same author. `AltStream/` is a small console tool that writes a stream, for testing by hand. The WPF version this
replaced is at the `wpf-final` tag.

If the submodule directory is empty, run `git submodule update --init`.

## Build and test

MSBuild only (no CMake, no lint). Toolset v145 (VS 18), SDK 10.0.26100.0, static CRT (`/MT`, `/MTd`), `/utf-8`.

```powershell
msbuild NtfsStreams.sln /p:Configuration=Debug /p:Platform=x64 /m
x64\Debug\Tests.exe            # all the tests; Tests.exe Rename: those whose names contain "Rename"; -Rename: all but those
```

Dependencies come from vcpkg with user-wide integration: WIL, Detours and WTL. The app, the tests, WTLHelper and
HexControl all set `VcpkgUseStatic` (the static triplet, `x64-windows-static`). CI (`.github/workflows/build.yml`) builds
Debug and Release x64 and runs the tests on every push and pull request to `master`.

`Tests\Tests.vcxproj` is a console runner of its own (`TestFramework.h`: `TEST`, `CHECK`, `CHECK_EQ`; the exit code is
the number of failed tests). Its precompiled header is the app's `pch.h`, and it compiles the app's non-UI sources
(`Streams.cpp`, `Helpers.cpp`, `StreamBuffer.cpp`, `TextDecode.cpp`, `ExplorerMenu.cpp`) as the app does. Tests work in a
new folder in `%TEMP%` (or a temporary file) and remove it; the registry tests use a key under
`HKCU\Software\NtfsStreamsTests`, never the real `Software\Classes`. Keep code worth testing apart from the UI, add a test
with a change to it, and see it fail once before trusting it.

Include paths are `..\wtlhelper\WTLHelper` and `..\wtlhelper\WTL` (WTLHelper headers by bare name: `<WTLHelper.h>`,
`<VirtualListView.h>`) and `..\wtlhelper` (`<HexControl/SimpleHexControl.h>`).

## Architecture

- **Non-UI core** (tested):
  - `Streams.h/.cpp`: streams by bare name (`Zone.Identifier`, not `:Zone.Identifier:$DATA`; the default stream left
    out): `GetStreams`, `ReadStream`, `WriteStream`, `DeleteStream`, `RenameStream`, `ExportStream`, `ImportFile`,
    `ScanFolder` (recursive, cancellable with a `stop_token`, reports files as it finds them, counts as it goes, doesn't
    follow reparse points) and `FileTimesKeeper` (puts a file's times back when it goes: writing a stream changes the
    file's last write time). Errors are Win32 codes (`std::expected<T, DWORD>` or `DWORD`). NTFS won't rename a stream
    onto a non-empty one even with `ReplaceIfExists`, so `RenameStream` with replace moves the target aside first.
  - `StreamBuffer`: the hex control's `IBufferManager` over a stream, read on demand (one 64 KB chunk cached), read-only.
  - `TextDecode`: bytes as text for the text pane (`Detect`: BOM, zeros at odd positions for UTF-16, valid UTF-8;
    `Decode` counts the characters it replaced with '.'; `Encode` for writing).
  - `ExplorerMenu`: Explorer's "View NTFS Streams" verbs under HKCU (`*`, `Directory`, `Directory\Background`).
  - `Helpers`: number and size formatting, error text, paths, the recent lists, `JoinPaths`/`SplitPaths` (WM_COPYDATA).
- `NtfsStreams.cpp`: `_tWinMain`. Paths on the command line go to a running instance (`WM_COPYDATA` with
  `CMainFrame::CopyDataPaths`) if there is one; otherwise settings load from `Software\ScorpioSoftware\NtfsStreams`,
  `WTLHelper::InitDarkMode` runs *before* any window is created, then the frame and its message loop.
- `AppSettings.h`: WTLHelper `Settings` (`SETTING`/`DEF_SETTING`); the singleton is `AppSettings::Get()`; call `Save()`
  after a change.
- `MainFrm.*`: `CMainFrame` hosts the tabs in `CMainTabView` (`CNativeCustomTabViewImpl`), which doubles '&' in tab text
  (`ShortenTitle`) and gives tooltips the full path; the frame sets its caption itself (`UpdateCaption`). One tab per path
  and type (`FindView`). Tabs are matched to views by their windows (`GetView`), never by index; a view is forgotten
  (`ForgetView`) before its page is removed, which destroys the view's window (the view deletes itself). Views are asked
  `CanClose` before their tab closes or the app exits. The frame default-disables the views' commands on idle and the
  active view enables its own (`IView::UpdateUI`). Files dropped on the window and paths from another instance open
  through `OpenPaths`.
- Views (`Interfaces.h`: `IMainFrame`, `IView`; `ViewBase.h`: `CViewBase<T>` on WTLHelper's `CFrameView`, so each view is
  a child frame with its own rebar):
  - `COMMAND_TABVIEW_HANDLER(m_Tabs, 1)` routes the frame's `WM_COMMAND` to the active view's `ALT_MSG_MAP(1)`, which
    ends with `CHAIN_MSG_MAP_ALT(CViewBase<T>, 1)`. A view's own tool bar sends to its main map, which passes
    `WM_COMMAND` on to `ALT_MSG_MAP(1)` too.
  - `CStreamPanes` (`StreamPanes.*`): a file's streams top to bottom: the list, the hex view, the text pane, with a tool
    bar of their own and every stream command (new, import, edit text, edit bytes with save and discard, rename, delete,
    export). Not a view: a child window views host, routing commands to it (`ProcessWindowMessage(..., 1)`) and asking
    `CanLeave()` before the shown file goes (unsaved Edit Bytes changes). It tells its host about changed streams and
    status text through `IStreamPanesHost`.
  - `CFileStreamsView`: a file tab, the panes alone.
  - `CFolderView`: the files with streams (scanned on a `std::jthread`, results posted in batches with the scan's id so a
    previous scan's leftovers are ignored), the panes at the right for the selected file (after `SelectDelay`). Its
    commands come first (Refresh rescans; Copy copies paths unless the panes have the focus), then the panes'. The
    selection is kept by path across sorting and arriving rows.
- `StreamDialogs.*`: `CNameDlg` (rename, import name) and `CTextStreamDlg` (new stream, edit text; resizable).
- WTLHelper's window hook makes every `SendMessage` go through the kernel; large lists call `SuspendHookWhilePainting`
  (`ViewBase.h`).

## Conventions

- Every `.cpp` starts with `#include "pch.h"`.
- WTL style: `C` prefix for windows and dialogs, `m_` members, message maps with the commented handler-prototype
  boilerplate, tabs for indentation, opening braces on the same line.
- New command IDs go in `resource.h` (`ID_VIEW_REFRESH`, `ID_EDIT_COPY` and other common ones are WTL's, in
  `atlres.h`), menus and strings in `NtfsStreams.rc`. A command a view handles also goes in the frame's default-disabled
  list (`CMainFrame::OnIdle`).
- When adding a source file, register it in `NtfsStreams.vcxproj` and `NtfsStreams.vcxproj.filters` (and in
  `Tests\Tests.vcxproj` if the tests use it).
- Comments say what the code is for and why, in the style of the surrounding code.

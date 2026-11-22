# NTFS Streams

NTFS Streams shows, edits and manages the NTFS alternate data streams of files and folders. It's like the Sysinternals
command line tool `streams.exe`, but graphical, and it shows the streams' contents as well.

A native Windows app (C++, WTL), with dark mode and per-monitor DPI support.

## Features

- **Folders**: the files (and folders) that have alternate streams, optionally including subfolders, found in the
  background as the list fills; the selected file's streams show beside the list.
- **Files**: a file's streams, each shown in a hex view (any size: read as it's scrolled) and as text (Auto, ASCII,
  UTF-8 or UTF-16; Auto goes by the byte order mark and the bytes).
- **Editing**: add a stream (typed text or a file's contents), edit it as text or byte by byte, rename, delete, export to
  a file. Unsaved changes are never lost without asking.
- **Unblock**: remove the `Zone.Identifier` stream ("downloaded from the internet") from the selected files, or all
  their streams.
- **File times**: changing a stream changes the file's last write time; by default the times are put back (Options).
- **Explorer**: drop files and folders on the window, or add *View NTFS Streams* to Explorer's context menu (Options;
  on Windows 11 under *Show more options*). Paths opened while a window is open go to that window.
- Recent files and folders, tabs, settings kept in `HKCU\Software\ScorpioSoftware\NtfsStreams`.

## Command line

```
NtfsStreams.exe [file-or-folder ...]
```

Each file opens in a tab of its own, each folder in a folder tab; the first one is shown.

## Building

Visual Studio 2026 (toolset v145) with the C++ desktop workload, and [vcpkg](https://vcpkg.io) with user-wide
integration for WIL, Detours and WTL, in both triplets:

```
vcpkg install wil:x64-windows-static detours:x64-windows-static wtl:x64-windows-static wil:x64-windows detours:x64-windows wtl:x64-windows
vcpkg integrate install
git submodule update --init
msbuild NtfsStreams.sln /p:Configuration=Release /p:Platform=x64 /m
```

The UI libraries (WTLHelper, HexControl) come from the [WTLHelper](https://github.com/zodiacon/WTLHelper) submodule.
`x64\Release\Tests.exe` runs the tests (in a temporary folder); its exit code is the number that failed.

`AltStream` is a small console tool that writes a stream, for testing: `AltStream <file> <stream name> "text"`.

The original WPF version (2016) is at the `wpf-final` tag.

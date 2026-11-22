#pragma once

//
// Explorer's context menu entry, "View NTFS Streams", for the current user: on files (*), folders (Directory) and a
// folder's background (Directory\Background), each running the exe with the path. On Windows 11 the entries are under
// "Show more options". The keys are under root, in HKEY_CURRENT_USER: Software\Classes for the real ones (the tests use
// another key).
//
namespace ExplorerMenu {
	constexpr PCWSTR DefaultRoot = L"Software\\Classes";
	constexpr PCWSTR VerbName = L"NtfsStreams";

	// all the entries are there, running this exe (one moved elsewhere isn't)
	bool IsRegistered(std::wstring const& exePath, PCWSTR root = DefaultRoot);
	DWORD Register(std::wstring const& exePath, PCWSTR root = DefaultRoot);
	DWORD Unregister(PCWSTR root = DefaultRoot);
	// the command line an entry runs
	std::wstring Command(std::wstring const& exePath, bool background);
}

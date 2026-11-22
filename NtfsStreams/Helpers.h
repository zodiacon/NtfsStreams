#pragma once

namespace Helpers {
	// with the user's thousands separator: "1,234,567"
	std::wstring FormatNumber(int64_t value);
	// "0 bytes", "1 byte", "1,234 bytes"
	std::wstring FormatSize(int64_t bytes);
	// the system's message for a Win32 error, without the trailing line break
	std::wstring ErrorText(DWORD error);
	// the last component of a path ("C:\a\b.txt" -> "b.txt"; "C:\" stays)
	std::wstring FileName(std::wstring const& path);
	// the full path with no trailing separator (except a root's: "C:\")
	std::wstring NormalizePath(std::wstring const& path);

	// a most-recently-used list: the path moves (or goes) to the front, compared without case; at most max entries
	void AddRecent(std::vector<std::wstring>& list, std::wstring const& path, size_t max);
	// true if it was there
	bool RemoveRecent(std::vector<std::wstring>& list, std::wstring const& path);
	// a menu item's text: "&1 C:\a&&b.txt" (the number a mnemonic, '&' doubled)
	std::wstring RecentMenuText(size_t index, std::wstring const& path);

	// paths in one string, each ending with a null (WM_COPYDATA's payload: paths from another instance), and back;
	// size in bytes, as WM_COPYDATA has it. Empty paths are left out
	std::wstring JoinPaths(std::vector<std::wstring> const& paths);
	std::vector<std::wstring> SplitPaths(const void* data, size_t size);
}

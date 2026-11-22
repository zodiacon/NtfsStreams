#pragma once

#include <expected>
#include <functional>
#include <span>
#include <stop_token>
#include <atomic>

//
// NTFS alternate data streams: the app's non-UI core (no windows here, so the tests can call all of it).
// Errors are Win32 error codes. A stream is named by its bare name ("Zone.Identifier"); the raw names FindFirstStreamW
// gives (":Zone.Identifier:$DATA") are turned into bare ones, and the default stream (unnamed, "::$DATA") is left out.
//
namespace Streams {
	struct StreamInfo {
		std::wstring Name;	// bare: no leading ':' and no ":$DATA"
		int64_t Size{ 0 };
	};

	// a file (or directory) with at least one alternate stream
	struct FileStreams {
		std::wstring Path;
		std::vector<StreamInfo> Streams;
		bool IsDirectory{ false };

		int64_t TotalSize() const;
	};

	// the bare name of a raw one (":name:$DATA" -> "name"); a name without the decorations stays as it is
	std::wstring BareName(std::wstring_view rawName);
	// "path:name": what CreateFile opens to get at the stream
	std::wstring StreamPath(std::wstring_view path, std::wstring_view name);
	// a name a stream can have: not empty, no ':', '\', '/' or control characters, at most 255 characters
	bool IsValidStreamName(std::wstring_view name);

	// the file's alternate data streams, by name. No streams at all (a directory without any) is an empty list, not an error
	std::expected<std::vector<StreamInfo>, DWORD> GetStreams(std::wstring const& path);

	// the stream's first maxBytes bytes (all of them by default)
	std::expected<std::vector<uint8_t>, DWORD> ReadStream(std::wstring const& path, std::wstring_view name, size_t maxBytes = SIZE_MAX);
	// creates the stream, or replaces its contents
	DWORD WriteStream(std::wstring const& path, std::wstring_view name, std::span<const uint8_t> data);
	DWORD DeleteStream(std::wstring const& path, std::wstring_view name);
	// fails with ERROR_ALREADY_EXISTS if a stream has the new name, unless replace
	DWORD RenameStream(std::wstring const& path, std::wstring_view name, std::wstring_view newName, bool replace = false);

	struct ScanOptions {
		bool Recursive{ false };
		bool IncludeDirectories{ true };	// directories' own streams
		// if set, counts the files looked at as the scan goes (ScanResult::Files at the end), for a progress display
		std::atomic<uint32_t>* Scanned{ nullptr };
	};

	struct ScanResult {
		uint32_t Files{ 0 };		// files and directories looked at
		uint32_t Errors{ 0 };		// those whose streams couldn't be read (access denied, mostly)
		bool Canceled{ false };
	};

	//
	// every file in the folder (and its subfolders, with Recursive) that has alternate streams, given to found as it's
	// found, so a UI can show them while the scan goes on (it's called on the scanning thread). Reparse points (junctions,
	// symbolic links) aren't followed. Stops early when stop is requested.
	//
	ScanResult ScanFolder(std::wstring const& folder, ScanOptions const& options, std::function<void(FileStreams&&)> const& found,
		std::stop_token stop = {});
}

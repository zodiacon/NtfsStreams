#include "pch.h"
#include "Streams.h"

using namespace Streams;

namespace {
	struct FindStreamCloser {
		void operator()(HANDLE h) const noexcept {
			::FindClose(h);
		}
	};
	using unique_hfindstream = std::unique_ptr<std::remove_pointer_t<HANDLE>, FindStreamCloser>;

	bool IsDots(PCWSTR name) {
		return name[0] == L'.' && (name[1] == 0 || (name[1] == L'.' && name[2] == 0));
	}

	ScanResult& Scan(std::wstring const& folder, ScanOptions const& options, std::function<void(FileStreams&&)> const& found,
		std::stop_token const& stop, ScanResult& result) {
		WIN32_FIND_DATA fd;
		wil::unique_hfind hFind(::FindFirstFileEx((folder + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr,
			FIND_FIRST_EX_LARGE_FETCH));
		if (!hFind) {
			result.Errors++;
			return result;
		}
		std::vector<std::wstring> subfolders;
		do {
			if (stop.stop_requested()) {
				result.Canceled = true;
				return result;
			}
			if (IsDots(fd.cFileName))
				continue;

			bool dir = fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			auto path = folder + L"\\" + fd.cFileName;
			if (dir && options.Recursive && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
				subfolders.push_back(path);
			if (dir && !options.IncludeDirectories)
				continue;

			result.Files++;
			auto streams = GetStreams(path);
			if (!streams) {
				result.Errors++;
				continue;
			}
			if (!streams->empty())
				found(FileStreams{ std::move(path), std::move(*streams), dir });
		} while (::FindNextFile(hFind.get(), &fd));

		for (auto& sub : subfolders) {
			Scan(sub, options, found, stop, result);
			if (result.Canceled)
				break;
		}
		return result;
	}
}

int64_t FileStreams::TotalSize() const {
	int64_t total = 0;
	for (auto& s : Streams)
		total += s.Size;
	return total;
}

std::wstring Streams::BareName(std::wstring_view rawName) {
	if (rawName.starts_with(L':'))
		rawName.remove_prefix(1);
	if (rawName.ends_with(L":$DATA"))
		rawName.remove_suffix(6);
	return std::wstring(rawName);
}

std::wstring Streams::StreamPath(std::wstring_view path, std::wstring_view name) {
	std::wstring result(path);
	result += L':';
	result += name;
	return result;
}

bool Streams::IsValidStreamName(std::wstring_view name) {
	if (name.empty() || name.size() > 255)
		return false;
	return std::ranges::none_of(name, [](wchar_t c) { return c == L':' || c == L'\\' || c == L'/' || c < 32; });
}

std::expected<std::vector<StreamInfo>, DWORD> Streams::GetStreams(std::wstring const& path) {
	WIN32_FIND_STREAM_DATA data;
	unique_hfindstream hFind(::FindFirstStreamW(path.c_str(), FindStreamInfoStandard, &data, 0));
	if (hFind.get() == INVALID_HANDLE_VALUE) {
		hFind.release();
		auto error = ::GetLastError();
		// no streams at all (a directory without named streams)
		if (error == ERROR_HANDLE_EOF)
			return std::vector<StreamInfo>{};
		return std::unexpected(error);
	}

	std::vector<StreamInfo> streams;
	do {
		auto name = BareName(data.cStreamName);
		// the default stream ("::$DATA" -> "")
		if (name.empty())
			continue;
		streams.push_back({ std::move(name), data.StreamSize.QuadPart });
	} while (::FindNextStreamW(hFind.get(), &data));
	return streams;
}

std::expected<std::vector<uint8_t>, DWORD> Streams::ReadStream(std::wstring const& path, std::wstring_view name, size_t maxBytes) {
	wil::unique_hfile hFile(::CreateFile(StreamPath(path, name).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
	if (!hFile)
		return std::unexpected(::GetLastError());

	LARGE_INTEGER size;
	if (!::GetFileSizeEx(hFile.get(), &size))
		return std::unexpected(::GetLastError());
	if ((uint64_t)size.QuadPart > SIZE_MAX)
		return std::unexpected((DWORD)ERROR_NOT_ENOUGH_MEMORY);

	auto count = (std::min)((size_t)size.QuadPart, maxBytes);
	std::vector<uint8_t> buffer(count);
	size_t total = 0;
	while (total < count) {
		DWORD chunk = (DWORD)(std::min)(count - total, (size_t)1 << 30);
		DWORD read;
		if (!::ReadFile(hFile.get(), buffer.data() + total, chunk, &read, nullptr))
			return std::unexpected(::GetLastError());
		if (read == 0)
			break;
		total += read;
	}
	buffer.resize(total);
	return buffer;
}

DWORD Streams::WriteStream(std::wstring const& path, std::wstring_view name, std::span<const uint8_t> data) {
	if (!IsValidStreamName(name))
		return ERROR_INVALID_NAME;

	wil::unique_hfile hFile(::CreateFile(StreamPath(path, name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
	if (!hFile)
		return ::GetLastError();

	size_t total = 0;
	while (total < data.size()) {
		DWORD chunk = (DWORD)(std::min)(data.size() - total, (size_t)1 << 30);
		DWORD written;
		if (!::WriteFile(hFile.get(), data.data() + total, chunk, &written, nullptr))
			return ::GetLastError();
		total += written;
	}
	return ERROR_SUCCESS;
}

DWORD Streams::DeleteStream(std::wstring const& path, std::wstring_view name) {
	if (!IsValidStreamName(name))
		return ERROR_INVALID_NAME;
	return ::DeleteFile(StreamPath(path, name).c_str()) ? ERROR_SUCCESS : ::GetLastError();
}

namespace {
	// a stream is renamed to ":newname:$DATA", within the same file. NTFS refuses to replace a stream that isn't empty
	// (ERROR_INVALID_PARAMETER, even with ReplaceIfExists), so this never replaces: an existing target is ERROR_ALREADY_EXISTS
	DWORD RenameNoReplace(std::wstring const& path, std::wstring_view name, std::wstring_view newName) {
		wil::unique_hfile hFile(::CreateFile(StreamPath(path, name).c_str(), DELETE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
			OPEN_EXISTING, 0, nullptr));
		if (!hFile)
			return ::GetLastError();

		auto target = L":" + std::wstring(newName) + L":$DATA";
		auto bytes = sizeof(FILE_RENAME_INFO) + target.size() * sizeof(WCHAR);
		auto buffer = std::make_unique<BYTE[]>(bytes);
		auto info = reinterpret_cast<FILE_RENAME_INFO*>(buffer.get());
		info->ReplaceIfExists = FALSE;
		info->RootDirectory = nullptr;
		info->FileNameLength = DWORD(target.size() * sizeof(WCHAR));
		::memcpy(info->FileName, target.c_str(), (target.size() + 1) * sizeof(WCHAR));
		if (!::SetFileInformationByHandle(hFile.get(), FileRenameInfo, info, (DWORD)bytes))
			return ::GetLastError();
		return ERROR_SUCCESS;
	}
}

DWORD Streams::RenameStream(std::wstring const& path, std::wstring_view name, std::wstring_view newName, bool replace) {
	if (!IsValidStreamName(name) || !IsValidStreamName(newName))
		return ERROR_INVALID_NAME;
	if (name == newName)
		return ERROR_SUCCESS;

	auto error = RenameNoReplace(path, name, newName);
	if (error != ERROR_ALREADY_EXISTS || !replace)
		return error;

	// the target moves aside first, and comes back if the rename still fails
	GUID guid;
	::CoCreateGuid(&guid);
	WCHAR aside[64];
	::StringFromGUID2(guid, aside, _countof(aside));
	if (error = RenameNoReplace(path, newName, aside); error != ERROR_SUCCESS)
		return error;
	if (error = RenameNoReplace(path, name, newName); error != ERROR_SUCCESS) {
		RenameNoReplace(path, aside, newName);
		return error;
	}
	DeleteStream(path, aside);
	return ERROR_SUCCESS;
}

ScanResult Streams::ScanFolder(std::wstring const& folder, ScanOptions const& options, std::function<void(FileStreams&&)> const& found,
	std::stop_token stop) {
	ScanResult result;
	auto root = folder;
	while (root.size() > 3 && (root.back() == L'\\' || root.back() == L'/'))
		root.pop_back();
	// "C:\" -> "C:" so the paths below don't get a double backslash
	if (root.size() == 3 && root[1] == L':' && root[2] == L'\\')
		root.pop_back();
	return Scan(root, options, found, stop, result);
}

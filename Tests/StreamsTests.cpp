#include "pch.h"
#include "TestFramework.h"
#include "Streams.h"

using namespace Streams;

namespace {
	// a new folder in %TEMP%, removed with everything in it at the end of the test
	struct TempFolder {
		TempFolder() {
			WCHAR temp[MAX_PATH];
			::GetTempPath(_countof(temp), temp);
			GUID guid;
			::CoCreateGuid(&guid);
			WCHAR name[64];
			::StringFromGUID2(guid, name, _countof(name));
			Path = std::wstring(temp) + L"NtfsStreamsTests" + name;
			::CreateDirectory(Path.c_str(), nullptr);
		}
		~TempFolder() {
			std::error_code ec;
			std::filesystem::remove_all(Path, ec);
		}

		// an empty file (or with the text as its default stream) in the folder
		std::wstring CreateFile(std::wstring const& name, std::string_view text = {}) const {
			auto path = Path + L"\\" + name;
			wil::unique_hfile hFile(::CreateFile(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
			DWORD written;
			if (!text.empty())
				::WriteFile(hFile.get(), text.data(), (DWORD)text.size(), &written, nullptr);
			return path;
		}

		std::wstring CreateFolder(std::wstring const& name) const {
			auto path = Path + L"\\" + name;
			::CreateDirectory(path.c_str(), nullptr);
			return path;
		}

		std::wstring Path;
	};

	std::span<const uint8_t> Bytes(std::string_view text) {
		return { reinterpret_cast<const uint8_t*>(text.data()), text.size() };
	}

	std::string Text(std::vector<uint8_t> const& bytes) {
		return std::string(bytes.begin(), bytes.end());
	}

	std::vector<std::wstring> Names(std::vector<StreamInfo> const& streams) {
		std::vector<std::wstring> names;
		for (auto& s : streams)
			names.push_back(s.Name);
		std::ranges::sort(names);
		return names;
	}

	std::vector<std::wstring> Paths(std::vector<FileStreams> const& files) {
		std::vector<std::wstring> paths;
		for (auto& f : files)
			paths.push_back(f.Path);
		std::ranges::sort(paths);
		return paths;
	}
}

TEST(BareNameStripsDecorations) {
	CHECK_EQ(BareName(L":Zone.Identifier:$DATA"), std::wstring(L"Zone.Identifier"));
	CHECK_EQ(BareName(L"::$DATA"), std::wstring());
	CHECK_EQ(BareName(L"plain"), std::wstring(L"plain"));
}

TEST(StreamPathJoinsWithColon) {
	CHECK_EQ(StreamPath(L"C:\\a.txt", L"s"), std::wstring(L"C:\\a.txt:s"));
}

TEST(ValidStreamNames) {
	CHECK(IsValidStreamName(L"Zone.Identifier"));
	CHECK(IsValidStreamName(L"with space"));
	CHECK(!IsValidStreamName(L""));
	CHECK(!IsValidStreamName(L"a:b"));
	CHECK(!IsValidStreamName(L"a\\b"));
	CHECK(!IsValidStreamName(L"a/b"));
	CHECK(!IsValidStreamName(L"a\tb"));
	CHECK(!IsValidStreamName(std::wstring(256, L'x')));
	CHECK(IsValidStreamName(std::wstring(255, L'x')));
}

TEST(FileWithoutStreamsHasNone) {
	TempFolder folder;
	auto path = folder.CreateFile(L"plain.txt", "default stream data");
	auto streams = GetStreams(path);
	CHECK(streams.has_value());
	CHECK(streams && streams->empty());
}

TEST(MissingFileIsAnError) {
	TempFolder folder;
	auto streams = GetStreams(folder.Path + L"\\nothere.txt");
	CHECK(!streams.has_value());
	CHECK(!streams && streams.error() == ERROR_FILE_NOT_FOUND);
}

TEST(WriteThenListAndRead) {
	TempFolder folder;
	auto path = folder.CreateFile(L"a.txt", "main");
	CHECK_EQ(WriteStream(path, L"one", Bytes("hello")), (DWORD)ERROR_SUCCESS);
	CHECK_EQ(WriteStream(path, L"two", Bytes("")), (DWORD)ERROR_SUCCESS);

	auto streams = GetStreams(path);
	CHECK(streams.has_value());
	if (!streams)
		return;
	CHECK(Names(*streams) == (std::vector<std::wstring>{ L"one", L"two" }));
	for (auto& s : *streams)
		CHECK_EQ(s.Size, s.Name == L"one" ? 5LL : 0LL);

	auto data = ReadStream(path, L"one");
	CHECK(data.has_value());
	CHECK(data && Text(*data) == "hello");
	auto empty = ReadStream(path, L"two");
	CHECK(empty && empty->empty());
	// the default stream is untouched
	auto main = ReadStream(path, L":$DATA");
	CHECK(!main.has_value() || Text(*main) == "main");
}

TEST(ReadStreamHonorsMaxBytes) {
	TempFolder folder;
	auto path = folder.CreateFile(L"a.txt");
	WriteStream(path, L"s", Bytes("0123456789"));
	auto data = ReadStream(path, L"s", 4);
	CHECK(data && Text(*data) == "0123");
}

TEST(WriteReplacesContents) {
	TempFolder folder;
	auto path = folder.CreateFile(L"a.txt");
	WriteStream(path, L"s", Bytes("a long first value"));
	WriteStream(path, L"s", Bytes("short"));
	auto data = ReadStream(path, L"s");
	CHECK(data && Text(*data) == "short");
}

TEST(WriteRejectsInvalidName) {
	TempFolder folder;
	auto path = folder.CreateFile(L"a.txt");
	CHECK_EQ(WriteStream(path, L"bad:name", Bytes("x")), (DWORD)ERROR_INVALID_NAME);
	auto streams = GetStreams(path);
	CHECK(streams && streams->empty());
}

TEST(DeleteStream) {
	TempFolder folder;
	auto path = folder.CreateFile(L"a.txt", "main");
	WriteStream(path, L"s", Bytes("x"));
	CHECK_EQ(DeleteStream(path, L"s"), (DWORD)ERROR_SUCCESS);
	auto streams = GetStreams(path);
	CHECK(streams && streams->empty());
	// the file itself stays
	CHECK(::GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES);
	CHECK_EQ(DeleteStream(path, L"s"), (DWORD)ERROR_FILE_NOT_FOUND);
}

TEST(RenameStream) {
	TempFolder folder;
	auto path = folder.CreateFile(L"a.txt");
	WriteStream(path, L"old", Bytes("data"));
	CHECK_EQ(RenameStream(path, L"old", L"new"), (DWORD)ERROR_SUCCESS);
	auto streams = GetStreams(path);
	CHECK(streams && Names(*streams) == std::vector<std::wstring>{ L"new" });
	auto data = ReadStream(path, L"new");
	CHECK(data && Text(*data) == "data");
}

TEST(RenameStreamOntoExisting) {
	TempFolder folder;
	auto path = folder.CreateFile(L"a.txt");
	WriteStream(path, L"a", Bytes("A"));
	WriteStream(path, L"b", Bytes("B"));
	CHECK_EQ(RenameStream(path, L"a", L"b"), (DWORD)ERROR_ALREADY_EXISTS);
	CHECK_EQ(RenameStream(path, L"a", L"b", true), (DWORD)ERROR_SUCCESS);
	auto streams = GetStreams(path);
	CHECK(streams && Names(*streams) == std::vector<std::wstring>{ L"b" });
	auto data = ReadStream(path, L"b");
	CHECK(data && Text(*data) == "A");
}

TEST(DirectoryStreams) {
	TempFolder folder;
	auto dir = folder.CreateFolder(L"sub");
	auto none = GetStreams(dir);
	CHECK(none && none->empty());
	CHECK_EQ(WriteStream(dir, L"s", Bytes("on a directory")), (DWORD)ERROR_SUCCESS);
	auto streams = GetStreams(dir);
	CHECK(streams && Names(*streams) == std::vector<std::wstring>{ L"s" });
	auto data = ReadStream(dir, L"s");
	CHECK(data && Text(*data) == "on a directory");
}

TEST(ScanFolderFindsFilesWithStreams) {
	TempFolder folder;
	auto a = folder.CreateFile(L"a.txt");
	folder.CreateFile(L"b.txt");
	auto sub = folder.CreateFolder(L"sub");
	auto c = folder.CreateFile(L"sub\\c.txt");
	WriteStream(a, L"s", Bytes("1"));
	WriteStream(c, L"s1", Bytes("12"));
	WriteStream(c, L"s2", Bytes("345"));

	std::vector<FileStreams> found;
	auto collect = [&](FileStreams&& f) { found.push_back(std::move(f)); };

	auto result = ScanFolder(folder.Path, {}, collect);
	CHECK(Paths(found) == std::vector<std::wstring>{ a });
	CHECK_EQ(result.Files, 3u);	// a.txt, b.txt, sub
	CHECK_EQ(result.Errors, 0u);
	CHECK(!result.Canceled);

	found.clear();
	result = ScanFolder(folder.Path + L"\\", { .Recursive = true }, collect);
	CHECK(Paths(found) == (std::vector<std::wstring>{ a, c }));
	CHECK_EQ(result.Files, 4u);
	for (auto& f : found)
		if (f.Path == c)
			CHECK_EQ(f.TotalSize(), 5LL);

	// a directory's own streams
	WriteStream(sub, L"d", Bytes("x"));
	found.clear();
	ScanFolder(folder.Path, {}, collect);
	CHECK(Paths(found) == (std::vector<std::wstring>{ a, sub }));
	for (auto& f : found)
		CHECK_EQ(f.IsDirectory, f.Path == sub);
	found.clear();
	ScanFolder(folder.Path, { .IncludeDirectories = false }, collect);
	CHECK(Paths(found) == std::vector<std::wstring>{ a });
}

TEST(ScanFolderCountsAsItGoes) {
	TempFolder folder;
	folder.CreateFile(L"a.txt");
	folder.CreateFile(L"b.txt");
	folder.CreateFolder(L"sub");
	folder.CreateFile(L"sub\\c.txt");
	std::atomic<uint32_t> scanned{ 0 };
	auto result = ScanFolder(folder.Path, { .Recursive = true, .Scanned = &scanned }, [](FileStreams&&) {});
	CHECK_EQ(result.Files, 4u);
	CHECK_EQ(scanned.load(), 4u);
}

TEST(ScanFolderCanBeCanceled) {
	TempFolder folder;
	WriteStream(folder.CreateFile(L"a.txt"), L"s", Bytes("1"));
	std::stop_source stop;
	stop.request_stop();
	int found = 0;
	auto result = ScanFolder(folder.Path, {}, [&](FileStreams&&) { found++; }, stop.get_token());
	CHECK(result.Canceled);
	CHECK_EQ(found, 0);
}

TEST(ScanMissingFolderIsAnError) {
	TempFolder folder;
	auto result = ScanFolder(folder.Path + L"\\nothere", {}, [](FileStreams&&) {});
	CHECK_EQ(result.Errors, 1u);
	CHECK_EQ(result.Files, 0u);
}

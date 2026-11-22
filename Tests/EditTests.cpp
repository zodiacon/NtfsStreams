#include "pch.h"
#include "TestFramework.h"
#include "Streams.h"
#include "TextDecode.h"

using namespace Streams;

namespace {
	struct TempFile {
		TempFile() {
			WCHAR temp[MAX_PATH], name[MAX_PATH];
			::GetTempPath(_countof(temp), temp);
			::GetTempFileName(temp, L"nts", 0, name);
			Path = name;
		}
		~TempFile() {
			::DeleteFile(Path.c_str());
		}
		std::wstring Path;
	};

	std::span<const uint8_t> Bytes(std::string_view text) {
		return { reinterpret_cast<const uint8_t*>(text.data()), text.size() };
	}

	// the file's last write time, set to a day in 2001 first if asked
	int64_t LastWrite(std::wstring const& path, bool setOld = false) {
		wil::unique_hfile file(::CreateFile(path.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
		FILETIME ft{};
		if (setOld) {
			SYSTEMTIME st{ 2001, 9, 0, 9, 12, 0, 0, 0 };
			::SystemTimeToFileTime(&st, &ft);
			::SetFileTime(file.get(), nullptr, nullptr, &ft);
		}
		::GetFileTime(file.get(), nullptr, nullptr, &ft);
		return (int64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	}
}

TEST(WritingAStreamChangesTheFileTime) {
	// what FileTimesKeeper is for: without it, the time moves
	TempFile file;
	auto old = LastWrite(file.Path, true);
	WriteStream(file.Path, L"s", Bytes("data"));
	CHECK(LastWrite(file.Path) != old);
}

TEST(FileTimesKeeperKeepsTimes) {
	TempFile file;
	auto old = LastWrite(file.Path, true);
	{
		FileTimesKeeper keep(file.Path);
		CHECK_EQ(WriteStream(file.Path, L"s", Bytes("data")), (DWORD)ERROR_SUCCESS);
		CHECK_EQ(RenameStream(file.Path, L"s", L"t"), (DWORD)ERROR_SUCCESS);
	}
	CHECK_EQ(LastWrite(file.Path), old);
	{
		FileTimesKeeper keep(file.Path);
		CHECK_EQ(DeleteStream(file.Path, L"t"), (DWORD)ERROR_SUCCESS);
	}
	CHECK_EQ(LastWrite(file.Path), old);
	// not enabled: the time moves
	{
		FileTimesKeeper keep(file.Path, false);
		WriteStream(file.Path, L"u", Bytes("data"));
	}
	CHECK(LastWrite(file.Path) != old);
}

TEST(EncodeText) {
	using namespace TextDecode;
	auto utf8 = Encode(L"café", Encoding::Utf8, false);
	CHECK(utf8 == (std::vector<uint8_t>{ 'c', 'a', 'f', 0xC3, 0xA9 }));
	CHECK(Encode(L"hi", Encoding::Utf8, true) == (std::vector<uint8_t>{ 0xEF, 0xBB, 0xBF, 'h', 'i' }));
	CHECK(Encode(L"hi", Encoding::Auto, false) == (std::vector<uint8_t>{ 'h', 'i' }));
	CHECK(Encode(L"hi", Encoding::Utf16, true) == (std::vector<uint8_t>{ 0xFF, 0xFE, 'h', 0, 'i', 0 }));
	CHECK(Encode(L"café", Encoding::Ascii, true) == (std::vector<uint8_t>{ 'c', 'a', 'f', '?' }));
	CHECK(Encode(L"", Encoding::Utf8, false).empty());

	// round trips, with the byte order mark noticed
	auto text = std::wstring(L"[ZoneTransfer]\r\nZoneId=3\r\nשלום");
	for (auto encoding : { Encoding::Utf8, Encoding::Utf16 }) {
		auto bytes = Encode(text, encoding, true);
		CHECK(HasBom(bytes));
		CHECK_EQ(Detect(bytes), encoding);
		CHECK_EQ(Decode(bytes, Encoding::Auto), text);
	}
	CHECK(!HasBom(Encode(text, Encoding::Utf8, false)));
}

TEST(DecodeCountsReplaced) {
	using namespace TextDecode;
	size_t replaced = 99;
	Decode(Bytes("text\r\nonly\t."), Encoding::Ascii, &replaced);
	CHECK_EQ(replaced, 0u);
	Decode(Bytes(std::string_view("a\x01" "b\x00" "c\xFF", 6)), Encoding::Ascii, &replaced);
	CHECK_EQ(replaced, 3u);
	Decode(Bytes("bad \xC3 utf8"), Encoding::Utf8, &replaced);
	CHECK_EQ(replaced, 1u);
	Decode(Bytes("caf\xC3\xA9"), Encoding::Utf8, &replaced);
	CHECK_EQ(replaced, 0u);
}

TEST(ImportFileIntoStream) {
	TempFile file, source;
	{
		std::ofstream out(source.Path, std::ios::binary);
		out << "imported contents";
	}
	CHECK_EQ(ImportFile(file.Path, L"imp", source.Path), (DWORD)ERROR_SUCCESS);
	auto data = ReadStream(file.Path, L"imp");
	CHECK(data && std::string(data->begin(), data->end()) == "imported contents");
	CHECK_EQ(ImportFile(file.Path, L"bad:name", source.Path), (DWORD)ERROR_INVALID_NAME);
	CHECK_EQ(ImportFile(file.Path, L"x", source.Path + L"-missing"), (DWORD)ERROR_FILE_NOT_FOUND);
}

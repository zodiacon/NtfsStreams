#include "pch.h"
#include "TestFramework.h"
#include "Streams.h"
#include "StreamBuffer.h"
#include "TextDecode.h"

using namespace TextDecode;

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

	std::vector<uint8_t> Utf16(std::wstring_view text, bool bom = false) {
		std::vector<uint8_t> bytes;
		if (bom)
			bytes = { 0xFF, 0xFE };
		auto p = reinterpret_cast<const uint8_t*>(text.data());
		bytes.insert(bytes.end(), p, p + text.size() * 2);
		return bytes;
	}
}

TEST(StreamBufferReadsAcrossChunks) {
	TempFile file;
	// a pattern longer than two chunks, so reads cross chunk boundaries
	std::vector<uint8_t> data(StreamBuffer::ChunkSize * 2 + 1000);
	for (size_t i = 0; i < data.size(); i++)
		data[i] = uint8_t(i * 7 + i / 251);
	CHECK_EQ(Streams::WriteStream(file.Path, L"big", data), (DWORD)ERROR_SUCCESS);

	DWORD error;
	auto buffer = StreamBuffer::Open(file.Path, L"big", error);
	CHECK(buffer != nullptr);
	if (!buffer)
		return;
	CHECK_EQ(buffer->GetSize(), (int64_t)data.size());
	CHECK(buffer->IsReadOnly());

	for (int64_t offset : { 0LL, 100LL, (long long)StreamBuffer::ChunkSize - 10, (long long)StreamBuffer::ChunkSize * 2 - 3, (long long)data.size() - 5 }) {
		uint8_t chunk[64]{};
		auto read = buffer->GetData(offset, chunk, sizeof(chunk));
		auto expected = (uint32_t)(std::min<int64_t>)(sizeof(chunk), (int64_t)data.size() - offset);
		CHECK_EQ(read, expected);
		CHECK(std::equal(chunk, chunk + read, data.begin() + offset));
	}
	uint8_t byte;
	CHECK_EQ(buffer->GetData((int64_t)data.size(), &byte, 1), 0u);
	CHECK(!buffer->SetData(0, &byte, 1));
}

TEST(StreamBufferMissingStream) {
	TempFile file;
	DWORD error = 0;
	auto buffer = StreamBuffer::Open(file.Path, L"nothere", error);
	CHECK(buffer == nullptr);
	CHECK_EQ(error, (DWORD)ERROR_FILE_NOT_FOUND);
}

TEST(DetectEncoding) {
	CHECK_EQ(Detect(Bytes("plain text\r\n")), Encoding::Ascii);
	CHECK_EQ(Detect(Bytes("caf\xC3\xA9")), Encoding::Utf8);
	CHECK_EQ(Detect(Bytes("\xEF\xBB\xBFhi")), Encoding::Utf8);
	CHECK_EQ(Detect(Utf16(L"hello, world")), Encoding::Utf16);
	CHECK_EQ(Detect(Utf16(L"x", true)), Encoding::Utf16);
	// invalid UTF-8 with high bytes: shown as ASCII ('.')
	CHECK_EQ(Detect(Bytes("\xFF\x80\x81 binary")), Encoding::Ascii);
	CHECK_EQ(Detect({}), Encoding::Ascii);
}

TEST(DecodeText) {
	// line breaks as CR LF, controls as '.'
	CHECK_EQ(Decode(Bytes("a\nb\rc\r\nd\x01\te"), Encoding::Ascii), std::wstring(L"a\r\nb\r\nc\r\nd.\te"));
	CHECK_EQ(Decode(Bytes("caf\xC3\xA9"), Encoding::Ascii), std::wstring(L"caf.."));
	CHECK_EQ(Decode(Bytes("caf\xC3\xA9"), Encoding::Utf8), std::wstring(L"caf\u00E9"));
	CHECK_EQ(Decode(Bytes("\xEF\xBB\xBFhi"), Encoding::Auto), std::wstring(L"hi"));
	CHECK_EQ(Decode(Utf16(L"hi\n", true), Encoding::Auto), std::wstring(L"hi\r\n"));
	// an odd last byte in UTF-16 is dropped
	auto odd = Utf16(L"ab");
	odd.push_back('c');
	CHECK_EQ(Decode(odd, Encoding::Utf16), std::wstring(L"ab"));
	CHECK_EQ(Decode(Bytes("[ZoneTransfer]\r\nZoneId=3"), Encoding::Auto), std::wstring(L"[ZoneTransfer]\r\nZoneId=3"));
}

TEST(ExportStreamCopiesContents) {
	TempFile file, target;
	std::vector<uint8_t> data((1 << 20) + 17);
	for (size_t i = 0; i < data.size(); i++)
		data[i] = uint8_t(i % 253);
	Streams::WriteStream(file.Path, L"s", data);
	CHECK_EQ(Streams::ExportStream(file.Path, L"s", target.Path), (DWORD)ERROR_SUCCESS);

	std::ifstream in(target.Path, std::ios::binary);
	std::vector<uint8_t> copy((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	CHECK(copy == data);
	CHECK_EQ(Streams::ExportStream(file.Path, L"nothere", target.Path), (DWORD)ERROR_FILE_NOT_FOUND);
}

TEST(ExportFileName) {
	CHECK_EQ(Streams::ExportFileName(L"C:\\x\\a.txt", L"Zone.Identifier"), std::wstring(L"a.txt_Zone.Identifier"));
	CHECK_EQ(Streams::ExportFileName(L"C:\\x\\a.txt", L"what?*<>|\""), std::wstring(L"a.txt_what______"));
}

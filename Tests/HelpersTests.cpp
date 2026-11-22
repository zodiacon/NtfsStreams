#include "pch.h"
#include "TestFramework.h"
#include "Helpers.h"

using namespace Helpers;

TEST(AddRecentMovesToFront) {
	std::vector<std::wstring> list;
	AddRecent(list, L"a", 3);
	AddRecent(list, L"b", 3);
	AddRecent(list, L"c", 3);
	CHECK(list == (std::vector<std::wstring>{ L"c", L"b", L"a" }));
	// already there (without case): moves, isn't duplicated
	AddRecent(list, L"A", 3);
	CHECK(list == (std::vector<std::wstring>{ L"A", L"c", L"b" }));
	// past the maximum the oldest goes
	AddRecent(list, L"d", 3);
	CHECK(list == (std::vector<std::wstring>{ L"d", L"A", L"c" }));
}

TEST(RemoveRecent) {
	std::vector<std::wstring> list{ L"C:\\One", L"C:\\Two" };
	CHECK(RemoveRecent(list, L"c:\\one"));
	CHECK(list == std::vector<std::wstring>{ L"C:\\Two" });
	CHECK(!RemoveRecent(list, L"C:\\Three"));
}

TEST(RecentMenuTextEscapesAmpersand) {
	CHECK_EQ(RecentMenuText(0, L"C:\\a&b.txt"), std::wstring(L"&1 C:\\a&&b.txt"));
	CHECK_EQ(RecentMenuText(9, L"x"), std::wstring(L"10 x"));
}

TEST(NormalizePath) {
	CHECK_EQ(NormalizePath(L"C:\\Windows\\"), std::wstring(L"C:\\Windows"));
	CHECK_EQ(NormalizePath(L"C:\\"), std::wstring(L"C:\\"));
	CHECK_EQ(NormalizePath(L"C:\\Windows\\System32\\..\\"), std::wstring(L"C:\\Windows"));
}

TEST(FileName) {
	CHECK_EQ(FileName(L"C:\\a\\b.txt"), std::wstring(L"b.txt"));
	CHECK_EQ(FileName(L"C:\\"), std::wstring(L"C:\\"));
}

TEST(FormatSize) {
	CHECK_EQ(FormatSize(1), std::wstring(L"1 byte"));
	CHECK_EQ(FormatSize(0), std::wstring(L"0 bytes"));
	// the separator is the user's; the digits are all there
	auto text = FormatSize(1234567);
	CHECK(text.starts_with(L"1") && text.find(L"234") != std::wstring::npos && text.ends_with(L"567 bytes"));
}

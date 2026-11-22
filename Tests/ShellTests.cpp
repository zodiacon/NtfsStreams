#include "pch.h"
#include "TestFramework.h"
#include "ExplorerMenu.h"
#include "Helpers.h"

namespace {
	// under HKCU, removed with everything under it at the end: never the real Software\Classes
	struct TempKey {
		TempKey() {
			GUID guid;
			::CoCreateGuid(&guid);
			WCHAR name[64];
			::StringFromGUID2(guid, name, _countof(name));
			Path = std::wstring(L"Software\\NtfsStreamsTests\\") + name;
		}
		~TempKey() {
			::RegDeleteTree(HKEY_CURRENT_USER, L"Software\\NtfsStreamsTests");
			::RegDeleteKey(HKEY_CURRENT_USER, L"Software\\NtfsStreamsTests");
		}
		std::wstring Path;
	};

	std::wstring Value(std::wstring const& key, PCWSTR name = nullptr) {
		WCHAR value[1024];
		DWORD size = sizeof(value);
		if (::RegGetValue(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS)
			return {};
		return value;
	}
}

TEST(ExplorerMenuRegisters) {
	TempKey root;
	auto exe = std::wstring(L"C:\\Tools\\NtfsStreams.exe");
	CHECK(!ExplorerMenu::IsRegistered(exe, root.Path.c_str()));
	CHECK_EQ(ExplorerMenu::Register(exe, root.Path.c_str()), (DWORD)ERROR_SUCCESS);
	CHECK(ExplorerMenu::IsRegistered(exe, root.Path.c_str()));
	// moved elsewhere: not this exe's entries
	CHECK(!ExplorerMenu::IsRegistered(L"D:\\Other\\NtfsStreams.exe", root.Path.c_str()));

	CHECK_EQ(Value(root.Path + L"\\*\\shell\\NtfsStreams"), std::wstring(L"View NTFS Streams"));
	CHECK_EQ(Value(root.Path + L"\\*\\shell\\NtfsStreams\\command"), std::wstring(L"\"C:\\Tools\\NtfsStreams.exe\" \"%1\""));
	CHECK_EQ(Value(root.Path + L"\\Directory\\shell\\NtfsStreams\\command"), std::wstring(L"\"C:\\Tools\\NtfsStreams.exe\" \"%1\""));
	CHECK_EQ(Value(root.Path + L"\\Directory\\Background\\shell\\NtfsStreams\\command"), std::wstring(L"\"C:\\Tools\\NtfsStreams.exe\" \"%V\""));

	CHECK_EQ(ExplorerMenu::Unregister(root.Path.c_str()), (DWORD)ERROR_SUCCESS);
	CHECK(!ExplorerMenu::IsRegistered(exe, root.Path.c_str()));
	wil::unique_hkey key;
	CHECK(::RegOpenKeyEx(HKEY_CURRENT_USER, (root.Path + L"\\*\\shell\\NtfsStreams").c_str(), 0, KEY_READ, key.addressof()) == ERROR_FILE_NOT_FOUND);
	// nothing to remove is fine
	CHECK_EQ(ExplorerMenu::Unregister(root.Path.c_str()), (DWORD)ERROR_SUCCESS);
}

TEST(JoinAndSplitPaths) {
	std::vector<std::wstring> paths{ L"C:\\a b\\c.txt", L"", L"D:\\folder" };
	auto joined = Helpers::JoinPaths(paths);
	auto split = Helpers::SplitPaths(joined.data(), joined.size() * sizeof(wchar_t));
	CHECK(split == (std::vector<std::wstring>{ L"C:\\a b\\c.txt", L"D:\\folder" }));
	// without the last null, and nothing
	auto noEnd = std::wstring(L"x\0y", 3);
	CHECK(Helpers::SplitPaths(noEnd.data(), noEnd.size() * sizeof(wchar_t)) == (std::vector<std::wstring>{ L"x", L"y" }));
	CHECK(Helpers::SplitPaths(nullptr, 0).empty());
	CHECK(Helpers::SplitPaths(L"", 0).empty());
}

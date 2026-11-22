#include "pch.h"
#include "ExplorerMenu.h"

namespace {
	struct Entry {
		PCWSTR Class;
		bool Background;	// %V (the folder shown) rather than %1 (the item clicked)
	};
	const Entry s_Entries[] = {
		{ L"*", false },
		{ L"Directory", false },
		{ L"Directory\\Background", true },
	};

	std::wstring VerbKey(PCWSTR root, Entry const& entry) {
		return std::format(L"{}\\{}\\shell\\{}", root, entry.Class, ExplorerMenu::VerbName);
	}

	DWORD SetString(HKEY key, PCWSTR name, std::wstring const& value) {
		return ::RegSetValueEx(key, name, 0, REG_SZ, (const BYTE*)value.c_str(), DWORD((value.size() + 1) * sizeof(WCHAR)));
	}

	std::wstring GetString(HKEY key, PCWSTR name) {
		WCHAR value[2048];
		DWORD size = sizeof(value);
		if (::RegGetValue(key, nullptr, name, RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS)
			return {};
		return value;
	}
}

std::wstring ExplorerMenu::Command(std::wstring const& exePath, bool background) {
	return std::format(L"\"{}\" \"{}\"", exePath, background ? L"%V" : L"%1");
}

bool ExplorerMenu::IsRegistered(std::wstring const& exePath, PCWSTR root) {
	for (auto& entry : s_Entries) {
		wil::unique_hkey key;
		if (::RegOpenKeyEx(HKEY_CURRENT_USER, (VerbKey(root, entry) + L"\\command").c_str(), 0, KEY_READ, key.addressof()) != ERROR_SUCCESS)
			return false;
		if (_wcsicmp(GetString(key.get(), nullptr).c_str(), Command(exePath, entry.Background).c_str()) != 0)
			return false;
	}
	return true;
}

DWORD ExplorerMenu::Register(std::wstring const& exePath, PCWSTR root) {
	for (auto& entry : s_Entries) {
		wil::unique_hkey verb;
		auto error = ::RegCreateKeyEx(HKEY_CURRENT_USER, VerbKey(root, entry).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, verb.addressof(), nullptr);
		if (error != ERROR_SUCCESS)
			return error;
		SetString(verb.get(), nullptr, L"View NTFS Streams");
		SetString(verb.get(), L"Icon", std::format(L"\"{}\",0", exePath));

		wil::unique_hkey command;
		error = ::RegCreateKeyEx(verb.get(), L"command", 0, nullptr, 0, KEY_WRITE, nullptr, command.addressof(), nullptr);
		if (error != ERROR_SUCCESS)
			return error;
		error = SetString(command.get(), nullptr, Command(exePath, entry.Background));
		if (error != ERROR_SUCCESS)
			return error;
	}
	return ERROR_SUCCESS;
}

DWORD ExplorerMenu::Unregister(PCWSTR root) {
	DWORD result = ERROR_SUCCESS;
	for (auto& entry : s_Entries) {
		auto error = ::RegDeleteTree(HKEY_CURRENT_USER, VerbKey(root, entry).c_str());
		if (error == ERROR_SUCCESS || error == ERROR_FILE_NOT_FOUND) {
			// the empty verb key goes too (RegDeleteTree leaves the key it's given)
			::RegDeleteKey(HKEY_CURRENT_USER, VerbKey(root, entry).c_str());
			continue;
		}
		result = error;
	}
	return result;
}

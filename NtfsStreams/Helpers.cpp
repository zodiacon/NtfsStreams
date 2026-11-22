#include "pch.h"
#include "Helpers.h"

std::wstring Helpers::FormatNumber(int64_t value) {
	auto digits = std::to_wstring(value);
	NUMBERFMT fmt{};
	WCHAR decimal[8], thousand[8], grouping[16];
	::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, decimal, _countof(decimal));
	::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, thousand, _countof(thousand));
	::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SGROUPING, grouping, _countof(grouping));
	fmt.lpDecimalSep = decimal;
	fmt.lpThousandSep = thousand;
	// "3;0" -> 3, "3;2;0" -> 32 (as NUMBERFMT wants it)
	for (auto p = grouping; *p; p++)
		if (*p >= L'1' && *p <= L'9')
			fmt.Grouping = fmt.Grouping * 10 + (*p - L'0');
	WCHAR result[64];
	if (::GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, digits.c_str(), &fmt, result, _countof(result)) == 0)
		return digits;
	return result;
}

std::wstring Helpers::FormatSize(int64_t bytes) {
	return FormatNumber(bytes) + (bytes == 1 ? L" byte" : L" bytes");
}

std::wstring Helpers::ErrorText(DWORD error) {
	PWSTR buffer = nullptr;
	auto len = ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
		(PWSTR)&buffer, 0, nullptr);
	if (len == 0)
		return std::format(L"Error {}", error);
	std::wstring text(buffer, len);
	::LocalFree(buffer);
	while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L' '))
		text.pop_back();
	return text;
}

std::wstring Helpers::NormalizePath(std::wstring const& path) {
	std::wstring full(MAX_PATH, 0);
	auto len = ::GetFullPathName(path.c_str(), (DWORD)full.size(), full.data(), nullptr);
	if (len > full.size()) {
		full.resize(len);
		len = ::GetFullPathName(path.c_str(), (DWORD)full.size(), full.data(), nullptr);
	}
	if (len == 0)
		full = path;
	else
		full.resize(len);
	while (full.size() > 3 && (full.back() == L'\\' || full.back() == L'/'))
		full.pop_back();
	return full;
}

void Helpers::AddRecent(std::vector<std::wstring>& list, std::wstring const& path, size_t max) {
	RemoveRecent(list, path);
	list.insert(list.begin(), path);
	if (list.size() > max)
		list.resize(max);
}

bool Helpers::RemoveRecent(std::vector<std::wstring>& list, std::wstring const& path) {
	return std::erase_if(list, [&](auto& p) { return _wcsicmp(p.c_str(), path.c_str()) == 0; }) > 0;
}

std::wstring Helpers::RecentMenuText(size_t index, std::wstring const& path) {
	std::wstring escaped;
	for (auto c : path) {
		if (c == L'&')
			escaped += L'&';
		escaped += c;
	}
	return (index < 9 ? std::format(L"&{} ", index + 1) : std::format(L"{} ", index + 1)) + escaped;
}

std::wstring Helpers::FileName(std::wstring const& path) {
	auto pos = path.find_last_of(L"\\/");
	if (pos == std::wstring::npos || pos == path.size() - 1)
		return path;
	return path.substr(pos + 1);
}

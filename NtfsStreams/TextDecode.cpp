#include "pch.h"
#include "TextDecode.h"

using namespace TextDecode;

namespace {
	bool HasUtf8Bom(std::span<const uint8_t> data) {
		return data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF;
	}

	bool HasUtf16Bom(std::span<const uint8_t> data) {
		return data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE;
	}

	// what can't be decoded (an ASCII byte above 0x7E, invalid UTF-8) is first this, then '.'
	constexpr wchar_t Undecodable = 0xFFFD;

	// shown as is: printable, tab and line breaks
	bool IsShown(wchar_t c) {
		if (c == L'\t' || c == L'\r' || c == L'\n')
			return true;
		return !(c < 0x20 || c == 0x7F || (c >= 0x80 && c < 0xA0) || c == Undecodable);
	}

	// CR, LF and CR LF all as CR LF; what isn't shown as '.', counted
	std::wstring Finish(std::wstring_view text, size_t& replaced) {
		std::wstring result;
		result.reserve(text.size() + text.size() / 16);
		for (size_t i = 0; i < text.size(); i++) {
			auto c = text[i];
			if (c == L'\r') {
				result += L"\r\n";
				if (i + 1 < text.size() && text[i + 1] == L'\n')
					i++;
			}
			else if (c == L'\n') {
				result += L"\r\n";
			}
			else if (IsShown(c)) {
				result += c;
			}
			else {
				result += L'.';
				replaced++;
			}
		}
		return result;
	}

	std::wstring FromUtf8(std::span<const uint8_t> data, DWORD flags = 0) {
		if (data.empty())
			return {};
		auto len = ::MultiByteToWideChar(CP_UTF8, flags, (PCSTR)data.data(), (int)data.size(), nullptr, 0);
		if (len <= 0)
			return {};
		std::wstring text(len, 0);
		::MultiByteToWideChar(CP_UTF8, flags, (PCSTR)data.data(), (int)data.size(), text.data(), len);
		return text;
	}
}

Encoding TextDecode::Detect(std::span<const uint8_t> data) {
	if (HasUtf16Bom(data))
		return Encoding::Utf16;
	if (HasUtf8Bom(data))
		return Encoding::Utf8;

	auto sample = data.first((std::min)(data.size(), (size_t)4096));
	if (sample.size() >= 2) {
		size_t pairs = sample.size() / 2, oddZeros = 0, evenZeros = 0;
		for (size_t i = 0; i + 1 < sample.size(); i += 2) {
			evenZeros += sample[i] == 0;
			oddZeros += sample[i + 1] == 0;
		}
		if (oddZeros * 10 >= pairs * 3 && evenZeros * 10 < pairs)
			return Encoding::Utf16;
	}

	bool ascii = std::ranges::all_of(sample, [](uint8_t b) { return b < 0x80; });
	if (!ascii) {
		// valid UTF-8, allowing a sequence cut at the sample's end (when the data goes on)
		auto check = sample;
		if (sample.size() < data.size()) {
			for (int i = 0; i < 3 && !check.empty() && (check.back() & 0xC0) == 0x80; i++)
				check = check.first(check.size() - 1);
			if (!check.empty() && (check.back() & 0xC0) == 0xC0)
				check = check.first(check.size() - 1);
		}
		if (!check.empty() && ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (PCSTR)check.data(), (int)check.size(), nullptr, 0) > 0)
			return Encoding::Utf8;
	}
	return Encoding::Ascii;
}

std::wstring TextDecode::Decode(std::span<const uint8_t> data, Encoding encoding, size_t* replaced) {
	if (encoding == Encoding::Auto)
		encoding = Detect(data);

	size_t count = 0;
	std::wstring result;
	switch (encoding) {
		case Encoding::Utf16:
		{
			if (HasUtf16Bom(data))
				data = data.subspan(2);
			std::wstring_view text((const wchar_t*)data.data(), data.size() / 2);
			result = Finish(text, count);
			// an odd last byte isn't shown
			count += data.size() % 2;
			break;
		}
		case Encoding::Utf8:
			if (HasUtf8Bom(data))
				data = data.subspan(3);
			result = Finish(FromUtf8(data), count);
			break;
		default:
		{
			std::wstring text;
			text.reserve(data.size());
			for (auto b : data)
				text += b < 0x80 ? (wchar_t)b : Undecodable;
			result = Finish(text, count);
			break;
		}
	}
	if (replaced)
		*replaced = count;
	return result;
}

std::vector<uint8_t> TextDecode::Encode(std::wstring_view text, Encoding encoding, bool bom) {
	std::vector<uint8_t> data;
	switch (encoding) {
		case Encoding::Utf16:
		{
			if (bom)
				data = { 0xFF, 0xFE };
			auto p = reinterpret_cast<const uint8_t*>(text.data());
			data.insert(data.end(), p, p + text.size() * sizeof(wchar_t));
			break;
		}
		case Encoding::Ascii:
			data.reserve(text.size());
			for (auto c : text)
				data.push_back(c < 0x80 ? (uint8_t)c : (uint8_t)'?');
			break;
		default:
			if (bom)
				data = { 0xEF, 0xBB, 0xBF };
			if (!text.empty()) {
				auto len = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
				auto start = data.size();
				data.resize(start + len);
				::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), (PSTR)data.data() + start, len, nullptr, nullptr);
			}
			break;
	}
	return data;
}

bool TextDecode::HasBom(std::span<const uint8_t> data) {
	return HasUtf8Bom(data) || HasUtf16Bom(data);
}

PCWSTR TextDecode::Name(Encoding encoding) {
	switch (encoding) {
		case Encoding::Ascii: return L"ASCII";
		case Encoding::Utf8: return L"UTF-8";
		case Encoding::Utf16: return L"UTF-16";
	}
	return L"Auto";
}

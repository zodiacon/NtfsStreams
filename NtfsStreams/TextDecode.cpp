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

	// shown as is: printable, tab and line breaks
	wchar_t Shown(wchar_t c) {
		if (c == L'\t' || c == L'\r' || c == L'\n')
			return c;
		if (c < 0x20 || c == 0x7F || (c >= 0x80 && c < 0xA0))
			return L'.';
		return c;
	}

	// CR, LF and CR LF all as CR LF; the rest through Shown
	std::wstring Finish(std::wstring_view text) {
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
			else {
				result += Shown(c);
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

std::wstring TextDecode::Decode(std::span<const uint8_t> data, Encoding encoding) {
	if (encoding == Encoding::Auto)
		encoding = Detect(data);

	switch (encoding) {
		case Encoding::Utf16:
		{
			if (HasUtf16Bom(data))
				data = data.subspan(2);
			std::wstring_view text((const wchar_t*)data.data(), data.size() / 2);
			return Finish(text);
		}
		case Encoding::Utf8:
			if (HasUtf8Bom(data))
				data = data.subspan(3);
			return Finish(FromUtf8(data));
		default:
		{
			std::wstring text;
			text.reserve(data.size());
			for (auto b : data)
				text += b < 0x80 ? (wchar_t)b : L'.';
			return Finish(text);
		}
	}
}

PCWSTR TextDecode::Name(Encoding encoding) {
	switch (encoding) {
		case Encoding::Ascii: return L"ASCII";
		case Encoding::Utf8: return L"UTF-8";
		case Encoding::Utf16: return L"UTF-16";
	}
	return L"Auto";
}

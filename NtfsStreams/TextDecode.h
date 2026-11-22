#pragma once

#include <span>

//
// A stream's bytes as text, for the text pane: in the chosen encoding, or one guessed from them (Auto). Characters that
// don't show (controls, other than tab and line breaks) become '.', and line breaks become CR LF for the edit control.
//
namespace TextDecode {
	enum class Encoding { Auto, Ascii, Utf8, Utf16 };

	// a byte order mark first; then UTF-16 if most of the bytes at odd positions are zero (ASCII text in UTF-16LE),
	// UTF-8 if the bytes are valid UTF-8 with some that aren't ASCII, else ASCII
	Encoding Detect(std::span<const uint8_t> data);

	// Auto: as Detect says. A byte order mark isn't shown; in ASCII, bytes above 0x7E are '.'
	std::wstring Decode(std::span<const uint8_t> data, Encoding encoding);

	PCWSTR Name(Encoding encoding);
}

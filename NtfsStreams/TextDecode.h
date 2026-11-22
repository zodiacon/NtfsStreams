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

	// Auto: as Detect says. A byte order mark isn't shown; in ASCII, bytes above 0x7E are '.'. replaced: how many
	// characters became '.' (none: the text can be edited and written back as it was, but for its line breaks)
	std::wstring Decode(std::span<const uint8_t> data, Encoding encoding, size_t* replaced = nullptr);

	// text as a stream's bytes, for writing one: Auto is UTF-8; UTF-16 is little endian; in ASCII, characters above 0x7E
	// are '?'. bom: a byte order mark first (UTF-8 and UTF-16)
	std::vector<uint8_t> Encode(std::wstring_view text, Encoding encoding, bool bom);
	// whether the bytes start with a byte order mark (UTF-8 or UTF-16), to keep it when they're written again
	bool HasBom(std::span<const uint8_t> data);

	PCWSTR Name(Encoding encoding);
}

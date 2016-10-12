// AltStream.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


int wmain(int argc, const wchar_t* const argv[]) {
	if (argc < 4) {
		printf("Usage: AltStream <file> <stream name> \"text to write\"\n");
		return 1;
	}

	std::wstring file(argv[1]);
	(file += L":") += argv[2];

	HANDLE hFile = ::CreateFile(file.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("Error: %d\n", ::GetLastError());
		return 1;
	}

	DWORD dummy;
	::WriteFile(hFile, argv[3], (1 + ::wcslen(argv[3])) * sizeof(wchar_t), &dummy, nullptr);
	::SetEndOfFile(hFile);
	::CloseHandle(hFile);

	printf("Done.\n");

	return 0;
}


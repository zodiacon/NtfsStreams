#pragma once

#include <Settings.h>

// persisted under HKCU\Software\ScorpioSoftware\NtfsStreams; the first instance is the singleton (AppSettings::Get())
struct AppSettings : Settings {
	BEGIN_SETTINGS(AppSettings)
		SETTING(DarkMode, 0, SettingType::Bool);
		SETTING(ToolBar, 1, SettingType::Bool);		// View > Toolbar
		SETTING(StatusBar, 1, SettingType::Bool);	// View > Status Bar
		SETTING(FolderRecursive, 0, SettingType::Bool);	// a folder tab's Include Subfolders, for the next one opened
		// a file tab's: the hex view's bytes per line (8, 16, 32) and item size (1, 2, 4, 8), the text pane's encoding
		// (TextDecode::Encoding), the stream list's share of the width and the hex view's of the height, in percent
		SETTING(HexBytesPerLine, 16, SettingType::Int32);
		SETTING(HexDataSize, 1, SettingType::Int32);
		SETTING(TextEncoding, 0, SettingType::Int32);
		SETTING(StreamListPos, 25, SettingType::Int32);
		SETTING(HexPanePos, 65, SettingType::Int32);
		m_settings.insert({ L"RecentFiles", Setting(L"RecentFiles", std::vector<std::wstring>{}) });
		m_settings.insert({ L"RecentFolders", Setting(L"RecentFolders", std::vector<std::wstring>{}) });
	END_SETTINGS

	DEF_SETTING(DarkMode, int)
	DEF_SETTING(ToolBar, int)
	DEF_SETTING(StatusBar, int)
	DEF_SETTING(FolderRecursive, int)
	DEF_SETTING(HexBytesPerLine, int)
	DEF_SETTING(HexDataSize, int)
	DEF_SETTING(TextEncoding, int)
	DEF_SETTING(StreamListPos, int)
	DEF_SETTING(HexPanePos, int)
	DEF_SETTING_MULTI(RecentFiles)
	DEF_SETTING_MULTI(RecentFolders)

	static constexpr PCWSTR MainWindowPlacement = L"MainWindowPlacement";
	static constexpr PCWSTR RegistryPath = L"Software\\ScorpioSoftware\\NtfsStreams";
};

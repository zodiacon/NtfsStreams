#pragma once

#include <Settings.h>

// persisted under HKCU\Software\ScorpioSoftware\NtfsStreams; the first instance is the singleton (AppSettings::Get())
struct AppSettings : Settings {
	BEGIN_SETTINGS(AppSettings)
		SETTING(DarkMode, 0, SettingType::Bool);
		SETTING(ToolBar, 1, SettingType::Bool);		// View > Toolbar
		SETTING(StatusBar, 1, SettingType::Bool);	// View > Status Bar
		SETTING(FolderRecursive, 0, SettingType::Bool);	// a folder tab's Include Subfolders, for the next one opened
		m_settings.insert({ L"RecentFiles", Setting(L"RecentFiles", std::vector<std::wstring>{}) });
		m_settings.insert({ L"RecentFolders", Setting(L"RecentFolders", std::vector<std::wstring>{}) });
	END_SETTINGS

	DEF_SETTING(DarkMode, int)
	DEF_SETTING(ToolBar, int)
	DEF_SETTING(StatusBar, int)
	DEF_SETTING(FolderRecursive, int)
	DEF_SETTING_MULTI(RecentFiles)
	DEF_SETTING_MULTI(RecentFolders)

	static constexpr PCWSTR MainWindowPlacement = L"MainWindowPlacement";
	static constexpr PCWSTR RegistryPath = L"Software\\ScorpioSoftware\\NtfsStreams";
};

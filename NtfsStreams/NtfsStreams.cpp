// NtfsStreams.cpp : main source file for NtfsStreams.exe
//

#include "pch.h"
#include "MainFrm.h"
#include "AppSettings.h"
#include "Helpers.h"
#include <WTLHelper.h>

CAppModule _Module;
AppSettings _Settings;

namespace {
	// NtfsStreams.exe [path...]: each a file or a folder to open (full paths: another instance has another current folder)
	std::vector<std::wstring> GetCommandLinePaths() {
		std::vector<std::wstring> paths;
		int argc = 0;
		if (auto argv = ::CommandLineToArgvW(::GetCommandLine(), &argc); argv) {
			for (int i = 1; i < argc; i++)
				paths.push_back(Helpers::NormalizePath(argv[i]));
			::LocalFree(argv);
		}
		return paths;
	}

	// paths go to a running instance (Explorer's menu starts one each time): true if one took them
	bool SendToRunningInstance(std::vector<std::wstring> const& paths) {
		auto hWnd = ::FindWindow(CMainFrame::WindowClassName, nullptr);
		if (!hWnd)
			return false;
		// it may come to the front: this process may let it, as the user just started this one
		DWORD pid = 0;
		::GetWindowThreadProcessId(hWnd, &pid);
		::AllowSetForegroundWindow(pid);
		auto data = Helpers::JoinPaths(paths);
		COPYDATASTRUCT cds{ CMainFrame::CopyDataPaths, DWORD(data.size() * sizeof(WCHAR)), data.data() };
		DWORD_PTR result = 0;
		// a hung instance (or one that doesn't know the message): this one opens them
		return ::SendMessageTimeout(hWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds), SMTO_ABORTIFHUNG, 5000, &result) && result;
	}
}

static int Run(int nCmdShow, std::vector<std::wstring> const& paths) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;
	if (wndMain.CreateEx() == nullptr) {
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	// the last position, but never minimized: as it was before it was minimized
	auto& settings = AppSettings::Get();
	if (auto wp = settings.GetBinary<WINDOWPLACEMENT>(AppSettings::MainWindowPlacement); wp && (wp->showCmd == SW_SHOWMINIMIZED || wp->showCmd == SW_MINIMIZE)) {
		auto placement = *wp;
		placement.showCmd = (placement.flags & WPF_RESTORETOMAXIMIZED) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
		settings.Set(AppSettings::MainWindowPlacement, placement);
	}
	if (!settings.LoadWindowPosition(wndMain, AppSettings::MainWindowPlacement))
		wndMain.ShowWindow(nCmdShow);

	wndMain.OpenPaths(paths);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpstrCmdLine*/, int nCmdShow) {
	// paths with a window open already: that one shows them (without paths, a new window)
	auto paths = GetCommandLinePaths();
	if (!paths.empty() && SendToRunningInstance(paths))
		return 0;

	HRESULT hRes = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_BAR_CLASSES | ICC_LINK_CLASS);	// SysLink: the About box

	hRes = _Module.Init(nullptr, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	if (AppSettings::Get().Load(AppSettings::RegistryPath))
		WTLHelper::InitDarkMode(AppSettings::Get().DarkMode() ? DarkModeKind::Dark : DarkModeKind::Light);
	else
		WTLHelper::InitDarkMode();

	int nRet = Run(nCmdShow, paths);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

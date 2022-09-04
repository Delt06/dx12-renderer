#define WIN32_LEAN_AND_MEAN
#include <codecvt>
#include <Windows.h>
#include <Shlwapi.h>
#include <shellapi.h>

#include "Application.h"
#include "Tutorial2/Tutorial2.h"

#include <dxgidebug.h>

void ReportLiveObjects()
{
	IDXGIDebug1* dxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	dxgiDebug->Release();
}

struct Parameters
{
	int ClientWidth = 1280;
	int ClientHeight = 720;
	bool UseWarp = false;
	std::string DemoName;
};

void ParseCommandLineArguments(Parameters& parameters)
{
	int argc;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	for (size_t i = 0; i < argc; ++i)
	{
		if (wcscmp(argv[i], L"-w") == 0 || wcscmp(argv[i], L"--width") == 0)
		{
			parameters.ClientWidth = wcstol(argv[++i], nullptr, 10);
		}

		if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--height") == 0)
		{
			parameters.ClientHeight = wcstol(argv[++i], nullptr, 10);
		}

		if (wcscmp(argv[i], L"-warp") == 0 || wcscmp(argv[i], L"--warp") == 0)
		{
			parameters.UseWarp = true;
		}

		if (wcscmp(argv[i], L"-d") == 0 || wcscmp(argv[i], L"--demo") == 0)
		{
			parameters.DemoName = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(argv[++i]);
		}
	}

	LocalFree(argv);
}

std::shared_ptr<Game> CreateGame(const Parameters& parameters)
{
	if (parameters.DemoName == "Tutorial2")
		return std::make_shared<Tutorial2>(L"Learning DirectX 12 - Lesson 2", parameters.ClientWidth,
		                                   parameters.ClientHeight);

	const std::string message = std::string(parameters.DemoName) + " is an unknown demo name";
	throw std::exception(message.c_str());
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	int retCode = 0;

	// Set the working directory to the path of the executable.
	WCHAR path[MAX_PATH];
	const HMODULE hModule = GetModuleHandleW(nullptr);
	if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
	{
		PathRemoveFileSpecW(path);
		SetCurrentDirectoryW(path);
	}

	Parameters parameters;
	ParseCommandLineArguments(parameters);

	Application::Create(hInstance, parameters.UseWarp);
	{
		const auto demo = std::make_shared<Tutorial2>(L"Learning DirectX 12 - Lesson 2", parameters.ClientWidth,
		                                              parameters.ClientHeight);
		retCode = Application::Get().Run(demo);
	}
	Application::Destroy();

	atexit(&ReportLiveObjects);

	return retCode;
}

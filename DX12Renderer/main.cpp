#define WIN32_LEAN_AND_MEAN
#include <codecvt>
#include <Windows.h>
#include <Shlwapi.h>
#include <shellapi.h>

#include "Application.h"

#include <dxgidebug.h>

#include "GraphicsSettings.h"
#include "Tutorial3/Tutorial3.h"
#include "LightingDemo/LightingDemo.h"

void ReportLiveObjects()
{
	IDXGIDebug1* dxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	dxgiDebug->Release();
}

struct Parameters
{
	int m_ClientWidth = 1280;
	int m_ClientHeight = 720;
	std::string m_DemoName;

	GraphicsSettings m_GraphicsSettings;
};

void ParseCommandLineArguments(Parameters& parameters)
{
	int argc;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	for (size_t i = 0; i < argc; ++i)
	{
		if (wcscmp(argv[i], L"-w") == 0 || wcscmp(argv[i], L"--width") == 0)
		{
			parameters.m_ClientWidth = wcstol(argv[++i], nullptr, 10);
		}

		if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--height") == 0)
		{
			parameters.m_ClientHeight = wcstol(argv[++i], nullptr, 10);
		}

		if (wcscmp(argv[i], L"-d") == 0 || wcscmp(argv[i], L"--demo") == 0)
		{
			std::wstring arg = argv[++i];
			parameters.m_DemoName = std::string(arg.begin(), arg.end());
		}
		else
		{
			parameters.m_DemoName = "LightingDemo";
		}

		if (wcscmp(argv[i], L"--shadowResolution") == 0)
		{
			parameters.m_GraphicsSettings.m_ShadowsResolution = wcstol(argv[++i], nullptr, 10);
		}

		if (wcscmp(argv[i], L"--poissonSpread") == 0)
		{
			parameters.m_GraphicsSettings.m_PoissonSpread = wcstof(argv[++i], nullptr);
		}
	}

	LocalFree(argv);
}

std::shared_ptr<Game> CreateGame(const Parameters& parameters)
{
	if (parameters.m_DemoName == "Tutorial3")
		return std::make_shared<Tutorial3>(L"Learning DirectX 12 - Lesson 3", parameters.m_ClientWidth,
		                                   parameters.m_ClientHeight);
	if (parameters.m_DemoName == "LightingDemo")
		return std::make_shared<LightingDemo>(L"Lighting Demo", parameters.m_ClientWidth, parameters.m_ClientHeight, parameters.m_GraphicsSettings);

	const std::string message = "'" + std::string(parameters.m_DemoName) + "' is an unknown demo name";
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

	Application::Create(hInstance);
	{
		const auto demo = CreateGame(parameters);
		retCode = Application::Get().Run(demo);
	}
	Application::Destroy();

	atexit(&ReportLiveObjects);

	return retCode;
}

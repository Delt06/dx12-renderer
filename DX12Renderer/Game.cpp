#include "Game.h"

#include <DirectXMath.h>

#include "Application.h"
#include "Window.h"

bool Game::Initialize()
{
	if (!DirectX::XMVerifyCPUSupport())
	{
		MessageBoxA(nullptr, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	PWindow = Application::Get().CreateRenderWindow(Name, Width, Height, VSync);
	PWindow->RegisterCallbacks(shared_from_this());
	PWindow->Show();

	return true;
}

void Game::Destroy()
{
	Application::Get().DestroyWindow(PWindow);
	PWindow.reset();
}

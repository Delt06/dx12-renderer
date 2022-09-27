#include "Game.h"

#include <DirectXMath.h>

#include "Application.h"
#include "Window.h"

Game::Game(const std::wstring& name, const int width, const int height, const bool vSync)
	: Name(name)
	, Width(width)
	, Height(height)
	, VSync(vSync)
{
}

Game::~Game()
{
	assert(!PWindow && "Use Game::Destroy() before destruction.");
}

bool Game::Initialize()
{
	// Check for DirectX Math library support.
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

int Game::GetClientWidth() const
{
	return Width;
}

int Game::GetClientHeight() const
{
	return Height;
}


void Game::OnUpdate(UpdateEventArgs& e)
{
}

void Game::OnRender(RenderEventArgs& e)
{
}

void Game::OnKeyPressed(KeyEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnKeyReleased(KeyEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseMoved(class MouseMotionEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseWheel(MouseWheelEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnResize(ResizeEventArgs& e)
{
	Width = e.Width;
	Height = e.Height;
}

void Game::OnWindowDestroy()
{
	// If the Window which we are registered to is 
	// destroyed, then any resources which are associated 
	// to the window must be released.
	UnloadContent();
}

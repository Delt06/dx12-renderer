#include "DX12LibPCH.h"

#include "Window.h"

#include "Application.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "Game.h"
#include "RenderTarget.h"
#include "ResourceStateTracker.h"
#include "Texture.h"

Window::Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync)
	: HWnd(hWnd)
	  , WindowName(windowName)
	  , ClientWidth(clientWidth)
	  , ClientHeight(clientHeight)
	  , VSync(vSync)
	  , Fullscreen(false)
	  , FenceValues{0}
	  , FrameValues{0}
{
	Application& app = Application::Get();

	IsTearingSupported = app.IsTearingSupported();

	for (int i = 0; i < BUFFER_COUNT; ++i)
	{
		BackBufferTextures[i].SetName(L"Backbuffer[" + std::to_wstring(i) + L"]");
	}

	DxgiSwapChain = CreateSwapChain();
	UpdateRenderTargetViews();
}

Window::~Window()
{
	// Window should be destroyed with Application::DestroyWindow before
	// the window goes out of scope.
	assert(!HWnd && "Use Application::DestroyWindow before destruction.");
}

void Window::Initialize()
{
}


HWND Window::GetWindowHandle() const
{
	return HWnd;
}

const std::wstring& Window::GetWindowName() const
{
	return WindowName;
}

void Window::Show()
{
	ShowWindow(HWnd, SW_SHOW);
}

/**
* Hide the window.
*/
void Window::Hide()
{
	ShowWindow(HWnd, SW_HIDE);
}

void Window::Destroy()
{
	if (auto pGame = PGame.lock())
	{
		// Notify the registered game that the window is being destroyed.
		pGame->OnWindowDestroy();
	}

	if (HWnd)
	{
		DestroyWindow(HWnd);
		HWnd = nullptr;
	}
}

int Window::GetClientWidth() const
{
	return ClientWidth;
}

int Window::GetClientHeight() const
{
	return ClientHeight;
}

bool Window::IsVSync() const
{
	return VSync;
}

void Window::SetVSync(bool vSync)
{
	VSync = vSync;
}

void Window::ToggleVSync()
{
	SetVSync(!VSync);
}

bool Window::IsFullScreen() const
{
	return Fullscreen;
}

// Set the fullscreen state of the window.
void Window::SetFullscreen(bool fullscreen)
{
	if (Fullscreen != fullscreen)
	{
		Fullscreen = fullscreen;

		if (Fullscreen) // Switching to fullscreen.
		{
			// Store the current window dimensions so they can be restored 
			// when switching out of fullscreen state.
			GetWindowRect(HWnd, &WindowRect);

			// Set the window style to a borderless window so the client area fills
			// the entire screen.
			const UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX |
				WS_MAXIMIZEBOX);

			SetWindowLongW(HWnd, GWL_STYLE, windowStyle);

			// Query the name of the nearest display device for the window.
			// This is required to set the fullscreen dimensions of the window
			// when using a multi-monitor setup.
			const HMONITOR hMonitor = MonitorFromWindow(HWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitorInfo);

			SetWindowPos(HWnd, HWND_TOP,
			             monitorInfo.rcMonitor.left,
			             monitorInfo.rcMonitor.top,
			             monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			             monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			             SWP_FRAMECHANGED | SWP_NOACTIVATE);

			ShowWindow(HWnd, SW_MAXIMIZE);
		}
		else
		{
			// Restore all the window decorators.
			::SetWindowLong(HWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			SetWindowPos(HWnd, HWND_NOTOPMOST,
			             WindowRect.left,
			             WindowRect.top,
			             WindowRect.right - WindowRect.left,
			             WindowRect.bottom - WindowRect.top,
			             SWP_FRAMECHANGED | SWP_NOACTIVATE);

			ShowWindow(HWnd, SW_NORMAL);
		}
	}
}

void Window::ToggleFullscreen()
{
	SetFullscreen(!Fullscreen);
}


void Window::RegisterCallbacks(std::shared_ptr<Game> pGame)
{
	PGame = pGame;
}

void Window::OnUpdate(UpdateEventArgs& e)
{
	UpdateClock.Tick();

	if (auto pGame = PGame.lock())
	{
		UpdateEventArgs updateEventArgs(UpdateClock.GetDeltaSeconds(), UpdateClock.GetTotalSeconds(),
		                                e.FrameNumber);
		pGame->OnUpdate(updateEventArgs);
	}
}

void Window::OnRender(RenderEventArgs& e)
{
	RenderClock.Tick();

	if (auto pGame = PGame.lock())
	{
		RenderEventArgs renderEventArgs(RenderClock.GetDeltaSeconds(), RenderClock.GetTotalSeconds(),
		                                e.FrameNumber);
		pGame->OnRender(renderEventArgs);
	}
}

void Window::OnKeyPressed(KeyEventArgs& e)
{
	if (auto pGame = PGame.lock())
	{
		pGame->OnKeyPressed(e);
	}
}

void Window::OnKeyReleased(KeyEventArgs& e)
{
	if (auto pGame = PGame.lock())
	{
		pGame->OnKeyReleased(e);
	}
}

// The mouse was moved
void Window::OnMouseMoved(MouseMotionEventArgs& e)
{
	e.RelX = e.X - PreviousMouseX;
	e.RelY = e.Y - PreviousMouseY;

	PreviousMouseX = e.X;
	PreviousMouseY = e.Y;

	if (auto pGame = PGame.lock())
	{
		pGame->OnMouseMoved(e);
	}
}

// A button on the mouse was pressed
void Window::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	PreviousMouseX = e.X;
	PreviousMouseY = e.Y;

	if (auto pGame = PGame.lock())
	{
		pGame->OnMouseButtonPressed(e);
	}
}

// A button on the mouse was released
void Window::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	if (auto pGame = PGame.lock())
	{
		pGame->OnMouseButtonReleased(e);
	}
}

// The mouse wheel was moved.
void Window::OnMouseWheel(MouseWheelEventArgs& e)
{
	if (auto pGame = PGame.lock())
	{
		pGame->OnMouseWheel(e);
	}
}

void Window::OnResize(ResizeEventArgs& e)
{
	// Update the client size.
	if (ClientWidth != e.Width || ClientHeight != e.Height)
	{
		ClientWidth = std::max(1, e.Width);
		ClientHeight = std::max(1, e.Height);

		Application::Get().Flush();

		// Release all references to back buffer textures.
		MRenderTarget.AttachTexture(Color0, Texture());
		for (int i = 0; i < BUFFER_COUNT; ++i)
		{
			ResourceStateTracker::RemoveGlobalResourceState(BackBufferTextures[i].GetD3D12Resource().Get());
			BackBufferTextures[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(DxgiSwapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(DxgiSwapChain->ResizeBuffers(BUFFER_COUNT, ClientWidth,
		                                           ClientHeight, swapChainDesc.BufferDesc.Format,
		                                           swapChainDesc.Flags));

		CurrentBackBufferIndex = DxgiSwapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews();
	}

	if (auto pGame = PGame.lock())
	{
		pGame->OnResize(e);
	}
}

ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
	Application& app = Application::Get();

	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = ClientWidth;
	swapChainDesc.Height = ClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = {1, 0};
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = BUFFER_COUNT;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	ID3D12CommandQueue* pCommandQueue = app.GetCommandQueue()->GetD3D12CommandQueue().Get();

	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
		pCommandQueue,
		HWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1));

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(HWnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

	CurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

	return dxgiSwapChain4;
}

void Window::UpdateRenderTargetViews()
{
	for (int i = 0; i < BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(DxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		ResourceStateTracker::AddGlobalResourceState(backBuffer.Get(), D3D12_RESOURCE_STATE_COMMON);

		BackBufferTextures[i].SetD3D12Resource(backBuffer);
		BackBufferTextures[i].CreateViews();
	}
}

const RenderTarget& Window::GetRenderTarget() const
{
	MRenderTarget.AttachTexture(Color0, BackBufferTextures[CurrentBackBufferIndex]);
	return MRenderTarget;
}

UINT Window::Present(const Texture& texture)
{
	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();

	auto& backBuffer = BackBufferTextures[CurrentBackBufferIndex];

	if (texture.IsValid())
	{
		if (texture.GetD3D12ResourceDesc().SampleDesc.Count > 1)
		{
			commandList->ResolveSubresource(backBuffer, texture);
		}
		else
		{
			commandList->CopyResource(backBuffer, texture);
		}
	}

	RenderTarget renderTarget;
	renderTarget.AttachTexture(Color0, backBuffer);

	commandList->TransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
	commandQueue->ExecuteCommandList(commandList);

	UINT syncInterval = VSync ? 1 : 0;
	UINT presentFlags = IsTearingSupported && !VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(DxgiSwapChain->Present(syncInterval, presentFlags));

	FenceValues[CurrentBackBufferIndex] = commandQueue->Signal();
	FrameValues[CurrentBackBufferIndex] = Application::GetFrameCount();

	CurrentBackBufferIndex = DxgiSwapChain->GetCurrentBackBufferIndex();

	commandQueue->WaitForFenceValue(FenceValues[CurrentBackBufferIndex]);

	Application::Get().ReleaseStaleDescriptors(FrameValues[CurrentBackBufferIndex]);

	return CurrentBackBufferIndex;
}

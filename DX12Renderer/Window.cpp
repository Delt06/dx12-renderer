#include "Window.h"

#include "DX12LibPCH.h"
#include "Application.h"
#include "CommandQueue.h"
#include <d3d12.h>
#include <d3dx12.h>
#include "Window.h"
#include "Game.h"

Window::Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync)
	: HWnd(hWnd)
	  , WindowName(windowName)
	  , ClientWidth(clientWidth)
	  , ClientHeight(clientHeight)
	  , VSync(vSync)
	  , Fullscreen(false)
	  , FrameCounter(0)
{
	Application& app = Application::Get();

	IsTearingSupported = app.IsTearingSupported();

	DxgiSwapChain = CreateSwapChain();
	D3d12RtvDescriptorHeap = app.CreateDescriptorHeap(BUFFER_COUNT, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	RtvDescriptorSize = app.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews();
}

Window::~Window()
{
	// Window should be destroyed with Application::DestroyWindow before
	// the window goes out of scope.
	assert(!HWnd && "Use Application::DestroyWindow before destruction.");
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
			UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX |
				WS_MAXIMIZEBOX);

			SetWindowLongW(HWnd, GWL_STYLE, windowStyle);

			// Query the name of the nearest display device for the window.
			// This is required to set the fullscreen dimensions of the window
			// when using a multi-monitor setup.
			HMONITOR hMonitor = MonitorFromWindow(HWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitorInfo);

			SetWindowPos(HWnd, HWND_TOPMOST,
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

void Window::OnUpdate(UpdateEventArgs&)
{
	UpdateClock.Tick();

	if (auto pGame = PGame.lock())
	{
		FrameCounter++;

		UpdateEventArgs updateEventArgs(UpdateClock.GetDeltaSeconds(), UpdateClock.GetTotalSeconds());
		pGame->OnUpdate(updateEventArgs);
	}
}

void Window::OnRender(RenderEventArgs&)
{
	RenderClock.Tick();

	if (auto pGame = PGame.lock())
	{
		RenderEventArgs renderEventArgs(RenderClock.GetDeltaSeconds(), RenderClock.GetTotalSeconds());
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
	if (auto pGame = PGame.lock())
	{
		pGame->OnMouseMoved(e);
	}
}

// A button on the mouse was pressed
void Window::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
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

		for (int i = 0; i < BUFFER_COUNT; ++i)
		{
			D3d12BackBuffers[i].Reset();
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

// Update the render target views for the swapchain back buffers.
void Window::UpdateRenderTargetViews()
{
	auto device = Application::Get().GetDevice();

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(D3d12RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(DxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		D3d12BackBuffers[i] = backBuffer;

		rtvHandle.Offset(RtvDescriptorSize);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(D3d12RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
	                                     CurrentBackBufferIndex, RtvDescriptorSize);
}

ComPtr<ID3D12Resource> Window::GetCurrentBackBuffer() const
{
	return D3d12BackBuffers[CurrentBackBufferIndex];
}

UINT Window::GetCurrentBackBufferIndex() const
{
	return CurrentBackBufferIndex;
}

UINT Window::Present()
{
	UINT syncInterval = VSync ? 1 : 0;
	UINT presentFlags = IsTearingSupported && !VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(DxgiSwapChain->Present(syncInterval, presentFlags));
	CurrentBackBufferIndex = DxgiSwapChain->GetCurrentBackBufferIndex();

	return CurrentBackBufferIndex;
}

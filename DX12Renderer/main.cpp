#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // For CommandLineToArgvW

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include <d3dx12.h>

// STL Headers
#include <algorithm>
#include <cassert>
#include <chrono>

// Helper functions
#include "Helpers.h"

// the number of swap chain back buffers
constexpr uint8_t NUM_FRAMES = 3;

struct
{
	// use WARP adapter
	bool UseWarp = false;

	uint32_t ClientWidth = 1280;
	uint32_t ClientHeight = 720;

	// set to true once the DX12 objects have been initialized
	bool IsInitialized = false;

	HWND HWindow = nullptr;
	// used to store the previous window dimensions when going fullscreen
	RECT WindowRect = {};

	// DirectX 12 Objects
	ComPtr<ID3D12Device2> Device;
	ComPtr<ID3D12CommandQueue> CommandQueue;
	ComPtr<IDXGISwapChain4> SwapChain;
	ComPtr<ID3D12Resource> BackBuffers[NUM_FRAMES];
	ComPtr<ID3D12GraphicsCommandList> CommandList;
	ComPtr<ID3D12CommandAllocator> CommandAllocators[NUM_FRAMES];
	// RTV = render target view (descriptor of a texture residing in GPU memory)
	// RTVs are stored in a separate heap
	ComPtr<ID3D12DescriptorHeap> RtvDescriptorHeap;

	UINT RtvDescriptorSize = 0;
	UINT CurrentBackBufferIndex = 0;

	// Synchronization objects

	ComPtr<ID3D12Fence> Fence;
	// the next value to signal the command queue
	uint64_t FenceValue = 0;
	// values used to signal the command queue for each particular frame
	uint64_t FrameFenceValues[NUM_FRAMES] = {};
	// notification that the fence has reached a specific value
	HANDLE FenceEvent = nullptr;

	// toggled with V key
	bool VSync = true;
	bool TearingSupported = false;
	// use windowed mode by default; toggle via Alt+Enter or F11
	bool FullScreen = false;
} globals;

LRESULT CALLBACK WindowCallback(HWND, UINT, WPARAM, LPARAM);

void ParseCommandLineArguments()
{
	int argc;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	for (size_t i = 0; i < argc; ++i)
	{
		if (wcscmp(argv[i], L"-w") == 0 || wcscmp(argv[i], L"--width") == 0)
		{
			globals.ClientWidth = wcstol(argv[++i], nullptr, 10);
		}

		if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--height") == 0)
		{
			globals.ClientHeight = wcstol(argv[++i], nullptr, 10);
		}

		if (wcscmp(argv[i], L"-warp") == 0 || wcscmp(argv[i], L"--warp") == 0)
		{
			globals.ClientHeight = true;
		}
	}

	LocalFree(argv);
}

void EnabledDebugLayer()
{
#if defined(_DEBUG)
	// Always enabled the debug layer before doing anything DX12 related
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void RegisterWindowClass(const HINSTANCE hInstance, const wchar_t* windowClassName)
{
	// Register a window class for creating out render window with
	WNDCLASSEXW windowClass = {};

	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowCallback;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = ::LoadIcon(hInstance, nullptr);
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = nullptr;
	windowClass.lpszClassName = windowClassName;
	windowClass.hIconSm = ::LoadIcon(hInstance, nullptr);

	static ATOM atom = RegisterClassExW(&windowClass);
	assert(atom > 0);
}

HWND CreateWindow(const wchar_t* windowClassName, const HINSTANCE hInstance, const wchar_t* windowTitle,
                  const uint32_t width, const uint32_t height)
{
	const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	RECT windowRect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	const int windowWidth = windowRect.right - windowRect.left;
	const int windowHeight = windowRect.bottom - windowRect.top;

	// Center the window within the screen
	const int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
	const int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

	const HWND hWnd = CreateWindowExW(
		NULL,
		windowClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		windowX,
		windowY,
		windowWidth,
		windowHeight,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	assert(hWnd && "Failed to create window");
	return hWnd;
}

ComPtr<IDXGIAdapter4> GetAdapter(const bool useWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;

		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}
	return dxgiAdapter4;
}

ComPtr<ID3D12Device2> CreateDevice(const ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> device;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

#if defined(_DEBUG)

	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(device.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
	}

	// Suppress whole categories of messages
	//D3D12_MESSAGE_CATEGORY Categories[] = {};

	// Suppress messages based on their severity level
	D3D12_MESSAGE_SEVERITY severities[] =
	{
		D3D12_MESSAGE_SEVERITY_INFO
	};

	// Suppress individual messages by their ID
	D3D12_MESSAGE_ID denyIds[] = {
		// "I'm really not sure how to avoid this message."
		D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
		// This warning occurs when using capture frame while graphics debugging.
		D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
		// This warning occurs when using capture frame while graphics debugging.
		D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
	};

	D3D12_INFO_QUEUE_FILTER newFilter = {};
	//newFilter.DenyList.NumCategories = _countof(Categories);
	//newFilter.DenyList.pCategoryList = Categories;
	newFilter.DenyList.NumSeverities = _countof(severities);
	newFilter.DenyList.pSeverityList = severities;
	newFilter.DenyList.NumIDs = _countof(denyIds);
	newFilter.DenyList.pIDList = denyIds;

	ThrowIfFailed(infoQueue->PushStorageFilter(&newFilter));

#endif

	return device;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(const ComPtr<ID3D12Device2> device, const D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandQueue> commandQueue;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));

	return commandQueue;
}

bool CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(
				factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
	}

	return allowTearing == TRUE;
}

ComPtr<IDXGISwapChain4> CreateSwapChain(const HWND hWnd, const ComPtr<ID3D12CommandQueue> commandQueue,
                                        const uint32_t width,
                                        const uint32_t height, const uint32_t bufferCount)
{
	ComPtr<IDXGISwapChain4> swapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = {1, 0};
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
		commandQueue.Get(),
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1
	));

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&swapChain4));

	return swapChain4;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(const ComPtr<ID3D12Device2> device,
                                                  const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t numDescriptors)
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;

	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

void UpdateRenderTargetViews(const ComPtr<ID3D12Device2> device, const ComPtr<IDXGISwapChain4> swapChain,
                             const ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	const auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < NUM_FRAMES; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
		globals.BackBuffers[i] = backBuffer;
		rtvHandle.Offset(rtvDescriptorSize);
	}
}

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(const ComPtr<ID3D12Device2> device,
                                                      const D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(const ComPtr<ID3D12Device2> device,
                                                    const ComPtr<ID3D12CommandAllocator> commandAllocator,
                                                    const D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
	ThrowIfFailed(commandList->Close());
	return commandList;
}

ComPtr<ID3D12Fence> CreateFence(const ComPtr<ID3D12Device2> device)
{
	ComPtr<ID3D12Fence> fence;
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	return fence;
}

HANDLE CreateEventHandle()
{
	const HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent && "Failed to create fence event.");
	return fenceEvent;
}

uint64_t Signal(const ComPtr<ID3D12CommandQueue> commandQueue, const ComPtr<ID3D12Fence> fence, uint64_t& fenceValue)
{
	const uint64_t fenceValueForSignal = ++fenceValue;
	ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));
	return fenceValueForSignal;
}

void WaitForFenceValue(const ComPtr<ID3D12Fence> fence, const uint64_t fenceValue, const HANDLE fenceEvent,
                       const std::chrono::microseconds duration = std::chrono::microseconds::max())
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
		WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
}

void Flush(const ComPtr<ID3D12CommandQueue> commandQueue, const ComPtr<ID3D12Fence> fence, uint64_t& fenceValue,
           const HANDLE fenceEvent)
{
	const uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
	WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

void Update()
{
	static uint64_t frameCounter = 0;
	static double elapsedSeconds = 0.0;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	frameCounter++;
	const auto t1 = clock.now();
	const auto deltaTime = t1 - t0;
	t0 = t1;

	elapsedSeconds += deltaTime.count() * 1e-9;
	if (elapsedSeconds > 1.0)
	{
		wchar_t buffer[500];
		const auto fps = static_cast<double>(frameCounter) / elapsedSeconds;
		ThrowIfNegative(swprintf_s(buffer, 500, L"FPS: %f\n", fps));
		OutputDebugString(buffer);

		frameCounter = 0;
		elapsedSeconds = 0.0;
	}
}

void Render()
{
	const auto commandAllocator = globals.CommandAllocators[globals.CurrentBackBufferIndex];
	const auto backBuffer = globals.BackBuffers[globals.CurrentBackBufferIndex];

	commandAllocator->Reset();
	globals.CommandList->Reset(commandAllocator.Get(), nullptr);

	// Clear the render target
	{
		// the "before" state is known for sure
		// thus, it is hardcoded here
		const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		globals.CommandList->ResourceBarrier(1, &barrier);

		constexpr FLOAT clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};
		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(globals.RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		                                        globals.CurrentBackBufferIndex, globals.RtvDescriptorSize);

		// clear the entire resource view -> NumRects is 0
		globals.CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	}

	// Preset
	{
		const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		globals.CommandList->ResourceBarrier(1, &barrier);

		ThrowIfFailed(globals.CommandList->Close());

		ID3D12CommandList* const commandLists[] = {
			globals.CommandList.Get()
		};
		globals.CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		const UINT syncInterval = globals.VSync ? 1 : 0;
		const UINT presentFlags = globals.TearingSupported && !globals.VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(globals.SwapChain->Present(syncInterval, presentFlags));

		globals.FrameFenceValues[globals.CurrentBackBufferIndex] = Signal(
			globals.CommandQueue, globals.Fence, globals.FenceValue);
		globals.CurrentBackBufferIndex = globals.SwapChain->GetCurrentBackBufferIndex();

		WaitForFenceValue(globals.Fence, globals.FrameFenceValues[globals.CurrentBackBufferIndex], globals.FenceEvent);
	}
}

void Resize(const uint32_t width, const uint32_t height)
{
	if (globals.ClientWidth == width && globals.ClientHeight == height)
		return;

	// 0 is an invalid value
	globals.ClientWidth = std::max(1u, width);
	globals.ClientHeight = std::max(1u, height);

	Flush(globals.CommandQueue, globals.Fence, globals.FenceValue, globals.FenceEvent);

	for (int i = 0; i < NUM_FRAMES; ++i)
	{
		// Any references to the back buffers must be released
		// before the swap chain can be resized.
		globals.BackBuffers[i].Reset();
		globals.FrameFenceValues[i] = globals.FrameFenceValues[globals.CurrentBackBufferIndex];
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	ThrowIfFailed(globals.SwapChain->GetDesc(&swapChainDesc));
	ThrowIfFailed(globals.SwapChain->ResizeBuffers(NUM_FRAMES, globals.ClientWidth, globals.ClientHeight,
	                                               swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

	globals.CurrentBackBufferIndex = globals.SwapChain->GetCurrentBackBufferIndex();

	UpdateRenderTargetViews(globals.Device, globals.SwapChain, globals.RtvDescriptorHeap);
}

void SetFullScreen(const bool fullScreen)
{
	if (globals.FullScreen == fullScreen)
	{
		return;
	}

	globals.FullScreen = fullScreen;
	if (globals.FullScreen) // Switching to full screen...
	{
		// Store the current window dimensions so they can be restored 
		// when switching out of fullscreen state.
		GetWindowRect(globals.HWindow, &globals.WindowRect);
		// Set the window style to a borderless window so the client area fills
		// the entire screen.
		constexpr UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX |
			WS_MAXIMIZEBOX);
		SetWindowLongW(globals.HWindow, GWL_STYLE, windowStyle);

		// Query the name of the nearest display device for the window.
		// This is required to set the fullscreen dimensions of the window
		// when using a multi-monitor setup.
		const HMONITOR hMonitor = MonitorFromWindow(globals.HWindow, MONITOR_DEFAULTTONEAREST);
		MONITORINFOEX monitorInfo = {};
		monitorInfo.cbSize = sizeof(MONITORINFOEX);
		GetMonitorInfo(hMonitor, &monitorInfo);

		SetWindowPos(globals.HWindow, HWND_TOP,
		             monitorInfo.rcMonitor.left,
		             monitorInfo.rcMonitor.top,
		             monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
		             monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
		             SWP_FRAMECHANGED | SWP_NOACTIVATE);
		ShowWindow(globals.HWindow, SW_MAXIMIZE);
	}
	else
	{
		// Restore all the window decorators.
		SetWindowLong(globals.HWindow, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowPos(globals.HWindow, HWND_NOTOPMOST,
		             globals.WindowRect.left,
		             globals.WindowRect.top,
		             globals.WindowRect.right - globals.WindowRect.left,
		             globals.WindowRect.bottom - globals.WindowRect.top,
		             SWP_FRAMECHANGED | SWP_NOACTIVATE
		);
		ShowWindow(globals.HWindow, SW_NORMAL);
	}
}

LRESULT CALLBACK WindowCallback(HWND hWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (globals.IsInitialized)
	{
		switch (message)
		{
		case WM_PAINT:
			Update();
			Render();
			break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			{
				const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

				switch (wParam)
				{
				case 'V':
					globals.VSync = !globals.VSync;
					break;
				case VK_ESCAPE:
					PostQuitMessage(0);
					break;
				case VK_RETURN:
					if (alt)
					{
					case VK_F11:
						SetFullScreen(!globals.FullScreen);
					}
					break;
				default:
					break;
				}
			}
		case WM_SYSCHAR:
			break;
		case WM_SIZE:
			{
				RECT clientRect = {};
				GetClientRect(globals.HWindow, &clientRect);

				const int width = clientRect.right - clientRect.left;
				const int height = clientRect.bottom - clientRect.top;

				Resize(width, height);
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProcW(hWindow, message, wParam, lParam);
		}
	}
	else
	{
		return DefWindowProcW(hWindow, message, wParam, lParam);
	}

	return 0;
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Window class name. Used for registering / creating the window.
	const auto windowClassName = L"DX12WindowClass";
	ParseCommandLineArguments();

	EnabledDebugLayer();

	globals.TearingSupported = CheckTearingSupport();

	RegisterWindowClass(hInstance, windowClassName);
	globals.HWindow = CreateWindow(windowClassName, hInstance, L"Learning DirectX 12", globals.ClientWidth,
	                               globals.ClientHeight);

	// Initialize the global window rect variable
	GetWindowRect(globals.HWindow, &globals.WindowRect);

	const ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(globals.UseWarp);
	globals.Device = CreateDevice(dxgiAdapter4);
	globals.CommandQueue = CreateCommandQueue(globals.Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	globals.SwapChain = CreateSwapChain(globals.HWindow, globals.CommandQueue, globals.ClientWidth,
	                                    globals.ClientHeight, NUM_FRAMES);
	globals.CurrentBackBufferIndex = globals.SwapChain->GetCurrentBackBufferIndex();

	globals.RtvDescriptorHeap = CreateDescriptorHeap(globals.Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_FRAMES);
	globals.RtvDescriptorSize = globals.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews(globals.Device, globals.SwapChain, globals.RtvDescriptorHeap);

	for (int i = 0; i < NUM_FRAMES; ++i)
	{
		globals.CommandAllocators[i] = CreateCommandAllocator(globals.Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}

	globals.CommandList = CreateCommandList(globals.Device, globals.CommandAllocators[globals.CurrentBackBufferIndex],
	                                        D3D12_COMMAND_LIST_TYPE_DIRECT);

	globals.Fence = CreateFence(globals.Device);
	globals.FenceEvent = CreateEventHandle();

	// finished setting up!
	globals.IsInitialized = true;

	ShowWindow(globals.HWindow, SW_SHOW);

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (!::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			continue;
		}

		TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	Flush(globals.CommandQueue, globals.Fence, globals.FenceValue, globals.FenceEvent);

	CloseHandle(globals.FenceEvent);

	return 0;
}
#include "Application.h"

#include "DX12LibPCH.h"
#include "Application.h"
#include "resource.h"

#include "Game.h"
#include <map>

#include "CommandQueue.h"
#include "Helpers.h"
#include "Window.h"

constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12RenderWindowClass";

using WindowPtr = std::shared_ptr<Window>;
using WindowMap = std::map<HWND, WindowPtr>;
using WindowNameMap = std::map<std::wstring, WindowPtr>;

static Application* gsPSingleton = nullptr;
static WindowMap gsWindows;
static WindowNameMap gsWindowByName;

uint64_t Application::FrameCount;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// A wrapper struct to allow shared pointers for the window class.
struct MakeWindow final : Window
{
    MakeWindow(const HWND hWnd, const std::wstring& windowName, const int clientWidth, const int clientHeight,
               const bool vSync)
        : Window(hWnd, windowName, clientWidth, clientHeight, vSync)
    {
    }
};

Application::Application(const HINSTANCE hInst, const bool useWarp)
    : HInstance(hInst)
      , TearingSupported(false)
{
    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#if defined(_DEBUG)
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

    WNDCLASSEXW wndClass = {0};

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = &WndProc;
    wndClass.hInstance = HInstance;
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hIcon = LoadIcon(HInstance, MAKEINTRESOURCE(APP_ICON));
    wndClass.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = WINDOW_CLASS_NAME;
    wndClass.hIconSm = LoadIcon(HInstance, MAKEINTRESOURCE(APP_ICON));

    if (!RegisterClassExW(&wndClass))
    {
        MessageBoxA(nullptr, "Unable to register the window class.", "Error", MB_OK | MB_ICONERROR);
    }

    DxgiAdapter = GetAdapter(useWarp);
    if (DxgiAdapter)
    {
        D3d12Device = CreateDevice(DxgiAdapter);
    }
    if (D3d12Device)
    {
        DirectCommandQueue = std::make_shared<CommandQueue>(D3d12Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        ComputeCommandQueue = std::make_shared<CommandQueue>(D3d12Device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
        CopyCommandQueue = std::make_shared<CommandQueue>(D3d12Device, D3D12_COMMAND_LIST_TYPE_COPY);

        TearingSupported = CheckTearingSupport();

        // Create descriptor allocators
        for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            DescriptorAllocators[i] = std::make_unique<DescriptorAllocator>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
        }

        FrameCount = 0;
    }
}

void Application::Create(const HINSTANCE hInst, const bool useWarp)
{
    if (!gsPSingleton)
    {
        gsPSingleton = new Application(hInst, useWarp);
    }
}

Application& Application::Get()
{
    assert(gsPSingleton);
    return *gsPSingleton;
}

void Application::Destroy()
{
    if (gsPSingleton)
    {
        assert(gsWindows.empty() && gsWindowByName.empty() &&
            "All windows should be destroyed before destroying the application instance.");

        delete gsPSingleton;
        gsPSingleton = nullptr;
    }
}

Application::~Application()
{
    Flush();
}

ComPtr<IDXGIAdapter4> Application::GetAdapter(const bool useWarp) const
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

ComPtr<ID3D12Device2> Application::CreateDevice(const ComPtr<IDXGIAdapter4> adapter) const
{
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));
    //    NAME_D3D12_OBJECT(d3d12Device2);

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            // This warning occurs when using capture frame while graphics debugging.
        };

        D3D12_INFO_QUEUE_FILTER newFilter = {};
        //newFilter.DenyList.NumCategories = _countof(Categories);
        //newFilter.DenyList.pCategoryList = Categories;
        newFilter.DenyList.NumSeverities = _countof(severities);
        newFilter.DenyList.pSeverityList = severities;
        newFilter.DenyList.NumIDs = _countof(denyIds);
        newFilter.DenyList.pIDList = denyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&newFilter));
    }
#endif

    return d3d12Device2;
}

bool Application::CheckTearingSupport() const
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
            factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                          &allowTearing, sizeof(allowTearing));
        }
    }

    return allowTearing == TRUE;
}

bool Application::IsTearingSupported() const
{
    return TearingSupported;
}

std::shared_ptr<Window> Application::CreateRenderWindow(const std::wstring& windowName, int clientWidth,
                                                        int clientHeight, bool vSync) const
{
    // First check if a window with the given name already exists.
    const auto windowIterator = gsWindowByName.find(windowName);
    if (windowIterator != gsWindowByName.end())
    {
        return windowIterator->second;
    }

    RECT windowRect = {0, 0, clientWidth, clientHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(WINDOW_CLASS_NAME, windowName.c_str(),
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              windowRect.right - windowRect.left,
                              windowRect.bottom - windowRect.top,
                              nullptr, nullptr, HInstance, nullptr);

    if (!hWnd)
    {
        MessageBoxA(nullptr, "Could not create the render window.", "Error", MB_OK | MB_ICONERROR);
        return nullptr;
    }

    WindowPtr pWindow = std::make_shared<MakeWindow>(hWnd, windowName, clientWidth, clientHeight, vSync);

    gsWindows.insert(WindowMap::value_type(hWnd, pWindow));
    gsWindowByName.insert(WindowNameMap::value_type(windowName, pWindow));

    return pWindow;
}

void Application::DestroyWindow(const std::shared_ptr<Window> window)
{
    if (window)
        window->Destroy();
}

void Application::DestroyWindow(const std::wstring& windowName) const
{
    const WindowPtr pWindow = GetWindowByName(windowName);
    if (pWindow)
    {
        DestroyWindow(pWindow);
    }
}

std::shared_ptr<Window> Application::GetWindowByName(const std::wstring& windowName)
{
    std::shared_ptr<Window> window;
    const auto iterator = gsWindowByName.find(windowName);
    if (iterator != gsWindowByName.end())
    {
        window = iterator->second;
    }

    return window;
}


int Application::Run(const std::shared_ptr<Game> pGame)
{
    if (!pGame->Initialize()) return 1;
    if (!pGame->LoadContent()) return 2;

    MSG msg = {nullptr};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Flush any commands in the commands queues before quitting.
    Flush();

    pGame->UnloadContent();
    pGame->Destroy();

    return static_cast<int>(msg.wParam);
}

void Application::Quit(const int exitCode)
{
    PostQuitMessage(exitCode);
}

ComPtr<ID3D12Device2> Application::GetDevice() const
{
    return D3d12Device;
}

std::shared_ptr<CommandQueue> Application::GetCommandQueue(const D3D12_COMMAND_LIST_TYPE type) const
{
    std::shared_ptr<CommandQueue> commandQueue;
    switch (type)
    {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        commandQueue = DirectCommandQueue;
        break;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        commandQueue = ComputeCommandQueue;
        break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
        commandQueue = CopyCommandQueue;
        break;
    default:
        assert(false && "Invalid command queue type.");
    }

    return commandQueue;
}

void Application::Flush() const
{
    DirectCommandQueue->Flush();
    ComputeCommandQueue->Flush();
    CopyCommandQueue->Flush();
}

DescriptorAllocation Application::AllocateDescriptors(const D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                      const uint32_t numDescriptors) const
{
    return DescriptorAllocators[type]->Allocate(numDescriptors);
}

void Application::ReleaseStaleDescriptors(const uint64_t finishedFrame) const
{
    for (const auto& descriptorAllocator : DescriptorAllocators)
    {
        descriptorAllocator->ReleaseStaleDescriptors(finishedFrame);
    }
}

ComPtr<ID3D12DescriptorHeap> Application::CreateDescriptorHeap(const UINT numDescriptors,
                                                               const D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    D3D12_DESCRIPTOR_HEAP_DESC desc;
    desc.Type = type;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    ThrowIfFailed(D3d12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

UINT Application::GetDescriptorHandleIncrementSize(const D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    return D3d12Device->GetDescriptorHandleIncrementSize(type);
}


// Remove a window from our window lists.
static void RemoveWindow(const HWND hWnd)
{
    const auto windowIterator = gsWindows.find(hWnd);
    if (windowIterator != gsWindows.end())
    {
        const WindowPtr pWindow = windowIterator->second;
        gsWindowByName.erase(pWindow->GetWindowName());
        gsWindows.erase(windowIterator);
    }
}

// Convert the message ID into a MouseButton ID
MouseButtonEventArgs::MouseButton DecodeMouseButton(const UINT messageID)
{
    MouseButtonEventArgs::MouseButton mouseButton = MouseButtonEventArgs::None;
    switch (messageID)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        {
            mouseButton = MouseButtonEventArgs::Left;
        }
        break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
        {
            mouseButton = MouseButtonEventArgs::Right;
        }
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
        {
            mouseButton = MouseButtonEventArgs::Middle;
        }
        break;
    default:
        break;
    }

    return mouseButton;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowPtr pWindow;
    {
        auto iterator = gsWindows.find(hWnd);
        if (iterator != gsWindows.end())
        {
            pWindow = iterator->second;
        }
    }

    if (pWindow)
    {
        switch (message)
        {
        case WM_PAINT:
            {
                // Delta time will be filled in by the Window.
                UpdateEventArgs updateEventArgs(0.0f, 0.0f);
                pWindow->OnUpdate(updateEventArgs);
                RenderEventArgs renderEventArgs(0.0f, 0.0f);
                // Delta time will be filled in by the Window.
                pWindow->OnRender(renderEventArgs);
            }
            break;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            {
                MSG charMsg;
                // Get the Unicode character (UTF-16)
                unsigned int c = 0;
                // For printable characters, the next message will be WM_CHAR.
                // This message contains the character code we need to send the KeyPressed event.
                // Inspired by the SDL 1.2 implementation.
                if (PeekMessage(&charMsg, hWnd, 0, 0, PM_NOREMOVE) && charMsg.message == WM_CHAR)
                {
                    GetMessage(&charMsg, hWnd, 0, 0);
                    c = static_cast<unsigned int>(charMsg.wParam);
                }
                bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                auto key = static_cast<KeyCode::Key>(wParam);
                unsigned int scanCode = (lParam & 0x00FF0000) >> 16;
                KeyEventArgs keyEventArgs(key, c, KeyEventArgs::Pressed, shift, control, alt);
                pWindow->OnKeyPressed(keyEventArgs);
            }
            break;
        case WM_SYSKEYUP:
        case WM_KEYUP:
            {
                bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                auto key = static_cast<KeyCode::Key>(wParam);
                unsigned int c = 0;
                unsigned int scanCode = (lParam & 0x00FF0000) >> 16;

                // Determine which key was released by converting the key code and the scan code
                // to a printable character (if possible).
                // Inspired by the SDL 1.2 implementation.
                unsigned char keyboardState[256];
                GetKeyboardState(keyboardState);
                wchar_t translatedCharacters[4];
                if (int result = ToUnicodeEx(static_cast<UINT>(wParam), scanCode, keyboardState, translatedCharacters,
                                             4, 0, nullptr) > 0)
                {
                    c = translatedCharacters[0];
                }

                KeyEventArgs keyEventArgs(key, c, KeyEventArgs::Released, shift, control, alt);
                pWindow->OnKeyReleased(keyEventArgs);
            }
            break;
        // The default window procedure will play a system notification sound 
        // when pressing the Alt+Enter keyboard combination if this message is 
        // not handled.
        case WM_SYSCHAR:
            break;
        case WM_MOUSEMOVE:
            {
                bool lButton = (wParam & MK_LBUTTON) != 0;
                bool rButton = (wParam & MK_RBUTTON) != 0;
                bool mButton = (wParam & MK_MBUTTON) != 0;
                bool shift = (wParam & MK_SHIFT) != 0;
                bool control = (wParam & MK_CONTROL) != 0;

                int x = static_cast<short>(LOWORD(lParam));
                int y = static_cast<short>(HIWORD(lParam));

                MouseMotionEventArgs mouseMotionEventArgs(lButton, mButton, rButton, control, shift, x, y);
                pWindow->OnMouseMoved(mouseMotionEventArgs);
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            {
                bool lButton = (wParam & MK_LBUTTON) != 0;
                bool rButton = (wParam & MK_RBUTTON) != 0;
                bool mButton = (wParam & MK_MBUTTON) != 0;
                bool shift = (wParam & MK_SHIFT) != 0;
                bool control = (wParam & MK_CONTROL) != 0;

                int x = static_cast<short>(LOWORD(lParam));
                int y = static_cast<short>(HIWORD(lParam));

                MouseButtonEventArgs mouseButtonEventArgs(DecodeMouseButton(message), MouseButtonEventArgs::Pressed,
                                                          lButton, mButton, rButton, control, shift, x, y);
                pWindow->OnMouseButtonPressed(mouseButtonEventArgs);
            }
            break;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            {
                bool lButton = (wParam & MK_LBUTTON) != 0;
                bool rButton = (wParam & MK_RBUTTON) != 0;
                bool mButton = (wParam & MK_MBUTTON) != 0;
                bool shift = (wParam & MK_SHIFT) != 0;
                bool control = (wParam & MK_CONTROL) != 0;

                int x = static_cast<short>(LOWORD(lParam));
                int y = static_cast<short>(HIWORD(lParam));

                MouseButtonEventArgs mouseButtonEventArgs(DecodeMouseButton(message), MouseButtonEventArgs::Released,
                                                          lButton, mButton, rButton, control, shift, x, y);
                pWindow->OnMouseButtonReleased(mouseButtonEventArgs);
            }
            break;
        case WM_MOUSEWHEEL:
            {
                // The distance the mouse wheel is rotated.
                // A positive value indicates the wheel was rotated to the right.
                // A negative value indicates the wheel was rotated to the left.
                float zDelta = static_cast<int>(static_cast<short>(HIWORD(wParam))) / static_cast<float>(WHEEL_DELTA);
                short keyStates = static_cast<short>(LOWORD(wParam));

                bool lButton = (keyStates & MK_LBUTTON) != 0;
                bool rButton = (keyStates & MK_RBUTTON) != 0;
                bool mButton = (keyStates & MK_MBUTTON) != 0;
                bool shift = (keyStates & MK_SHIFT) != 0;
                bool control = (keyStates & MK_CONTROL) != 0;

                int x = static_cast<short>(LOWORD(lParam));
                int y = static_cast<short>(HIWORD(lParam));

                // Convert the screen coordinates to client coordinates.
                POINT clientToScreenPoint;
                clientToScreenPoint.x = x;
                clientToScreenPoint.y = y;
                ScreenToClient(hWnd, &clientToScreenPoint);

                MouseWheelEventArgs mouseWheelEventArgs(zDelta, lButton, mButton, rButton, control, shift,
                                                        clientToScreenPoint.x, clientToScreenPoint.y);
                pWindow->OnMouseWheel(mouseWheelEventArgs);
            }
            break;
        case WM_SIZE:
            {
                int width = static_cast<short>(LOWORD(lParam));
                int height = static_cast<short>(HIWORD(lParam));

                ResizeEventArgs resizeEventArgs(width, height);
                pWindow->OnResize(resizeEventArgs);
            }
            break;
        case WM_DESTROY:
            {
                // If a window is being destroyed, remove it from the 
                // window maps.
                RemoveWindow(hWnd);

                if (gsWindows.empty())
                {
                    // If there are no more windows, quit the application.
                    PostQuitMessage(0);
                }
            }
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
        }
    }
    else
    {
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }

    return 0;
}

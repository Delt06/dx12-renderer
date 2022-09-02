/**
* @brief A window for our application.
*/
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_5.h>
#include <memory>
#include <string>

#include "Events.h"
#include "HighResolutionClock.h"

// Forward-declare the DirectXTemplate class.
class Game;

class Window
{
public:
	// Number of swap chain back buffers.
	static constexpr UINT BUFFER_COUNT = 3;

	/**
	* Get a handle to this window's instance.
	* @returns The handle to the window instance or nullptr if this is not a valid window.
	*/
	HWND GetWindowHandle() const;

	/**
	* Destroy this window.
	*/
	void Destroy();

	const std::wstring& GetWindowName() const;

	int GetClientWidth() const;
	int GetClientHeight() const;

	/**
	* Should this window be rendered with vertical refresh synchronization.
	*/
	bool IsVSync() const;
	void SetVSync(bool vSync);
	void ToggleVSync();

	/**
	* Is this a windowed window or full-screen?
	*/
	bool IsFullScreen() const;

	// Set the fullscreen state of the window.
	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen();

	/**
	 * Show this window.
	 */
	void Show();

	/**
	 * Hide the window.
	 */
	void Hide();

	/**
	 * Return the current back buffer index.
	 */
	UINT GetCurrentBackBufferIndex() const;

	/**
	 * Present the swap chain's back buffer to the screen.
	 * Returns the current back buffer index after the present.
	 */
	UINT Present();

	/**
	 * Get the render target view for the current back buffer.
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;

	/**
	 * Get the back buffer resource for the current back buffer.
	 */
	Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentBackBuffer() const;


protected:
	// The Window procedure needs to call protected methods of this class.
	friend LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	// Only the application can create a window.
	friend class Application;
	// The DirectXTemplate class needs to register itself with a window.
	friend class Game;

	Window() = delete;
	Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync);
	virtual ~Window();

	// Register a Game with this window. This allows
	// the window to callback functions in the Game class.
	void RegisterCallbacks(std::shared_ptr<Game> pGame);

	// Update and Draw can only be called by the application.
	virtual void OnUpdate(UpdateEventArgs& e);
	virtual void OnRender(RenderEventArgs& e);

	// A keyboard key was pressed
	virtual void OnKeyPressed(KeyEventArgs& e);
	// A keyboard key was released
	virtual void OnKeyReleased(KeyEventArgs& e);

	// The mouse was moved
	virtual void OnMouseMoved(MouseMotionEventArgs& e);
	// A button on the mouse was pressed
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
	// A button on the mouse was released
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
	// The mouse wheel was moved.
	virtual void OnMouseWheel(MouseWheelEventArgs& e);

	// The window was resized.
	virtual void OnResize(ResizeEventArgs& e);

	// Create the swap chain.
	Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain();

	// Update the render target views for the swap chain back buffers.
	void UpdateRenderTargetViews();

private:
	// Windows should not be copied.
	Window(const Window& copy) = delete;
	Window& operator=(const Window& other) = delete;

	HWND HWnd;

	std::wstring WindowName;

	int ClientWidth;
	int ClientHeight;
	bool VSync;
	bool Fullscreen;

	HighResolutionClock UpdateClock;
	HighResolutionClock RenderClock;
	uint64_t FrameCounter;

	std::weak_ptr<Game> PGame;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> DxgiSwapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> D3d12RtvDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> D3d12BackBuffers[BUFFER_COUNT];

	UINT RtvDescriptorSize;
	UINT CurrentBackBufferIndex;

	RECT WindowRect;
	bool IsTearingSupported;
};

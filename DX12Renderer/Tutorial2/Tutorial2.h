#pragma once

#include "../Game.h"
#include "../Window.h"

#include <DirectXMath.h>

class Tutorial2 final : public Game
{
public:
	using Base = Game;

	Tutorial2(const std::wstring& name, int width, int height, bool vSync = false);

	bool LoadContent() override;

	void UnloadContent() override;

protected:
	void OnUpdate(UpdateEventArgs& e) override;

	void OnRender(RenderEventArgs& e) override;

	void OnKeyPressed(KeyEventArgs& e) override;

	void OnMouseWheel(MouseWheelEventArgs& e) override;

	void OnResize(ResizeEventArgs& e) override;

private:
	static void TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	                               const Microsoft::WRL::ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES beforeState,
	                               D3D12_RESOURCE_STATES afterState);

	static void ClearRtv(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	                     D3D12_CPU_DESCRIPTOR_HANDLE rtv, const FLOAT* clearColor);

	static void ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
	                       FLOAT depth = 1.0f);

	// Create a GPU buffer
	void UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	                          ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
	                          size_t numElements, size_t elementSize, const void* bufferData,
	                          D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
	);

	void ResizeDepthBuffer(int width, int height);

	uint64_t FenceValues[Window::BUFFER_COUNT] = {};

	// vertex buffer for the cube
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	// index buffer for the cube
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> DepthBuffer;
	// descriptor heap for the depth buffer
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DsvHeap;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	float Fov;

	DirectX::XMMATRIX ModelMatrix;
	DirectX::XMMATRIX ViewMatrix;
	DirectX::XMMATRIX ProjectionMatrix;

	bool IsContentLoaded;
};

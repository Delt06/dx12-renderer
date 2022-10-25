#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

#include "RenderTarget.h"
#include <cstdint>
#include <memory>

class CommandList;

class Cubemap
{
public:
	Cubemap(uint32_t size, DirectX::XMVECTOR position, CommandList& commandList, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, D3D12_CLEAR_VALUE clearColor);

	void Clear(CommandList& commandList);

	const std::shared_ptr<Texture>& GetTexture() const;
	const std::shared_ptr<Texture>& GetFallbackTexture() const;
	D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() const;

	void SetRenderTarget(CommandList& commandList, uint32_t sideIndex) const;
	void SetViewportScissorRect(CommandList& commandList) const;
	void ComputeViewProjectionMatrices(CommandList& commandList, uint32_t sideIndex, DirectX::XMMATRIX* viewDest, DirectX::XMMATRIX* projectionDest) const;

	struct CubeSideOrientation
	{
		DirectX::XMVECTOR Forward;
		DirectX::XMVECTOR Up;
	};

	static constexpr uint32_t SIDES_COUNT = 6;

	inline static const CubeSideOrientation SIDE_ORIENTATIONS[SIDES_COUNT] =
	{
		// +X
		{DirectX::XMVectorSet(1, 0, 0, 0), DirectX::XMVectorSet(0, 1, 0, 0)},
		// -X
		{DirectX::XMVectorSet(-1, 0, 0, 0), DirectX::XMVectorSet(0, 1, 0, 0)},
		// +Y
		{DirectX::XMVectorSet(0, 1, 0, 0), DirectX::XMVectorSet(0, 0, -1, 0)},
		// -Y
		{DirectX::XMVectorSet(0, -1, 0, 0), DirectX::XMVectorSet(0, 0, 1, 0)},
		// +Z
		{DirectX::XMVectorSet(0, 0, 1, 0), DirectX::XMVectorSet(0, 1, 0, 0)},
		// -Z
		{DirectX::XMVectorSet(0, 0, -1, 0), DirectX::XMVectorSet(0, 1, 0, 0)},
	};

private:
	uint32_t m_Size;
	DirectX::XMVECTOR m_Position;
	D3D12_CLEAR_VALUE m_ClearColor;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	D3D12_SHADER_RESOURCE_VIEW_DESC m_SrvDesc;
	std::shared_ptr<Texture> m_FallbackCubemap;
	RenderTarget m_RenderedCubemap;
};


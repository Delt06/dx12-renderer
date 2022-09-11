#include "Cubemap.h"

#include <d3d12.h>
#include <d3dx12.h>

#include <CommandList.h>

Cubemap::Cubemap(uint32_t size, DirectX::XMVECTOR position, CommandList& commandList, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, D3D12_CLEAR_VALUE clearColor)
	: m_Size(size)
	, m_Position(position)
	, m_ClearColor(clearColor)
	, m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(size), static_cast<float>(size)))
	, m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
	auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
		size, size,
		SIDES_COUNT,
		1, 1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
	);

	// Create a render target
	{
		auto colorTexture = Texture(colorDesc, &m_ClearColor,
			TextureUsageType::RenderTarget,
			L"Cubemap Color Render Target");

		auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat,
			size, size,
			SIDES_COUNT, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		D3D12_CLEAR_VALUE depthClearValue;
		depthClearValue.Format = depthDesc.Format;
		depthClearValue.DepthStencil = { 1.0f, 0 };

		auto depthTexture = Texture(depthDesc, &depthClearValue,
			TextureUsageType::Depth,
			L"Cubemap Depth Render Target");

		m_RenderedCubemap.AttachTexture(Color0, colorTexture);
		m_RenderedCubemap.AttachTexture(DepthStencil, depthTexture);
	}

	// Set up SRV for the cubemap
	{
		m_SrvDesc = {};
		m_SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		m_SrvDesc.Format = backBufferFormat;
		m_SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		m_SrvDesc.TextureCube.MipLevels = 1;
		m_SrvDesc.TextureCube.MostDetailedMip = 0;
		m_SrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}

	// Create empty cubemap
	{
		auto fallbackColorDesc = colorDesc;
		fallbackColorDesc.Width = 1;
		fallbackColorDesc.Height = 1;
		m_FallbackCubemap = Texture(colorDesc, &m_ClearColor, TextureUsageType::RenderTarget, L"Fallback Cubemap");
	}
}

void Cubemap::Clear(CommandList& commandList)
{
	commandList.ClearTexture(m_RenderedCubemap.GetTexture(Color0), m_ClearColor.Color);
	commandList.ClearDepthStencilTexture(m_RenderedCubemap.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
	commandList.ClearTexture(m_FallbackCubemap, m_ClearColor.Color);
}

void Cubemap::SetShaderResourceView(CommandList& commandList, uint32_t rootParameterIndex, uint32_t descriptorOffset) const
{
	constexpr auto stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	auto resource = m_RenderedCubemap.GetTexture(Color0);
	commandList.SetShaderResourceView(rootParameterIndex, descriptorOffset, resource, stateAfter, 0, SIDES_COUNT, &m_SrvDesc);
}

void Cubemap::SetEmptyShaderResourceView(CommandList& commandList, uint32_t rootParameterIndex, uint32_t descriptorOffset) const
{
	constexpr auto stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList.SetShaderResourceView(rootParameterIndex, descriptorOffset, m_FallbackCubemap, stateAfter, 0, SIDES_COUNT, &m_SrvDesc);
}

void Cubemap::SetRenderTarget(CommandList& commandList, uint32_t sideIndex) const
{
	commandList.SetRenderTarget(m_RenderedCubemap, sideIndex);
}

void Cubemap::SetViewportScissorRect(CommandList& commandList) const
{
	commandList.SetViewport(m_Viewport);
	commandList.SetScissorRect(m_ScissorRect);
}

void Cubemap::ComputeViewProjectionMatrices(CommandList& commandList, uint32_t sideIndex, DirectX::XMMATRIX* viewDest, DirectX::XMMATRIX* projectionDest) const
{
	auto orientation = SIDE_ORIENTATIONS[sideIndex];
	*viewDest = DirectX::XMMatrixLookToLH(m_Position, orientation.Forward, orientation.Up);
	*projectionDest = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.0f), 1, 0.01f, 100.0f);
}


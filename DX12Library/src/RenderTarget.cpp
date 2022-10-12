﻿#include "DX12LibPCH.h"

#include "RenderTarget.h"

RenderTarget::RenderTarget()
	: m_Textures(NumAttachmentPoints)
{
	for (auto& pTexture : m_Textures)
	{
		pTexture = std::make_shared<Texture>();
	}
}

// Attach a texture to the render target.
// The texture will be copied into the texture array.
void RenderTarget::AttachTexture(AttachmentPoint attachmentPoint, const std::shared_ptr<Texture>& texture)
{
	m_Textures[attachmentPoint] = texture;
}

const std::shared_ptr<Texture>& RenderTarget::GetTexture(AttachmentPoint attachmentPoint) const
{
	return m_Textures[attachmentPoint];
}

// Resize all of the textures associated with the render target.
void RenderTarget::Resize(uint32_t width, uint32_t height)
{
	for (auto& texture : m_Textures)
	{
		texture->Resize(width, height);
	}
}

// Get a list of the textures attached to the render target.
// This method is primarily used by the CommandList when binding the
// render target to the output merger stage of the rendering pipeline.
const std::vector<std::shared_ptr<Texture>>& RenderTarget::GetTextures() const
{
	return m_Textures;
}

D3D12_RT_FORMAT_ARRAY RenderTarget::GetRenderTargetFormats() const
{
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};

	for (int i = Color0; i <= Color7; ++i)
	{
		const auto& texture = m_Textures[i];
		if (texture->IsValid())
		{
			rtvFormats.RTFormats[rtvFormats.NumRenderTargets++] = texture->GetD3D12ResourceDesc().Format;
		}
	}

	return rtvFormats;
}

DXGI_FORMAT RenderTarget::GetDepthStencilFormat() const
{
	DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
	const auto& depthStencilTexture = m_Textures[DepthStencil];
	if (depthStencilTexture->IsValid())
	{
		dsvFormat = depthStencilTexture->GetD3D12ResourceDesc().Format;
	}

	return dsvFormat;
}

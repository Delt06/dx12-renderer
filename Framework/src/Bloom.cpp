#include "Bloom.h"
#include "DX12Library/Helpers.h"

namespace
{
	CD3DX12_RESOURCE_DESC CreateRenderTargetDesc(DXGI_FORMAT backBufferFormat, uint32_t width, uint32_t height)
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
			width, height,
			1, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	}
}

Bloom::Bloom(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat, size_t pyramidSize)
	: m_Width(width)
	, m_Height(height)
	, m_Prefilter(rootSignature, commandList)
	, m_Downsample(rootSignature, commandList)
	, m_Upsample(rootSignature, commandList)
{
	// create intermediate textures
	{
		for (size_t i = 0; i < pyramidSize - 1; ++i)
		{
			CreateIntermediateTexture(width,
				height,
				m_IntermediateTextures,
				i,
				L"Bloom Intermediate Texture",
				backBufferFormat);
		}
	}
}

void Bloom::Resize(uint32_t width, uint32_t height)
{
	m_Width = width;
	m_Height = height;

	for (size_t i = 0; i < m_IntermediateTextures.size(); ++i)
	{
		uint32_t textureWidth = 0, textureHeight = 0;
		GetIntermediateTextureSize(width, height, i, textureWidth, textureHeight);
		m_IntermediateTextures[i].Resize(textureWidth, textureHeight);
	}
}

void Bloom::Draw(CommandList& commandList,
	const std::shared_ptr<Texture>& source,
	const RenderTarget& destination,
	const BloomParameters& parameters)
{
	PIXScope(commandList, "Bloom");

	m_Prefilter.Execute(commandList, parameters, source, m_IntermediateTextures[0]);

	for (size_t i = 1; i < m_IntermediateTextures.size(); ++i)
	{
		const auto& previousTexture = m_IntermediateTextures[i - 1].GetTexture(Color0);
		auto& currentTexture = m_IntermediateTextures[i];
		m_Downsample.Execute(commandList, parameters, previousTexture, currentTexture);
	}

	for (size_t i = m_IntermediateTextures.size() - 1; i >= 1; --i)
	{
		auto& currentTexture = m_IntermediateTextures[i - 1];
		const auto& nextTexture = m_IntermediateTextures[i].GetTexture(Color0);
		m_Upsample.Execute(commandList, parameters, nextTexture, currentTexture);
	}

	m_Upsample.Execute(commandList, parameters, m_IntermediateTextures[0].GetTexture(Color0), destination);
}

void Bloom::GetIntermediateTextureSize(uint32_t width,
	uint32_t height,
	size_t index,
	uint32_t& outWidth,
	uint32_t& outHeight)
{
	outWidth = width;
	outHeight = height;

	for (size_t i = 0; i <= index; i++)
	{
		outWidth = max(1, outWidth >> 1);
		outHeight = max(1, outHeight >> 1);
	}
}

void Bloom::CreateIntermediateTexture(uint32_t width,
	uint32_t height,
	std::vector<RenderTarget>& destinationList,
	size_t index,
	const std::wstring& name,
	DXGI_FORMAT format)
{
	uint32_t textureWidth, textureHeight;
	GetIntermediateTextureSize(width, height, index, textureWidth, textureHeight);

	auto desc = CreateRenderTargetDesc(format, textureWidth, textureHeight);
	auto intermediateTexture = std::make_shared<Texture>(desc, nullptr, TextureUsageType::RenderTarget, name);

	auto renderTarget = RenderTarget();
	renderTarget.AttachTexture(Color0, intermediateTexture);
	destinationList.push_back(renderTarget);
}

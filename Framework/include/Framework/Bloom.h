#pragma once

#include "BloomPrefilter.h"
#include "BloomParameters.h"
#include "BloomDownsample.h"
#include "BloomUpsample.h"
#include <Framework/CommonRootSignature.h>
#include <memory>
#include <DX12Library/CommandList.h>
#include <cstdint>

class Bloom
{
public:
	explicit Bloom(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat, size_t pyramidSize = 8);

	void Resize(uint32_t width, uint32_t height);

	void Draw(CommandList& commandList, const std::shared_ptr<Texture>& source, const RenderTarget& destination, const BloomParameters& parameters);

private:
	static void GetIntermediateTextureSize(uint32_t width, uint32_t height, size_t index, uint32_t& outWidth, uint32_t& outHeight);
	static void CreateIntermediateTexture(uint32_t width, uint32_t height, std::vector<RenderTarget>& destinationList, size_t index, const std::wstring& name, DXGI_FORMAT format);

	BloomPrefilter m_Prefilter;
	BloomDownsample m_Downsample;
	BloomUpsample m_Upsample;

	uint32_t m_Width, m_Height;
	std::vector<RenderTarget> m_IntermediateTextures;
};
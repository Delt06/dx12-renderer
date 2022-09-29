#pragma once
#include "Shader.h"
#include "BloomPrefilter.h"
#include "BloomParameters.h"
#include "BloomDownsample.h"
#include "BloomUpsample.h"

class Bloom
{
public:
	explicit Bloom(uint32_t width, uint32_t height, Shader::Format backBufferFormat, size_t pyramidSize = 8);

	void Init(Microsoft::WRL::ComPtr<Shader::IDevice> device, CommandList& commandList);

	void Resize(uint32_t width, uint32_t height);

	void Draw(CommandList& commandList, const Texture& source, const RenderTarget& destination, const BloomParameters& parameters);

private:
	static void GetIntermediateTextureSize(uint32_t width, uint32_t height, size_t index, uint32_t& outWidth, uint32_t& outHeight);
	static void CreateIntermediateTexture(uint32_t width, uint32_t height, std::vector<RenderTarget>& destinationList, size_t index, const std::wstring& name, DXGI_FORMAT format);

	BloomPrefilter m_Prefilter;
	BloomDownsample m_Downsample;
	BloomUpsample m_Upsample;

	uint32_t m_Width, m_Height;
	std::vector<RenderTarget> m_IntermediateTextures;
};
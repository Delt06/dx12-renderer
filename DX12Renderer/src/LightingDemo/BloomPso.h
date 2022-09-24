#pragma once

#include <wrl.h>
#include <d3d12.h>

#include <memory>
#include <vector>
#include <cstdint>
#include <string>

#include "../RootSignature.h"

class Mesh;
class CommandList;
class Texture;
class RenderTarget;

class BloomPso
{
public:
	explicit BloomPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat);

	void Resize(uint32_t width, uint32_t height);

	struct Parameters
	{
		float Intensity;
		float Threshold;
		float SoftThreshold;
	};

	void Draw(CommandList& commandList, const Texture& source, const RenderTarget& destination, const Parameters& parameters);

private:
	static void GetIntermediateTextureSize(uint32_t width, uint32_t height, size_t index, uint32_t& outWidth, uint32_t& outHeight);
	static void CreateIntermediateTexture(uint32_t width, uint32_t height, std::vector<RenderTarget>& destinationList, size_t index, std::wstring name, DXGI_FORMAT format);

	std::shared_ptr<Mesh> m_BlitTriangle;
	std::vector<RenderTarget> m_IntermediateTextures;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PrefilterPipelineState;
	RootSignature m_PrefilterRootSignature;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_DownsamplePipelineState;
	RootSignature m_DownsampleRootSignature;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_UpsamplePipelineState;
	RootSignature m_UpsampleRootSignature;

	uint32_t m_Width, m_Height;
	D3D12_RECT m_ScissorRect;
};


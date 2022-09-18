#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "RootSignature.h"
#include "StructuredBuffer.h"

class CommandList;
class Texture;

class AutoExposurePso
{
public:
	explicit AutoExposurePso(Microsoft::WRL::ComPtr<ID3D12Device2> device);

	void Dispatch(CommandList& commandList, const Texture& hdrTexture);

private:
	static constexpr uint32_t NUM_HISTOGRAM_BINS = 256;

	RootSignature m_BuildLuminanceHistogramRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_BuildLuminanceHistogramPipelineState;
	StructuredBuffer m_LuminanceHistogram;
};


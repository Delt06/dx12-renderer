#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "RootSignature.h"
#include "StructuredBuffer.h"
#include "Texture.h"

class CommandList;

class AutoExposurePso
{
public:
	explicit AutoExposurePso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList);

	void Dispatch(CommandList& commandList, const Texture& hdrTexture, float deltaTime, float tau = 1.1f);

	const Texture& GetLuminanceOutput() const;

private:
	static constexpr uint32_t NUM_HISTOGRAM_BINS = 256;

	RootSignature m_BuildLuminanceHistogramRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_BuildLuminanceHistogramPipelineState;

	RootSignature m_AverageLuminanceHistogramRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_AverageLuminanceHistogramPipelineState;

	StructuredBuffer m_LuminanceHistogram;
	Texture m_LuminanceOutput;
};


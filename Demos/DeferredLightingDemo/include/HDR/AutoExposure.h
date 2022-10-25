#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <DX12Library/RootSignature.h>
#include <DX12Library/StructuredBuffer.h>
#include <DX12Library/Texture.h>
#include <memory>
#include <Framework/CommonRootSignature.h>

class CommandList;

class AutoExposure
{
public:
	explicit AutoExposure(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void Dispatch(CommandList& commandList, const std::shared_ptr<Texture>& hdrTexture, float deltaTime, float tau = 1.1f);

	const std::shared_ptr<Texture>& GetLuminanceOutput() const;

private:
	static constexpr uint32_t NUM_HISTOGRAM_BINS = 256;

	std::shared_ptr<CommonRootSignature> m_RootSignature;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_BuildLuminanceHistogramPipelineState;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_AverageLuminanceHistogramPipelineState;

	std::shared_ptr<StructuredBuffer> m_LuminanceHistogram;
	std::shared_ptr<Texture> m_LuminanceOutput;
};


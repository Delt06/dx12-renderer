#include "AutoExposurePso.h"
#include "HDR_AutoExposure_BuildLuminanceHistogram_CS.h"
#include "CommandList.h"
#include <d3d12.h>
#include "Helpers.h"
#include "Texture.h"
#include "StructuredBuffer.h"

namespace
{
	namespace BuildLuminanceHistogramRootParameters
	{
		enum RootParameters
		{
			HDRTexture,
			LuminanceHistogram,
			LuminanceHistogramParametersCB,
			NumRootParameters
		};

		struct LuminanceHistogramParameters
		{
			uint32_t InputWidth;
			uint32_t InputHeight;
			float MinLogLuminance;
			float OneOverLogLuminanceRange;
		};
	}

	
}

AutoExposurePso::AutoExposurePso(Microsoft::WRL::ComPtr<ID3D12Device2> device)
{
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		const CD3DX12_DESCRIPTOR_RANGE1 hdrTexture(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[BuildLuminanceHistogramRootParameters::NumRootParameters];
		rootParameters[BuildLuminanceHistogramRootParameters::HDRTexture].InitAsDescriptorTable(1, &hdrTexture);
		rootParameters[BuildLuminanceHistogramRootParameters::LuminanceHistogram].InitAsUnorderedAccessView(0, 0);
		rootParameters[BuildLuminanceHistogramRootParameters::LuminanceHistogramParametersCB].InitAsConstants(sizeof(BuildLuminanceHistogramRootParameters::LuminanceHistogramParameters) / sizeof(float), 0);

		const CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
			0,
			D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		);

		const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(
			BuildLuminanceHistogramRootParameters::NumRootParameters,
			rootParameters, 0, nullptr
		);

		m_BuildLuminanceHistogramRootSignature.SetRootSignatureDesc(
			rootSignatureDesc.Desc_1_1,
			featureData.HighestVersion
		);

		// Create the PSO for GenerateMips shader
		struct PipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS Cs;
		} pipelineStateStream;

		pipelineStateStream.PRootSignature = m_BuildLuminanceHistogramRootSignature.GetRootSignature().Get();
		pipelineStateStream.Cs = { buildLumimanceHistogram_Cs, sizeof buildLumimanceHistogram_Cs };

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{ sizeof(PipelineStateStream), &pipelineStateStream };
		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_BuildLuminanceHistogramPipelineState)));
	}

	auto luminanceHistogramDesc = CD3DX12_RESOURCE_DESC::Buffer(NUM_HISTOGRAM_BINS * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_LuminanceHistogram = StructuredBuffer(luminanceHistogramDesc, NUM_HISTOGRAM_BINS, sizeof(uint32_t), L"Luminance Histogram");
}

void AutoExposurePso::Dispatch(CommandList& commandList, const Texture& hdrTexture)
{
	PIXScope(commandList, "Compute Average Luminance");
	{
		PIXScope(commandList, "Build Luminance Histogram");

		commandList.SetPipelineState(m_BuildLuminanceHistogramPipelineState);
		commandList.SetComputeRootSignature(m_BuildLuminanceHistogramRootSignature);

		commandList.SetShaderResourceView(BuildLuminanceHistogramRootParameters::HDRTexture, 0, hdrTexture);

		commandList.SetComputeRootUnorderedAccessView(BuildLuminanceHistogramRootParameters::LuminanceHistogram, m_LuminanceHistogram);

		auto hdrTextureDesc = hdrTexture.GetD3D12ResourceDesc();
		BuildLuminanceHistogramRootParameters::LuminanceHistogramParameters parameters;
		parameters.InputWidth = static_cast<uint32_t>(hdrTextureDesc.Width);
		parameters.InputHeight = static_cast<uint32_t>(hdrTextureDesc.Height);
		parameters.MinLogLuminance = -10.0f;
		constexpr auto maxLogLuminance = 2.0f;
		parameters.OneOverLogLuminanceRange = 1.0f / (maxLogLuminance - parameters.MinLogLuminance);
		commandList.SetCompute32BitConstants(BuildLuminanceHistogramRootParameters::LuminanceHistogramParametersCB, parameters);

		commandList.Dispatch(Math::DivideByMultiple(parameters.InputWidth, 16), Math::DivideByMultiple(parameters.InputHeight, 16));
	}
}

#include <HDR/AutoExposure.h>
#include <CommandList.h>
#include <d3d12.h>
#include <Helpers.h>
#include <StructuredBuffer.h>
#include <ShaderUtils.h>

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

	namespace AverageLuminanceHistogramRootParameters
	{
		enum RootParameters
		{
			LuminanceHistogram,
			LuminanceOutput,
			LuminanceHistogramParametersCB,
			NumRootParameters
		};

		struct LuminanceHistogramParameters
		{
			uint32_t PixelCount;
			float MinLogLuminance;
			float LogLuminanceRange;
			float DeltaTime;

			float Tau;
			float _Padding[3];
		};
	}


}

AutoExposure::AutoExposure(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList)
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
		const auto computeShader = ShaderUtils::LoadShaderFromFile(L"HDR_AutoExposure_BuildLuminanceHistogram_CS.cso");
		pipelineStateStream.Cs = CD3DX12_SHADER_BYTECODE(computeShader.Get());

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{ sizeof(PipelineStateStream), &pipelineStateStream };
		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_BuildLuminanceHistogramPipelineState)));
	}

	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 luminanceOutputDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[AverageLuminanceHistogramRootParameters::NumRootParameters];
		rootParameters[AverageLuminanceHistogramRootParameters::LuminanceHistogram].InitAsUnorderedAccessView(0, 0);
		rootParameters[AverageLuminanceHistogramRootParameters::LuminanceOutput].InitAsDescriptorTable(1, &luminanceOutputDescriptorRange);
		rootParameters[AverageLuminanceHistogramRootParameters::LuminanceHistogramParametersCB].InitAsConstants(sizeof(AverageLuminanceHistogramRootParameters::LuminanceHistogramParameters) / sizeof(float), 0);

		const CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
			0,
			D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		);

		const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(
			AverageLuminanceHistogramRootParameters::NumRootParameters,
			rootParameters, 0, nullptr
		);

		m_AverageLuminanceHistogramRootSignature.SetRootSignatureDesc(
			rootSignatureDesc.Desc_1_1,
			featureData.HighestVersion
		);

		// Create the PSO for GenerateMips shader
		struct PipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS Cs;
		} pipelineStateStream;

		pipelineStateStream.PRootSignature = m_AverageLuminanceHistogramRootSignature.GetRootSignature().Get();
		const auto computeShader = ShaderUtils::LoadShaderFromFile(L"HDR_AutoExposure_AverageLuminanceHistogram_CS.cso");
		pipelineStateStream.Cs = CD3DX12_SHADER_BYTECODE(computeShader.Get());

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{ sizeof(PipelineStateStream), &pipelineStateStream };
		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_AverageLuminanceHistogramPipelineState)));
	}

	auto luminanceHistogramDesc = CD3DX12_RESOURCE_DESC::Buffer(NUM_HISTOGRAM_BINS * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_LuminanceHistogram = StructuredBuffer(luminanceHistogramDesc, NUM_HISTOGRAM_BINS, sizeof(uint32_t), L"Luminance Histogram");

	auto luminanceOutputDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, 1, 1, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_LuminanceOutput = Texture(luminanceOutputDesc, nullptr, TextureUsageType::Other, L"Luminance Output");

	D3D12_SUBRESOURCE_DATA luminanceOutputData;
	float data = 0;
	luminanceOutputData.pData = &data;
	luminanceOutputData.RowPitch = sizeof(float);
	commandList.CopyTextureSubresource(m_LuminanceOutput, 0, 1, &luminanceOutputData);
}

void AutoExposure::Dispatch(CommandList& commandList, const Texture& hdrTexture, float deltaTime, float tau)
{
	constexpr auto minLogLuminance = -10.0f;
	constexpr auto maxLogLuminance = 2.0f;
	constexpr auto logLuminanceRange = maxLogLuminance - minLogLuminance;

	auto hdrTextureDesc = hdrTexture.GetD3D12ResourceDesc();

	PIXScope(commandList, "Compute Average Luminance");
	{
		PIXScope(commandList, "Build Luminance Histogram");

		commandList.SetPipelineState(m_BuildLuminanceHistogramPipelineState);
		commandList.SetComputeRootSignature(m_BuildLuminanceHistogramRootSignature);

		commandList.SetShaderResourceView(BuildLuminanceHistogramRootParameters::HDRTexture, 0, hdrTexture);

		commandList.SetComputeRootUnorderedAccessView(BuildLuminanceHistogramRootParameters::LuminanceHistogram, m_LuminanceHistogram);

		BuildLuminanceHistogramRootParameters::LuminanceHistogramParameters parameters;
		parameters.InputWidth = static_cast<uint32_t>(hdrTextureDesc.Width);
		parameters.InputHeight = static_cast<uint32_t>(hdrTextureDesc.Height);
		parameters.MinLogLuminance = minLogLuminance;
		parameters.OneOverLogLuminanceRange = 1.0f / logLuminanceRange;
		commandList.SetCompute32BitConstants(BuildLuminanceHistogramRootParameters::LuminanceHistogramParametersCB, parameters);

		commandList.Dispatch(Math::DivideByMultiple(parameters.InputWidth, 16), Math::DivideByMultiple(parameters.InputHeight, 16));
	}

	commandList.UavBarrier(m_LuminanceHistogram);

	{
		PIXScope(commandList, "Average Luminance Histogram");

		commandList.SetPipelineState(m_AverageLuminanceHistogramPipelineState);
		commandList.SetComputeRootSignature(m_AverageLuminanceHistogramRootSignature);

		commandList.SetComputeRootUnorderedAccessView(AverageLuminanceHistogramRootParameters::LuminanceHistogram, m_LuminanceHistogram);
		commandList.SetUnorderedAccessView(AverageLuminanceHistogramRootParameters::LuminanceOutput, 0, m_LuminanceOutput);

		AverageLuminanceHistogramRootParameters::LuminanceHistogramParameters parameters;
		parameters.PixelCount = static_cast<uint32_t>(hdrTextureDesc.Width * hdrTextureDesc.Height);
		parameters.MinLogLuminance = minLogLuminance;
		parameters.LogLuminanceRange = logLuminanceRange;
		parameters.DeltaTime = deltaTime;
		parameters.Tau = tau;
		commandList.SetCompute32BitConstants(AverageLuminanceHistogramRootParameters::LuminanceHistogramParametersCB, parameters);

		commandList.Dispatch(1, 1);
	}

	commandList.UavBarrier(m_LuminanceOutput);
}

const Texture& AutoExposure::GetLuminanceOutput() const
{
	return m_LuminanceOutput;
}

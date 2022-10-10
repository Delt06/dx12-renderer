#include <Framework/BloomDownsample.h>
#include <Framework/Blit_VS.h>
#include <Framework/Bloom_Downsample_PS.h>
#include <Framework/Mesh.h>
#include <DirectXMath.h>
#include <DX12Library/Helpers.h>

using namespace DirectX;

namespace
{
	namespace RootParameters
	{
		enum
		{
			SourceTexture = 0,
			Parameters,
			NumRootParameters
		};

		struct ParametersCb
		{
			XMFLOAT2 TexelSize;
			float Intensity;
			float Padding;
		};
	}
}

BloomDownsample::BloomDownsample(Format backBufferFormat)
	: m_BackBufferFormat(backBufferFormat)
	, m_RootParameters(RootParameters::NumRootParameters)
	, m_StaticSamplers(1)
	, m_SourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0)
{
	m_RootParameters[RootParameters::SourceTexture].InitAsDescriptorTable(1, &m_SourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootParameters[RootParameters::Parameters].InitAsConstants(sizeof(RootParameters::ParametersCb) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	m_StaticSamplers[0] = StaticSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
}

void BloomDownsample::Execute(CommandList& commandList, const BloomParameters& parameters, const Texture& source, const RenderTarget& destination)
{
	PIXScope(commandList, "Bloom Downsample");

	SetRenderTarget(commandList, destination);

	commandList.SetShaderResourceView(RootParameters::SourceTexture, 0, source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	RootParameters::ParametersCb parametersCb{};
	parametersCb.Intensity = parameters.Intensity;
	auto sourceDesc = source.GetD3D12ResourceDesc();
	auto fSourceWidth = static_cast<float>(sourceDesc.Width);
	auto fSourceHeight = static_cast<float>(sourceDesc.Height);
	parametersCb.TexelSize = { 1 / fSourceWidth , 1 / fSourceHeight };
	commandList.SetGraphics32BitConstants(RootParameters::Parameters, parametersCb);

	m_BlitMesh->Draw(commandList);
}

CompositeEffect::ShaderBytecode BloomDownsample::GetVertexShaderBytecode() const
{
	return { ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS };
}

CompositeEffect::ShaderBytecode BloomDownsample::GetPixelShaderBytecode() const
{
	return { ShaderBytecode_Bloom_Downsample_PS, sizeof ShaderBytecode_Bloom_Downsample_PS };
}

std::vector<CompositeEffect::RootParameter> BloomDownsample::GetRootParameters() const
{
	return m_RootParameters;
}

std::vector<CompositeEffect::StaticSampler> BloomDownsample::GetStaticSamplers() const
{
	return m_StaticSamplers;
}

CompositeEffect::Format BloomDownsample::GetRenderTargetFormat() const
{
	return m_BackBufferFormat;
}

void BloomDownsample::OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	m_BlitMesh = Mesh::CreateBlitTriangle(commandList);
}

void BloomDownsample::Begin(CommandList& commandList)
{
	SetContext(commandList);
}

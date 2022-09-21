#include "SsrLightPass.h"
#include "Mesh.h"

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			TraceResult, // register(t0)
			CBuffer, // register(b0)
			NumRootParameters
		};

		struct ParametersCBuffer
		{
			DirectX::XMFLOAT2 TexelSize;
			float Separation;
			float Size;
		};
	};
}

SsrLightPass::SsrLightPass(Format renderTargetFormat)
	: m_RenderTargetFormat(renderTargetFormat)
	, m_RootParameters(RootParameters::NumRootParameters)
	, m_SourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0)
{
	m_RootParameters[RootParameters::TraceResult].InitAsDescriptorTable(1, &m_SourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootParameters[RootParameters::CBuffer].InitAsConstants(sizeof(RootParameters::ParametersCBuffer) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
}

void SsrLightPass::Execute(CommandList& commandList, const Texture& traceResult, const RenderTarget& renderTarget) const
{
	SetContext(commandList);
	SetRenderTarget(commandList, renderTarget);

	commandList.SetShaderResourceView(RootParameters::TraceResult, 0, traceResult, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	const auto traceResultDesc = traceResult.GetD3D12ResourceDesc();

	RootParameters::ParametersCBuffer cbuffer{};
	cbuffer.TexelSize = {
		1.0f / static_cast<float>(traceResultDesc.Width),
		1.0f / static_cast<float>(traceResultDesc.Height)
	};
	cbuffer.Size = 0;
	cbuffer.Separation = 1.0f;
	commandList.SetGraphics32BitConstants(RootParameters::CBuffer, cbuffer);

	m_BlitMesh->Draw(commandList);
}

std::wstring SsrLightPass::GetVertexShaderName() const
{
	return L"Blit_VertexShader.cso";
}

std::wstring SsrLightPass::GetPixelShaderName() const
{
	return L"SSR_LightPass_PixelShader.cso";
}

std::vector<Shader::RootParameter> SsrLightPass::GetRootParameters() const
{
	return m_RootParameters;
}

std::vector<Shader::StaticSampler> SsrLightPass::GetStaticSamplers() const
{
	return
	{
		StaticSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP)
	};
}

Shader::Format SsrLightPass::GetRenderTargetFormat() const
{
	return m_RenderTargetFormat;
}

void SsrLightPass::OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	m_BlitMesh = Mesh::CreateBlitTriangle(commandList);
}

Shader::BlendMode SsrLightPass::GetBlendMode() const
{
	return AdditiveBlend();
}

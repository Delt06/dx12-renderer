#include <SSR/SsrTrace.h>
#include <Framework/Mesh.h>
#include <Framework/Blit_VS.h>

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			CBuffer, // register(b0)
			Source, // scene color, normals, depth : register(t0-t3)
			NumRootParameters
		};
	};

	struct ParametersCBuffer
	{
		DirectX::XMMATRIX InverseProjection;
		DirectX::XMMATRIX InverseView;
		DirectX::XMMATRIX View;
		DirectX::XMMATRIX Projection;

		uint32_t Steps;
		float Thickness;
		DirectX::XMFLOAT2 JitterOffset;

		DirectX::XMFLOAT2 TexelSize;
		float _Padding[2];
	};
}

SsrTrace::SsrTrace(Format renderTargetFormat)
	: m_RenderTargetFormat(renderTargetFormat)
	, m_RootParameters(RootParameters::NumRootParameters)
	, m_SourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0)
	, m_PixelShader(LoadShaderFromFile(L"SSR_Trace_PS.cso"))
{
	m_RootParameters[RootParameters::CBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootParameters[RootParameters::Source].InitAsDescriptorTable(1, &m_SourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
}

void SsrTrace::Execute(CommandList& commandList, const Texture& sceneColor, const Texture& normals, const Texture& surface, const Texture& depth, const RenderTarget& renderTarget, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix) const
{
	SetContext(commandList);
	SetRenderTarget(commandList, renderTarget);

	ParametersCBuffer cbuffer;

	cbuffer.InverseProjection = XMMatrixInverse(nullptr, projectionMatrix);
	cbuffer.InverseView = XMMatrixInverse(nullptr, viewMatrix);
	cbuffer.View = viewMatrix;
	cbuffer.Projection = projectionMatrix;

	cbuffer.Steps = 5u;
	cbuffer.Thickness = 0.5f;
	cbuffer.JitterOffset = m_JitterOffset;

	const auto colorDesc = sceneColor.GetD3D12ResourceDesc();
	cbuffer.TexelSize =
	{
		1.0f / static_cast<float>(colorDesc.Width),
		1.0f / static_cast<float>(colorDesc.Height),
	};

	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::CBuffer, cbuffer);

	commandList.SetShaderResourceView(RootParameters::Source, 0, sceneColor, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Source, 1, normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Source, 2, surface, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Source, 3, depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1, m_PDepthSrv);

	m_BlitMesh->Draw(commandList);
}

std::vector<CompositeEffect::RootParameter> SsrTrace::GetRootParameters() const
{
	return m_RootParameters;
}

std::vector<CompositeEffect::StaticSampler> SsrTrace::GetStaticSamplers() const
{
	return
	{
		StaticSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP)
	};
}

CompositeEffect::Format SsrTrace::GetRenderTargetFormat() const
{
	return m_RenderTargetFormat;
}

void SsrTrace::OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	m_BlitMesh = Mesh::CreateBlitTriangle(commandList);
}

CompositeEffect::ShaderBytecode SsrTrace::GetVertexShaderBytecode() const
{
	return ShaderBytecode(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS);
}

CompositeEffect::ShaderBytecode SsrTrace::GetPixelShaderBytecode() const
{
	return ShaderBytecode(m_PixelShader.Get());
}

void SsrTrace::SetDepthSrv(const D3D12_SHADER_RESOURCE_VIEW_DESC* pDepthSrv)
{
	m_PDepthSrv = pDepthSrv;
}

void SsrTrace::SetJitterOffset(DirectX::XMFLOAT2 jitterOffset)
{
	m_JitterOffset = jitterOffset;
}

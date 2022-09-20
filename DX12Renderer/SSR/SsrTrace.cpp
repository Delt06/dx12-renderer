#include "SsrTrace.h"
#include "Mesh.h"

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			CBuffer, // register(b0)
			Source, // scene color, normals, depth : register(t0-t2)
			NumRootParameters
		};
	};

	struct ParametersCBuffer
	{
		DirectX::XMMATRIX InverseProjection;
		DirectX::XMMATRIX InverseView;
		DirectX::XMMATRIX View;
		DirectX::XMMATRIX Projection;

		float MaxDistance;
		float Resolution;
		uint32_t Steps;
		float Thickness;

		DirectX::XMFLOAT2 TexelSize;
		float _Padding[2];
	};
}

SsrTrace::SsrTrace(Format renderTargetFormat)
	: m_RenderTargetFormat(renderTargetFormat)
	, m_RootParameters(RootParameters::NumRootParameters)
	, m_SourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0)
{
	m_RootParameters[RootParameters::CBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootParameters[RootParameters::Source].InitAsDescriptorTable(1, &m_SourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
}

void SsrTrace::Execute(CommandList& commandList, const Texture& sceneColor, const Texture& normals, const Texture& depth, const RenderTarget& renderTarget, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix) const
{
	SetContext(commandList);
	SetRenderTarget(commandList, renderTarget);

	ParametersCBuffer cbuffer;

	cbuffer.InverseProjection = XMMatrixInverse(nullptr, projectionMatrix);
	cbuffer.InverseView = XMMatrixInverse(nullptr, viewMatrix);
	cbuffer.View = viewMatrix;
	cbuffer.Projection = projectionMatrix;

	cbuffer.MaxDistance = 8.0f;
	cbuffer.Resolution = 0.3f;
	cbuffer.Steps = 5u;
	cbuffer.Thickness = 0.5f;

	const auto colorDesc = sceneColor.GetD3D12ResourceDesc();
	cbuffer.TexelSize = 
	{
		1.0f / static_cast<float>(colorDesc.Width),
		1.0f / static_cast<float>(colorDesc.Height),
	};

	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::CBuffer, cbuffer);

	commandList.SetShaderResourceView(RootParameters::Source, 0, sceneColor, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Source, 1, normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Source, 2, depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1, m_PDepthSrv);

	m_BlitMesh->Draw(commandList);
}

std::wstring SsrTrace::GetVertexShaderName() const
{
	return L"Blit_VertexShader.cso";
}

std::wstring SsrTrace::GetPixelShaderName() const
{
	return L"SSR_Trace_PixelShader.cso";
}

std::vector<Shader::RootParameter> SsrTrace::GetRootParameters() const
{
	return m_RootParameters;
}

std::vector<Shader::StaticSampler> SsrTrace::GetStaticSamplers() const
{
	return 
	{
		StaticSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP) 
	};
}

Shader::Format SsrTrace::GetRenderTargetFormat() const
{
	return m_RenderTargetFormat;
}

void SsrTrace::OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	m_BlitMesh = Mesh::CreateBlitTriangle(commandList);
}

void SsrTrace::SetDepthSrv(const D3D12_SHADER_RESOURCE_VIEW_DESC* pDepthSrv)
{
	m_PDepthSrv = pDepthSrv;
}

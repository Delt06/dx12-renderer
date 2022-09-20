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
		DirectX::XMMATRIX ViewProjection;
		float MaxDistance;
		float Resolution;
		uint32_t Steps;
		float Thickness;
	};
}

SsrTrace::SsrTrace(Format renderTargetFormat)
	: m_RenderTargetFormat(renderTargetFormat)
{

}

void SsrTrace::Execute(CommandList& commandList, const Texture& sceneColor, const Texture& normals, const Texture& depth, const RenderTarget& renderTarget, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix) const
{
	SetContext(commandList);
	SetRenderTarget(commandList, renderTarget);

	ParametersCBuffer cbuffer;
	cbuffer.InverseProjection = XMMatrixInverse(nullptr, projectionMatrix);
	cbuffer.InverseView = XMMatrixInverse(nullptr, viewMatrix);
	cbuffer.View = viewMatrix;
	cbuffer.ViewProjection = viewMatrix * projectionMatrix;
	cbuffer.MaxDistance = 15.0f;
	cbuffer.Resolution = 0.3f;
	cbuffer.Steps = 10u;
	cbuffer.Thickness = 0.5f;
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
	std::vector<RootParameter> rootParameters(RootParameters::NumRootParameters);
	DescriptorRange sourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
	rootParameters[RootParameters::CBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);;
	rootParameters[RootParameters::Source].InitAsDescriptorTable(1, &sourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	return rootParameters;
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

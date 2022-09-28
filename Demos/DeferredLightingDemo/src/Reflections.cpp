#include "Reflections.h"
#include <Framework/Mesh.h>
#include <DX12Library/Helpers.h>
#include <Framework/Blit_VS.h>

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			MatricesCB, // register(t0)
			GBuffer, // register(t0-t3)
			Reflections, // register(t4-t6): pre-filter, brdfLUT, SSR
			NumRootParameters
		};
	}
}

Reflections::Reflections(Shader::Format renderTargetFormat)
	: m_RenderTargetFormat(renderTargetFormat)
	, m_RootParameters(RootParameters::NumRootParameters)
	, m_BlitMesh(nullptr)
	, m_PreFilterSrvDesc()
	, m_DepthSrvDesc()
	, m_PixelShader(LoadShaderFromFile(L"DeferredLightingDemo_LightBuffer_Reflections_PS.cso"))
{
	const UINT gBufferTexturesCount = 4;
	m_GBufferDescriptorRange = Shader::DescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, gBufferTexturesCount, 0);
	m_ReflectionsDescriptorRange = Shader::DescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, gBufferTexturesCount);

	m_RootParameters[RootParameters::MatricesCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootParameters[RootParameters::GBuffer].InitAsDescriptorTable(1, &m_GBufferDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootParameters[RootParameters::Reflections].InitAsDescriptorTable(1, &m_ReflectionsDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
}

void Reflections::SetSrvDescriptors(const D3D12_SHADER_RESOURCE_VIEW_DESC& preFilterSrvDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& depthSrvDesc)
{
	m_PreFilterSrvDesc = preFilterSrvDesc;
	m_DepthSrvDesc = depthSrvDesc;
}

void Reflections::Draw(CommandList& commandList, const RenderTarget& gBufferRenderTarget, const Texture& depthTexture, const Texture& preFilterMap, const Texture& brdfLut, const Texture& ssrTexture, const MatricesCb& matrices)
{
	PIXScope(commandList, "Reflections Light Pass");

	SetContext(commandList);

	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);

	commandList.SetShaderResourceView(RootParameters::GBuffer, 0, gBufferRenderTarget.GetTexture(Color0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::GBuffer, 1, gBufferRenderTarget.GetTexture(Color1), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::GBuffer, 2, gBufferRenderTarget.GetTexture(Color2), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::GBuffer, 3, depthTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1, &m_DepthSrvDesc);

	commandList.SetShaderResourceView(RootParameters::Reflections, 0, preFilterMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, UINT_MAX, &m_PreFilterSrvDesc);
	commandList.SetShaderResourceView(RootParameters::Reflections, 1, brdfLut, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Reflections, 2, ssrTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	m_BlitMesh->Draw(commandList);
}

std::vector<Shader::RootParameter> Reflections::GetRootParameters() const
{
	return m_RootParameters;
}

std::vector<Shader::StaticSampler> Reflections::GetStaticSamplers() const
{
	auto gBufferSampler = Shader::StaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	gBufferSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	auto skyboxSampler = Shader::StaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	skyboxSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	return { skyboxSampler, gBufferSampler };
}

Shader::Format Reflections::GetRenderTargetFormat() const
{
	return m_RenderTargetFormat;
}

Shader::BlendMode Reflections::GetBlendMode() const
{
	return AdditiveBlend();
}

void Reflections::OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	m_BlitMesh = Mesh::CreateBlitTriangle(commandList);
}

Shader::ShaderBytecode Reflections::GetVertexShaderBytecode() const
{
	return ShaderBytecode(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS);
}

Shader::ShaderBytecode Reflections::GetPixelShaderBytecode() const
{
	return ShaderBytecode(m_PixelShader.Get());
}

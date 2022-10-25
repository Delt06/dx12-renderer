#include "Reflections.h"
#include <Framework/Mesh.h>
#include <DX12Library/Helpers.h>
#include <Framework/Blit_VS.h>
#include <Framework/ShaderBlob.h>

Reflections::Reflections(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
	auto shader = std::make_shared<Shader>(rootSignature,
		ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
		ShaderBlob(L"DeferredLightingDemo_LightBuffer_Reflections_PS.cso"),
		[](PipelineStateBuilder& builder)
		{
			builder
				.WithAdditiveBlend()
				.WithDisabledDepthStencil()
				;
		});
	m_Material = Material::Create(shader);
}

void Reflections::SetPreFilterEnvironmentSrvDesc(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc)
{
	m_PreFilterEnvironmentSrvDesc = srvDesc;
}

void Reflections::Draw(CommandList& commandList, const std::shared_ptr<Texture>& preFilterMap, const std::shared_ptr<Texture>& brdfLut, const std::shared_ptr<Texture>& ssrTexture)
{
	PIXScope(commandList, "Reflections Light Pass");

	auto preFilterMapSrv = ShaderResourceView(preFilterMap, m_PreFilterEnvironmentSrvDesc);
	m_Material->SetShaderResourceView("preFilterMap", preFilterMapSrv);
	m_Material->SetShaderResourceView("brdfLut", ShaderResourceView(brdfLut));
	m_Material->SetShaderResourceView("ssrTexture", ShaderResourceView(ssrTexture));

	m_Material->Bind(commandList);
	m_BlitMesh->Draw(commandList);
}

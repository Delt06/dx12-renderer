#pragma once
#include <Framework/Mesh.h>
#include <Framework/CommonRootSignature.h>
#include <Framework/ShaderResourceView.h>
#include <Framework/Material.h>
#include <DX12Library/CommandList.h>
#include <memory>

class Reflections
{
public:
	explicit Reflections(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void SetPreFilterEnvironmentSrvDesc(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);
	void Draw(CommandList& commandList, const std::shared_ptr<Texture>& preFilterMap, const std::shared_ptr<Texture>& brdfLut, const std::shared_ptr<Texture>& ssrTexture);

protected:

private:
	std::shared_ptr<Mesh> m_BlitMesh;
	std::shared_ptr<Material> m_Material;

	D3D12_SHADER_RESOURCE_VIEW_DESC m_PreFilterEnvironmentSrvDesc{};
};


#pragma once
#include <DX12Library/RootSignature.h>
#include <Framework/Material.h>
#include <wrl.h>

class Mesh;
class CommandList;
class RenderTarget;
class Texture;

class DiffuseIrradiance
{
public:
	DiffuseIrradiance(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void SetSourceCubemap(CommandList& commandList, const std::shared_ptr<Texture>& texture);
	void SetRenderTarget(CommandList& commandList, RenderTarget& renderTarget, UINT texArrayIndex = -1);
	void Draw(CommandList& commandList, uint32_t cubemapSideIndex);

private:
	std::shared_ptr<Mesh> m_BlitMesh;
	std::shared_ptr<Material> m_Material;

	D3D12_SHADER_RESOURCE_VIEW_DESC m_SourceSrvDesc{};
};


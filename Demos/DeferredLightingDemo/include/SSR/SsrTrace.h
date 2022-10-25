#pragma once
#include <DirectXMath.h>
#include <Framework/Material.h>
#include <Framework/CommonRootSignature.h>
#include <DX12Library/CommandList.h>
#include <memory>

class Mesh;

class SsrTrace
{
public:
	explicit SsrTrace(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void Execute(CommandList& commandList, const std::shared_ptr<Texture>& sceneColor, const RenderTarget& renderTarget) const;


private:
	std::shared_ptr<Mesh> m_BlitMesh;
	std::shared_ptr<Material> m_Material;
};


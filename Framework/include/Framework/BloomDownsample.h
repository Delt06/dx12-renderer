#pragma once

#include <memory>
#include <vector>
#include <Framework/BloomParameters.h>
#include <DX12Library/Texture.h>
#include <DX12Library/RenderTarget.h>
#include <DX12Library/CommandList.h>
#include <Framework/Material.h>

class Mesh;

class BloomDownsample
{
public:
	explicit BloomDownsample(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void Execute(CommandList& commandList,
		const BloomParameters& parameters,
		const std::shared_ptr<Texture>& source,
		const RenderTarget& destination);

private:
	std::shared_ptr<Mesh> m_BlitMesh;
	std::shared_ptr<Material> m_Material;
};
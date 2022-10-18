#pragma once

#include "BloomParameters.h"
#include <memory>
#include <Framework/Material.h>

class Mesh;

class BloomUpsample
{
public:
	explicit BloomUpsample(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void Execute(CommandList& commandList,
		const BloomParameters& parameters,
		const std::shared_ptr<Texture>& source,
		const RenderTarget& destination);

private:
	std::shared_ptr<Material> m_Material;
	std::shared_ptr<Mesh> m_BlitMesh;
};

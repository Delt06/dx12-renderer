#include "Model.h"
#include <CommandList.h>
#include <Mesh.h>
#include <Texture.h>

Model::Model(const MeshCollectionType& meshes)
	: m_Meshes(meshes)

{
}

Model::Model(std::shared_ptr<Mesh> mesh)
	: m_Meshes({mesh})
{
}


void Model::SetMap(const ModelMaps::MapType mapType, const std::shared_ptr<Texture> map)
{
	m_Maps[mapType] = map;
	m_HasMaps[mapType] = true;
}

Model::~Model() = default;


void Model::Draw(const uint32_t mapsRootParameterIndex, CommandList& commandList) const
{
	for (uint32_t i = 0; i < ModelMaps::TotalNumber; ++i)
	{
		const uint32_t descriptorOffset = i;
		constexpr auto stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (m_HasMaps[i])
		{
			const auto& map = m_Maps[i];
			commandList.SetShaderResourceView(mapsRootParameterIndex, descriptorOffset, *map, stateAfter);
		}
	}

	for (const auto& mesh : m_Meshes)
	{
		mesh->Draw(commandList);
	}
}

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
	switch (mapType)
	{
	case ModelMaps::Diffuse:
		m_Material.HasDiffuseMap = true;
		break;
	case ModelMaps::Normal:
		m_Material.HasNormalMap = true;
		break;
	case ModelMaps::Specular:
		m_Material.HasSpecularMap = true;
		break;
	case ModelMaps::Gloss:
		m_Material.HasGlossMap = true;
		break;
	default:
		throw std::exception("Invalid map type.");
	}

	m_Maps[mapType] = map;
}

void Model::SetMapsEmpty(const std::shared_ptr<Texture> emptyMap)
{
	m_Material.HasDiffuseMap = false;
	m_Material.HasNormalMap = false;
	m_Material.HasSpecularMap = false;
	m_Material.HasGlossMap = false;

	for (uint32_t i = 0; i < ModelMaps::TotalNumber; ++i)
	{
		m_Maps[i] = emptyMap;
	}
}

Model::~Model() = default;


void Model::Draw(CommandList& commandList, const uint32_t materialRootParameterIndex,
                 const uint32_t mapsRootParameterIndex) const
{
	commandList.SetGraphicsDynamicConstantBuffer(materialRootParameterIndex, m_Material);

	for (uint32_t i = 0; i < ModelMaps::TotalNumber; ++i)
	{
		const uint32_t descriptorOffset = i;
		constexpr auto stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		const auto& map = m_Maps[i];
		commandList.SetShaderResourceView(mapsRootParameterIndex, descriptorOffset, *map, stateAfter);
	}

	for (const auto& mesh : m_Meshes)
	{
		mesh->Draw(commandList);
	}
}

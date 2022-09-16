#pragma once

#include <memory>
#include <vector>
#include <Material.h>
#include <cstdint>

#include "DescriptorAllocation.h"


class Mesh;
class Texture;
class CommandList;
class Material;

namespace ModelMaps
{
	enum MapType
	{
		Diffuse = 0,
		Normal,
		Specular,
		Gloss,
		TotalNumber,
	};
}

class Model
{
public:
	using MeshCollectionType = std::vector<std::shared_ptr<Mesh>>;

	explicit Model(const MeshCollectionType& meshes);
	explicit Model(std::shared_ptr<Mesh> mesh);

	virtual ~Model();

	void Draw(CommandList& commandList, uint32_t materialRootParameterIndex, uint32_t mapsRootParameterIndex) const;

	void SetMap(ModelMaps::MapType mapType, std::shared_ptr<Texture> map);
	void SetMapsEmpty(std::shared_ptr<Texture> emptyMap);
	Material& GetMaterial();
	const Material& GetMaterial() const;

	const MeshCollectionType& GetMeshes() const;
	void GetMaps(std::shared_ptr<Texture> mapsDestination[ModelMaps::TotalNumber]) const;
private:
	MeshCollectionType m_Meshes{};

	Material m_Material{};
	std::shared_ptr<Texture> m_Maps[ModelMaps::TotalNumber];
};

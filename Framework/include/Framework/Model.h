#pragma once

#include <memory>
#include <vector>
#include <cstdint>

#include <DX12Library/DescriptorAllocation.h>


class Mesh;
class Texture;
class CommandList;
class Material;

class Model
{
public:
	using MeshCollectionType = std::vector<std::shared_ptr<Mesh>>;

	explicit Model(const MeshCollectionType& meshes);
	explicit Model(std::shared_ptr<Mesh> mesh);

	virtual ~Model();

	void Draw(CommandList& commandList) const;

	const MeshCollectionType& GetMeshes() const;

	const std::shared_ptr<Material>& GetMaterial() const { return m_Material; }
	void SetMaterial(const std::shared_ptr<Material>& material) { m_Material = material; }
private:
	MeshCollectionType m_Meshes{};
	std::shared_ptr<Material> m_Material = nullptr;
};

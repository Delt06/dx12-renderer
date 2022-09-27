#pragma once

#include <memory>
#include <vector>
#include <cstdint>

#include <DX12Library/DescriptorAllocation.h>


class Mesh;
class Texture;
class CommandList;

class Model
{
public:
	using MeshCollectionType = std::vector<std::shared_ptr<Mesh>>;

	explicit Model(const MeshCollectionType& meshes);
	explicit Model(std::shared_ptr<Mesh> mesh);

	virtual ~Model();

	void Draw(CommandList& commandList) const;

	const MeshCollectionType& GetMeshes() const;
private:
	MeshCollectionType m_Meshes{};
};

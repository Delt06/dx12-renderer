#include <Model.h>
#include <Framework/Mesh.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Texture.h>

Model::Model(const MeshCollectionType& meshes)
	: m_Meshes(meshes)

{
}

Model::Model(std::shared_ptr<Mesh> mesh)
	: m_Meshes({ mesh })
{
}

const Model::MeshCollectionType& Model::GetMeshes() const
{
	return m_Meshes;
}

Model::~Model() = default;

void Model::Draw(CommandList& commandList) const
{
	for (const auto& mesh : m_Meshes)
	{
		mesh->Draw(commandList);
	}
}
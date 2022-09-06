#include "ModelLoader.h"
#include "Mesh.h"
#include <OBJ_Loader.h>

namespace
{
	union ConvertibleVertex
	{
		objl::Vertex m_Vertex;
		VertexAttributes m_VertexAttributes;
	};
}

std::vector<std::unique_ptr<Mesh>> ModelLoader::LoadObj(CommandList& commandList, const std::string& path, const bool rhCoords)
{
	objl::Loader loader;
	if (!loader.LoadFile(path))
	{
		throw std::exception("Model was not loaded. Probably, it does not exist.");
	}

	const std::vector<objl::Mesh>& loadedMeshes = loader.LoadedMeshes;
	std::vector<std::unique_ptr<Mesh>> outputMeshes;
	outputMeshes.reserve(loadedMeshes.size());

	auto toVertexAttribute = [](const objl::Vertex& vertex)
	{
		ConvertibleVertex convertibleVertex = {};
		convertibleVertex.m_Vertex = vertex;
		return convertibleVertex.m_VertexAttributes;
	};

	for (const auto& objMesh : loadedMeshes)
	{
		VertexCollectionType outputVertices;
		outputVertices.reserve(objMesh.Vertices.size());

		for (const auto& vertex : objMesh.Vertices)
		{
			outputVertices.push_back(toVertexAttribute(vertex));
		}

		IndexCollectionType outputIndices;
		outputIndices.reserve(objMesh.Indices.size());

		for (const auto index  : objMesh.Indices)
		{
			outputIndices.push_back(static_cast<uint16_t>(index));
		}

		outputMeshes.push_back(Mesh::CreateMesh(commandList, outputVertices, outputIndices, rhCoords));
	}

	return outputMeshes;
}

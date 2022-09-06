#include "ModelLoader.h"
#include "Mesh.h"
#include <../DirectXMesh/Utilities/WaveFrontReader.h>
#include <Helpers.h>

#include "Model.h"
#include "Texture.h"
#include "DirectXMesh/DirectXMesh.h"

using namespace DirectX;

ModelLoader::ModelLoader(const std::shared_ptr<Texture> emptyTexture2d) :
	m_EmptyTexture2d(emptyTexture2d)
{
}


std::shared_ptr<Model> ModelLoader::LoadObj(CommandList& commandList, const std::wstring& path,
                                            const bool rhCoords)
{
	const auto mesh = std::make_unique<WaveFrontReader<uint16_t>>();
	ThrowIfFailed(mesh->Load(path.c_str()));

	const size_t nIndices = mesh->indices.size();
	const size_t nFaces = nIndices / 3;
	const size_t nVerts = mesh->vertices.size();


	auto pos = std::make_unique<XMFLOAT3[]>(nVerts);
	for (size_t j = 0; j < nVerts; ++j)
		pos[j] = mesh->vertices[j].position;

	auto normals = std::make_unique<XMFLOAT3[]>(nVerts);

	if (mesh->hasNormals)
	{
		for (size_t j = 0; j < nVerts; ++j)
			normals[j] = mesh->vertices[j].normal;
	}
	else
	{
		ThrowIfFailed(ComputeNormals(mesh->indices.data(), nFaces,
		                             pos.get(), nVerts, CNORM_DEFAULT, normals.get()));
	}

	std::unique_ptr<XMFLOAT2[]> texcoords;
	std::unique_ptr<XMFLOAT3[]> tangents;
	std::unique_ptr<XMFLOAT3[]> bitangents;

	if (mesh->hasTexcoords)
	{
		texcoords = std::make_unique<XMFLOAT2[]>(nVerts);
		tangents = std::make_unique<XMFLOAT3[]>(nVerts);
		bitangents = std::make_unique<XMFLOAT3[]>(nVerts);

		for (size_t j = 0; j < nVerts; ++j)
			texcoords[j] = mesh->vertices[j].textureCoordinate;

		ThrowIfFailed(ComputeTangentFrame(mesh->indices.data(), nFaces,
		                                  pos.get(), normals.get(), texcoords.get(), nVerts,
		                                  tangents.get(), bitangents.get()));
	}


	VertexCollectionType outputVertices;
	outputVertices.reserve(nVerts);

	for (uint16_t i = 0; i < nVerts; ++i)
	{
		VertexAttributes vertexAttributes;
		if (mesh->hasTexcoords)
			vertexAttributes = VertexAttributes(pos[i], normals[i], texcoords[i], tangents[i], bitangents[i]);
		else
			vertexAttributes = VertexAttributes(pos[i], normals[i], {}, {}, {});
		outputVertices.push_back(vertexAttributes);
	}

	IndexCollectionType outputIndices;
	outputVertices.reserve(nIndices);

	for (uint16_t i = 0; i < nIndices; ++i)
	{
		outputIndices.push_back(mesh->indices[i]);
	}

	std::vector<std::shared_ptr<Mesh>> outputMeshes;
	outputMeshes.push_back(Mesh::CreateMesh(commandList, outputVertices, outputIndices, rhCoords, false));
	auto model = std::make_shared<Model>(outputMeshes);

	for (uint32_t i = 0; i < ModelMaps::TotalNumber; ++i)
	{
		model->SetMap(static_cast<ModelMaps::MapType>(i), m_EmptyTexture2d);
	}

	return model;
}

void ModelLoader::LoadMap(Model& model, CommandList& commandList, ModelMaps::MapType mapType,
                          const std::wstring& path) const
{
	const auto map = std::make_shared<Texture>();
	TextureUsageType textureUsage;
	switch (mapType)
	{
	case ModelMaps::Diffuse: textureUsage = TextureUsageType::Albedo;
		break;
	case ModelMaps::Normal: textureUsage = TextureUsageType::Normalmap;
		break;
	case ModelMaps::Specular: textureUsage = TextureUsageType::Other;
		break;
	case ModelMaps::Gloss: textureUsage = TextureUsageType::Other;
		break;
	default:
		throw std::exception("Invalid map type.");
	}
	commandList.LoadTextureFromFile(*map, path, textureUsage);
	model.SetMap(mapType, map);
}

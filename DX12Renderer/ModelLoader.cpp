#include "ModelLoader.h"
#include "Mesh.h"
#include <../DirectXMesh/Utilities/WaveFrontReader.h>
#include <Helpers.h>

#include "Model.h"
#include "Texture.h"
#include "DirectXMesh/DirectXMesh.h"

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

using namespace DirectX;

namespace
{
	XMFLOAT3 ToXMFloat3(aiVector3D vector)
	{
		return { vector.x, vector.y, vector.z };
	}

	XMFLOAT2 ToXMFloat2(aiVector3D vector)
	{
		return { vector.x, vector.y };
	}
}

ModelLoader::ModelLoader(const std::shared_ptr<Texture> emptyTexture2d) :
	m_EmptyTexture2d(emptyTexture2d)
{
}

std::shared_ptr<Model> ModelLoader::Load(CommandList& commandList, const std::string& path) const
{
	Assimp::Importer importer;

	auto flags =
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType |
		aiProcess_GenSmoothNormals |
		aiProcess_ConvertToLeftHanded
		;

	const aiScene* scene = importer.ReadFile(path.c_str(), flags);

	if (scene == nullptr)
	{
		throw std::exception(importer.GetErrorString());
	}

	std::vector<std::shared_ptr<Mesh>> outputMeshes;


	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		auto mesh = scene->mMeshes[meshIndex];

		VertexCollectionType outputVertices;
		outputVertices.reserve(mesh->mNumVertices);

		for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
		{
			VertexAttributes vertexAttributes;

			if (mesh->HasPositions())
			{
				vertexAttributes.Position = ToXMFloat3(mesh->mVertices[vertexIndex]);
			}

			if (mesh->HasNormals())
			{
				vertexAttributes.Normal = ToXMFloat3(mesh->mNormals[vertexIndex]);
			}

			constexpr unsigned int uvIndex = 0;
			if (mesh->HasTextureCoords(uvIndex))
			{
				vertexAttributes.Uv = ToXMFloat2(mesh->mTextureCoords[uvIndex][vertexIndex]);
			}

			if (mesh->HasTangentsAndBitangents())
			{
				vertexAttributes.Tangent = ToXMFloat3(mesh->mTangents[vertexIndex]);
				vertexAttributes.Bitangent = ToXMFloat3(mesh->mBitangents[vertexIndex]);
			}

			outputVertices.push_back(vertexAttributes);
		}

		IndexCollectionType outputIndices;
		constexpr unsigned int indicesInTriangle = 3;
		outputVertices.reserve(mesh->mNumFaces * indicesInTriangle);

		for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			const auto face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == indicesInTriangle);

			outputIndices.push_back(face.mIndices[0]);
			outputIndices.push_back(face.mIndices[1]);
			outputIndices.push_back(face.mIndices[2]);
		}

		outputMeshes.push_back(Mesh::CreateMesh(commandList, outputVertices, outputIndices, true, false));
	}

	auto model = std::make_shared<Model>(outputMeshes);
	model->SetMapsEmpty(m_EmptyTexture2d);
	return model;
}

std::shared_ptr<Model> ModelLoader::LoadExisting(std::shared_ptr<Mesh> mesh) const
{
	auto model = std::make_shared<Model>(mesh);
	model->SetMapsEmpty(m_EmptyTexture2d);
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

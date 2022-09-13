#include "ModelLoader.h"
#include "Mesh.h"
#include <../DirectXMesh/Utilities/WaveFrontReader.h>
#include <Helpers.h>

#include "Model.h"
#include "Texture.h"
#include "Bone.h"
#include "DirectXMesh/DirectXMesh.h"

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <filesystem>

using namespace DirectX;
namespace fs = std::filesystem;

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

	XMMATRIX ToXMATRIX(aiMatrix4x4 matrix)
	{
		// transpose to convert to the left-handed orientation
		return XMMATRIX(
			matrix.a1, matrix.b1, matrix.c1, matrix.d1,
			matrix.a2, matrix.b2, matrix.c2, matrix.d2,
			matrix.a3, matrix.b3, matrix.c3, matrix.d3,
			matrix.a4, matrix.b4, matrix.c4, matrix.d4
		);
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
		std::string errorString = importer.GetErrorString();
		throw std::exception(errorString.c_str());
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


		auto outputMesh = Mesh::CreateMesh(commandList, outputVertices, outputIndices, true, false);

		if (mesh->HasBones())
		{
			std::vector<Bone> bones;
			bones.reserve(mesh->mNumBones);

			for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
			{
				Bone bone;

				auto meshBone = mesh->mBones[boneIndex];
				bone.Name = meshBone->mName.C_Str();
				bone.Transform = XMMatrixInverse(nullptr, ToXMATRIX(meshBone->mOffsetMatrix));

				bones.push_back(bone);
			}

			outputMesh->SetBones(bones);
		}

		outputMeshes.push_back(outputMesh);
	}

	auto model = std::make_shared<Model>(outputMeshes);
	model->SetMapsEmpty(m_EmptyTexture2d);

	if (outputMeshes.size() > 0 && scene->HasMaterials())
	{
		for (int i = 0; i < ModelMaps::TotalNumber; i++)
		{
			auto mapType = (ModelMaps::MapType)i;
			aiTextureType textureType;
			switch (mapType)
			{
			case ModelMaps::Diffuse:
				textureType = aiTextureType_DIFFUSE;
				break;
			case ModelMaps::Normal:
				textureType = aiTextureType_NORMALS;
				break;
			case ModelMaps::Specular:
				textureType = aiTextureType_SPECULAR;
				break;
			case ModelMaps::Gloss:
				textureType = aiTextureType_DIFFUSE_ROUGHNESS;
				break;
			}

			auto materialIndex = scene->mMeshes[0]->mMaterialIndex;
			auto material = scene->mMaterials[materialIndex];
			aiString texturePath;
			material->GetTexture(textureType, 0, &texturePath);
			if (texturePath.length > 0)
			{
				auto texture = scene->GetEmbeddedTexture(texturePath.C_Str());
				if (texture != nullptr)
				{
					throw std::exception("Embedded textures are not yet supported. Try extracting them.");
				}

				fs::path fsTexturePath(path);
				fs::path fsTexturePathLocal(texturePath.C_Str());
				fsTexturePath = fsTexturePath.replace_filename(fsTexturePathLocal);
				LoadMap(*model, commandList, mapType, fsTexturePath.generic_wstring(), false);
			}
		}
	}

	return model;
}

std::shared_ptr<Model> ModelLoader::LoadExisting(std::shared_ptr<Mesh> mesh) const
{
	auto model = std::make_shared<Model>(mesh);
	model->SetMapsEmpty(m_EmptyTexture2d);
	return model;
}

void ModelLoader::LoadMap(Model& model, CommandList& commandList, ModelMaps::MapType mapType,
	const std::wstring& path, bool throwOnNotFound) const
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

	if (commandList.LoadTextureFromFile(*map, path, textureUsage, throwOnNotFound))
		model.SetMap(mapType, map);
}

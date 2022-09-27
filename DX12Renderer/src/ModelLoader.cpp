#include <ModelLoader.h>
#include <Mesh.h>
#include <DirectXMesh.h>
#include <DX12Library/Helpers.h>

#include <Model.h>
#include <Bone.h>
#include <Animation.h>
#include <DX12Library/DX12LibPCH.h>
#include <DX12Library/Texture.h>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <filesystem>
#include <memory>

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

	XMVECTOR ToXMVECTOR(aiVector3D vector)
	{
		return XMVectorSet(vector.x, vector.y, vector.z, 0.0);
	}

	XMVECTOR ToXMVECTOR(aiQuaternion quaternion)
	{
		return XMVectorSet(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
	}

	constexpr auto AI_FLAGS = aiProcess_ConvertToLeftHanded;

	template<typename TKey>
	void BuildKeyFrames(unsigned int numKeys, const TKey* keys, std::vector<Animation::KeyFrame>& result)
	{
		for (unsigned int keyIndex = 0; keyIndex < numKeys; ++keyIndex)
		{
			Animation::KeyFrame keyFrame;
			auto key = keys[keyIndex];
			keyFrame.Value = ToXMVECTOR(key.mValue);
			keyFrame.NormalizedTime = static_cast<float>(key.mTime);
			result.push_back(keyFrame);
		}
	}
}

std::shared_ptr<Model> ModelLoader::Load(CommandList& commandList, const std::string& path, bool flipNormals) const
{
	Assimp::Importer importer;

	auto flags = AI_FLAGS |
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType |
		aiProcess_GenSmoothNormals |
		aiProcess_PopulateArmatureData |
		aiProcess_LimitBoneWeights 
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
				vertexAttributes.Normal = ToXMFloat3(mesh->mNormals[vertexIndex] * (flipNormals ? -1.0f : 1.0f));
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

			SkinningVertexCollectionType outputSkinningVertices;
			outputSkinningVertices.resize(mesh->mNumVertices);

			for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
			{
				auto meshBone = mesh->mBones[boneIndex];

				Bone bone;
				bone.Offset = ToXMATRIX(meshBone->mOffsetMatrix);
				bone.Name = meshBone->mName.C_Str();
				bone.IsDirty = true;

				bones.push_back(bone);

				for (unsigned int weightIndex = 0; weightIndex < meshBone->mNumWeights; ++weightIndex)
				{
					auto& weight = meshBone->mWeights[weightIndex];
					auto& vertexAttributes = outputSkinningVertices[weight.mVertexId];

					// try put the weight into any of the available slots of the vertex
					for (uint32_t vertexAttributeId = 0; vertexAttributeId < SkinningVertexAttributes::BONES_PER_VERTEX; vertexAttributeId++)
					{
						// search for the first 0 weight and fill it
						if (vertexAttributes.Weights[vertexAttributeId] == 0.0)
						{
							vertexAttributes.BoneIds[vertexAttributeId] = boneIndex;
							vertexAttributes.Weights[vertexAttributeId] = weight.mWeight;
							break;
						}
					}
				}
			}

			for (auto& vertexAttributes : outputSkinningVertices)
			{
				vertexAttributes.NormalizeWeights();
			}

			outputMesh->SetBones(bones);

			for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
			{
				std::vector<size_t> childrenIndices;

				const auto meshBoneNode = mesh->mBones[boneIndex]->mNode;
				auto& bone = outputMesh->GetBone(boneIndex);
				bone.LocalTransform = ToXMATRIX(meshBoneNode->mTransformation);

				for (unsigned int childIndex = 0; childIndex < meshBoneNode->mNumChildren; ++childIndex)
				{
					const auto child = meshBoneNode->mChildren[childIndex];
					auto childName = std::string(child->mName.C_Str());
					if (!outputMesh->HasBone(childName))
					{
						continue;
					}

					size_t boneIndex = outputMesh->GetBoneIndex(childName);
					childrenIndices.push_back(boneIndex);
				}

				outputMesh->SetBoneChildren(boneIndex, childrenIndices);
			}

			outputMesh->SetSkinningVertexAttributes(commandList, outputSkinningVertices);
		}

		outputMeshes.push_back(outputMesh);
	}

	auto model = std::make_shared<Model>(outputMeshes);
	/*model->SetMapsEmpty(m_EmptyTexture2d);

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
	}*/

	return model;
}

std::shared_ptr<Model> ModelLoader::LoadExisting(std::shared_ptr<Mesh> mesh) const
{
	auto model = std::make_shared<Model>(mesh);
	return model;
}

std::shared_ptr<Animation> ModelLoader::LoadAnimation(const std::string& path, const std::string& animationName) const
{
	Assimp::Importer importer;

	auto flags = AI_FLAGS;

	const aiScene* scene = importer.ReadFile(path.c_str(), flags);

	if (scene == nullptr)
	{
		std::string errorString = importer.GetErrorString();
		throw std::exception(errorString.c_str());
	}

	if (!scene->HasAnimations())
	{
		throw std::exception("Specified file does not contain animations");
	}

	for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
	{
		auto animation = scene->mAnimations[i];
		if (strcmp(animation->mName.C_Str(), animationName.c_str()) != 0) continue;

		auto duration = static_cast<float>(animation->mDuration);
		auto ticksPerSecond = static_cast<float>(animation->mTicksPerSecond);

		std::vector<Animation::Channel> resultingChannels;

		for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
		{
			auto channel = animation->mChannels[channelIndex];

			Animation::Channel resultingChannel;
			resultingChannel.NodeName = channel->mNodeName.C_Str();

			BuildKeyFrames(channel->mNumPositionKeys, channel->mPositionKeys, resultingChannel.PositionKeyFrames);
			BuildKeyFrames(channel->mNumRotationKeys, channel->mRotationKeys, resultingChannel.RotationKeyFrames);
			BuildKeyFrames(channel->mNumScalingKeys, channel->mScalingKeys, resultingChannel.ScalingKeyFrames);

			resultingChannels.push_back(resultingChannel);
		}

		auto resultingAnimation = std::make_shared<Animation>(duration, ticksPerSecond, resultingChannels);
		return resultingAnimation;
	}

	throw std::exception("The requested animation was not found.");
}
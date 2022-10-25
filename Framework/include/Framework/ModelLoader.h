#pragma once
#include <string>

#include <DX12Library/CommandList.h>
#include <DX12Library/Texture.h>
#include "Model.h"
#include <memory>

class Mesh;
class Animation;

class ModelLoader
{
public:
	std::shared_ptr<Model> Load(CommandList& commandList, const std::string& path, bool flipNormals = false) const;
	std::shared_ptr<Model> LoadExisting(std::shared_ptr<Mesh> mesh) const;

	std::shared_ptr<Animation> LoadAnimation(const std::string& path, const std::string& animationName) const;

	std::shared_ptr<Texture> LoadTexture(CommandList& commandList, const std::wstring& path, TextureUsageType usage = TextureUsageType::Albedo) const;
};

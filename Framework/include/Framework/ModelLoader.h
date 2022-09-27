#pragma once
#include <string>

#include <DX12Library/CommandList.h>
#include <memory>

class Model;
class Mesh;
class Animation;

class ModelLoader
{
public:
	std::shared_ptr<Model> Load(CommandList& commandList, const std::string& path, bool flipNormals = false) const;
	std::shared_ptr<Model> LoadExisting(std::shared_ptr<Mesh> mesh) const;

	std::shared_ptr<Animation> LoadAnimation(const std::string& path, const std::string& animationName) const;
};

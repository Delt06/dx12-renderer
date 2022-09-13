#pragma once
#include <string>

#include "CommandList.h"

class Model;
class Mesh;
class Animation;

namespace ModelMaps
{
	enum MapType;
}


class ModelLoader
{
public:
	explicit ModelLoader(std::shared_ptr<Texture> emptyTexture2d);
	std::shared_ptr<Model> Load(CommandList& commandList, const std::string& path) const;
	std::shared_ptr<Model> LoadExisting(std::shared_ptr<Mesh> mesh) const;
	void LoadMap(Model& model, CommandList& commandList, ModelMaps::MapType mapType, const std::wstring& path, bool throwOnNotFound = true) const;

	std::shared_ptr<Animation> LoadAnimation(const std::string& path, const std::string& animationName) const;

private:
	std::shared_ptr<Texture> m_EmptyTexture2d;
};

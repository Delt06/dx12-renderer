#pragma once
#include <string>

#include "CommandList.h"

class Model;
class Mesh;

namespace ModelMaps
{
	enum MapType;
}


class ModelLoader
{
public:
	explicit ModelLoader(std::shared_ptr<Texture> emptyTexture2d);
	std::shared_ptr<Model> LoadObj(CommandList& commandList, const std::wstring& path, bool rhCoords = false) const;
	std::shared_ptr<Model> LoadExisting(std::shared_ptr<Mesh> mesh) const;
	void LoadMap(Model& model, CommandList& commandList, ModelMaps::MapType mapType, const std::wstring& path) const;

private:
	std::shared_ptr<Texture> m_EmptyTexture2d;
};

#pragma once
#include "Material.h"
#include <string>
#include <memory>

class CommandList;
class Texture;

class TextureLoader
{
public:
	explicit TextureLoader(std::shared_ptr<Texture> emptyTexture);

	void Init(Material& material);
	void Load(Material& material, CommandList& commandList, Material::MapType mapType,
		const std::wstring& path, bool throwOnNotFound = true) const;

private:
	std::shared_ptr<Texture> m_EmptyTexture;
};


#pragma once

#include <string>
#include "PBR/PbrMaterial.h"
#include <memory>

class CommandList;
class Texture;

class PbrTextureLoader
{
public:
	explicit PbrTextureLoader(std::shared_ptr<Texture> emptyTexture);

	void Init(PbrMaterial& material);
	void LoadMap(PbrMaterial& material, CommandList& commandList, PbrMaterial::MapType mapType,
		const std::wstring& path, bool throwOnNotFound = true) const;

private:
	std::shared_ptr<Texture> m_EmptyTexture;
};


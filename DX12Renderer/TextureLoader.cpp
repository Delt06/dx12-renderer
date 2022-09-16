#include "TextureLoader.h"
#include "CommandList.h"
#include <memory>

TextureLoader::TextureLoader(std::shared_ptr<Texture> emptyTexture)
	: m_EmptyTexture(emptyTexture)
{

}

void TextureLoader::Init(Material& material)
{
	material.SetMapsEmpty(m_EmptyTexture);
}

void TextureLoader::Load(Material& material, CommandList& commandList, Material::MapType mapType, const std::wstring& path, bool throwOnNotFound) const
{
	const auto map = std::make_shared<Texture>();
	TextureUsageType textureUsage;
	switch (mapType)
	{
	case Material::Diffuse:
		textureUsage = TextureUsageType::Albedo;
		break;
	case Material::Normal:
		textureUsage = TextureUsageType::Normalmap;
		break;
	case Material::Specular:
	case Material::Gloss:
		textureUsage = TextureUsageType::Other;
		break;
	default:
		throw std::exception("Invalid map type.");
	}

	if (commandList.LoadTextureFromFile(*map, path, textureUsage, throwOnNotFound))
		material.SetMap(mapType, map);
}

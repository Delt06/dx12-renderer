#include <PBR/PbrTextureLoader.h>
#include <CommandList.h>
#include <PBR/PbrMaterial.h>
#include <memory>
#include <Texture.h>

PbrTextureLoader::PbrTextureLoader(std::shared_ptr<Texture> emptyTexture)
	: m_EmptyTexture(emptyTexture)
{

}

void PbrTextureLoader::Init(PbrMaterial& material)
{
	material.SetMapsEmpty(m_EmptyTexture);
}

void PbrTextureLoader::LoadMap(PbrMaterial& material, CommandList& commandList, PbrMaterial::MapType mapType, const std::wstring& path, bool throwOnNotFound) const
{
	const auto map = std::make_shared<Texture>();
	TextureUsageType textureUsage;
	switch (mapType)
	{
	case PbrMaterial::Diffuse:
		textureUsage = TextureUsageType::Albedo;
		break;
	case PbrMaterial::Normal:
		textureUsage = TextureUsageType::Normalmap;
		break;
	case PbrMaterial::Metallic:
	case PbrMaterial::Roughness:
	case PbrMaterial::AmbientOcclusion:
		textureUsage = TextureUsageType::Other;
		break;
	default:
		throw std::exception("Invalid map type.");
	}

	if (commandList.LoadTextureFromFile(*map, path, textureUsage, throwOnNotFound))
		material.SetMap(mapType, map);
}

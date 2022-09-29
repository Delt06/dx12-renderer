#include <PhongLighting/Material.h>

Material::Constants& Material::GetConstants()
{
	return m_Constants;
}

void Material::SetDynamicConstantBuffer(CommandList& commandList, uint32_t rootParameterIndex)
{
	commandList.SetGraphicsDynamicConstantBuffer(rootParameterIndex, m_Constants);
}

void Material::SetShaderResourceViews(CommandList& commandList, uint32_t rootParameterIndex, uint32_t baseDescriptorOffset /*= 0*/)
{
	for (size_t i = 0; i < TotalNumMaps; ++i)
	{
		commandList.SetShaderResourceView(rootParameterIndex, baseDescriptorOffset + i, *m_Maps[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}

void Material::SetMapsEmpty(std::shared_ptr<Texture> emptyMap)
{
	for (auto& Map : m_Maps)
	{
		Map = emptyMap;
	}

	m_Constants.HasDiffuseMap = false;
	m_Constants.HasNormalMap = false;
	m_Constants.HasSpecularMap = false;
	m_Constants.HasGlossMap = false;
}

void Material::SetMap(MapType mapType, std::shared_ptr<Texture> map)
{
	m_Maps[mapType] = map;

	switch (mapType)
	{
	case Diffuse:
		m_Constants.HasDiffuseMap = true;
		break;
	case Normal:
		m_Constants.HasNormalMap = true;
		break;
	case Specular:
		m_Constants.HasSpecularMap = true;
		break;
	case Gloss:
		m_Constants.HasGlossMap = true;
		break;
	default:
		throw std::exception("Invalid map.");
	}
}

std::shared_ptr<Texture> Material::GetMap(MapType mapType) const
{
	return m_Maps[mapType];
}
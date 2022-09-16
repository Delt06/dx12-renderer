#include <PbrMaterial.h>


PbrMaterial::Constants& PbrMaterial::GetConstants()
{
	return m_Constants;
}

void PbrMaterial::SetDynamicConstantBuffer(CommandList& commandList, uint32_t rootParameterIndex)
{
	commandList.SetGraphicsDynamicConstantBuffer(rootParameterIndex, m_Constants);
}

void PbrMaterial::SetMapsEmpty(std::shared_ptr<Texture> emptyMap)
{
	for (auto & Map : m_Maps)
	{
		Map = emptyMap;
	}
	UpdateMapFlags();
}

void PbrMaterial::SetMap(MapType mapType, std::shared_ptr<Texture> map)
{
	m_Maps[mapType] = map;
	UpdateMapFlags();
}

void PbrMaterial::UpdateMapFlags()
{
	for (size_t i = 0; i < TotalNumMaps; ++i)
	{
		MapType mapType = static_cast<MapType>(i);
		bool hasMap = m_Maps[i] != nullptr;
		switch (mapType)
		{
		case Diffuse:
			m_Constants.HasDiffuseMap = hasMap;
			break;
		case Normal:
			m_Constants.HasNormalMap = hasMap;
			break;
		case Metallic:
			m_Constants.HasMetallicMap = hasMap;
			break;
		case Roughness:
			m_Constants.HasRoughnessMap = hasMap;
			break;
		default:
			throw std::exception("Invalid map.");
		}
	}
}

void PbrMaterial::SetShaderResourceViews(CommandList& commandList, uint32_t rootParameterIndex, uint32_t baseDescriptorOffset /*= 0*/)
{
	for (size_t i = 0; i < TotalNumMaps; ++i)
	{
		commandList.SetShaderResourceView(rootParameterIndex, baseDescriptorOffset + i, *m_Maps[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}

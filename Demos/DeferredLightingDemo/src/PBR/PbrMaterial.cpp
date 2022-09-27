#include <PBR/PbrMaterial.h>
#include <DX12Library/CommandList.h>


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
	m_Constants.HasDiffuseMap = false;
	m_Constants.HasNormalMap = false;
	m_Constants.HasMetallicMap = false;
	m_Constants.HasRoughnessMap = false;
	m_Constants.HasAmbientOcclusionMap = false;
}

void PbrMaterial::SetMap(MapType mapType, std::shared_ptr<Texture> map)
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
	case Metallic:
		m_Constants.HasMetallicMap = true;
		break;
	case Roughness:
		m_Constants.HasRoughnessMap = true;
		break;
	case AmbientOcclusion:
		m_Constants.HasAmbientOcclusionMap = true;
		break;
	default:
		throw std::exception("Invalid map.");
	}
}

void PbrMaterial::SetShaderResourceViews(CommandList& commandList, uint32_t rootParameterIndex, uint32_t baseDescriptorOffset /*= 0*/)
{
	for (size_t i = 0; i < TotalNumMaps; ++i)
	{
		commandList.SetShaderResourceView(rootParameterIndex, baseDescriptorOffset + i, *m_Maps[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}

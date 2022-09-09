#include "PointLightShadowPassPso.h"

#include "CommandList.h"
#include "CommandQueue.h"

PointLightShadowPassPso::PointLightShadowPassPso(const Microsoft::WRL::ComPtr<ID3D12Device2>& device,
                                                 const UINT resolution): ShadowPassPsoBase(device, resolution),
                                                                         m_CubeShadowMapsCount(0),
                                                                         m_CurrentLightIndex(0),
                                                                         m_CurrentCubeMapSideIndex(0)
{
	EnsureSufficientShadowMaps(DEFAULT_CUBE_SHADOW_MAPS_COUNT);
}

void PointLightShadowPassPso::SetRenderTarget(CommandList& commandList) const
{
	commandList.SetRenderTarget(m_ShadowMapArray, GetCurrentSubresource());
}

void PointLightShadowPassPso::ClearShadowMap(CommandList& commandList) const
{
	commandList.ClearDepthStencilTexture(GetShadowMapsAsTexture(), D3D12_CLEAR_FLAG_DEPTH, 1, 0,
	                                     GetCurrentSubresource());
}

void PointLightShadowPassPso::SetShadowMapShaderResourceView(CommandList& commandList, uint32_t rootParameterIndex,
                                                             uint32_t descriptorOffset) const
{
}

void PointLightShadowPassPso::SetCurrentShadowMap(const uint32_t lightIndex, const uint32_t cubeMapSideIndex)
{
	if (lightIndex >= m_CubeShadowMapsCount)
		throw std::exception("Light index out of range");

	m_CurrentLightIndex = lightIndex;
	m_CurrentCubeMapSideIndex = cubeMapSideIndex;
}

void PointLightShadowPassPso::EnsureSufficientShadowMaps(const uint32_t count)
{
	if (count < m_CubeShadowMapsCount) return;

	const uint32_t arraySize = count * TEXTURES_IN_CUBEMAP;
	const auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(SHADOW_MAP_FORMAT,
	                                                        m_Resolution, m_Resolution,
	                                                        arraySize, 1,
	                                                        1, 0,
	                                                        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = shadowMapDesc.Format;
	depthClearValue.DepthStencil = {1.0f, 0};

	const auto shadowMap = Texture(shadowMapDesc, &depthClearValue,
	                               TextureUsageType::Depth,
	                               L"Point Light Shadow Map Array");
	m_ShadowMapArray.AttachTexture(DepthStencil, shadowMap);
	m_CubeShadowMapsCount = count;
}

uint32_t PointLightShadowPassPso::GetCurrentSubresource() const
{
	return m_CurrentLightIndex * TEXTURES_IN_CUBEMAP + m_CurrentCubeMapSideIndex;
}

const Texture& PointLightShadowPassPso::GetShadowMapsAsTexture() const
{
	return m_ShadowMapArray.GetTexture(DepthStencil);
}

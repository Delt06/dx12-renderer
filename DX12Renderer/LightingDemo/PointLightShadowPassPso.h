#pragma once
#include "RenderTarget.h"
#include "ShadowPassPsoBase.h"

class PointLightShadowPassPso final : public ShadowPassPsoBase
{
public:
	PointLightShadowPassPso(const Microsoft::WRL::ComPtr<ID3D12Device2>& device, UINT resolution);
	void SetRenderTarget(CommandList& commandList) const override;
	void ClearShadowMap(CommandList& commandList) const override;
	void SetShadowMapShaderResourceView(CommandList& commandList, uint32_t rootParameterIndex,
	                                    uint32_t descriptorOffset) const override;

	void SetCurrentShadowMap(uint32_t lightIndex, uint32_t cubeMapSideIndex);
	void EnsureSufficientShadowMaps(uint32_t count);


private:
	uint32_t GetCurrentSubresource() const;
	[[nodiscard]] const Texture& GetShadowMapsAsTexture() const;

	RenderTarget m_ShadowMapArray;
	uint32_t m_CubeShadowMapsCount;
	uint32_t m_CurrentLightIndex;
	uint32_t m_CurrentCubeMapSideIndex;

	static constexpr uint32_t TEXTURES_IN_CUBEMAP = 6;
	static constexpr uint32_t DEFAULT_CUBE_SHADOW_MAPS_COUNT = 4;
};

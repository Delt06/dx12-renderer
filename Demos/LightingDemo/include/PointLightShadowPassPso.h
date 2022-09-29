﻿#pragma once
#include <DX12Library/RenderTarget.h>
#include "ShadowPassPsoBase.h"

struct PointLight;
struct Scene;

class PointLightShadowPassPso final : public ShadowPassPsoBase
{
public:
	PointLightShadowPassPso(const Microsoft::WRL::ComPtr<ID3D12Device2>& device, UINT resolution);
	void SetRenderTarget(CommandList& commandList) const override;
	void ClearShadowMap(CommandList& commandList) const override;
	void SetShadowMapShaderResourceView(CommandList& commandList, uint32_t rootParameterIndex,
		uint32_t descriptorOffset = 0) const override;

	void ComputePassParameters(const PointLight& pointLight);
	void SetCurrentShadowMap(uint32_t lightIndex, uint32_t cubeMapSideIndex);
	void SetShadowMapsCount(uint32_t count);


private:
	static constexpr uint32_t DEFAULT_CUBE_SHADOW_MAPS_COUNT = 0;

	[[nodiscard]] uint32_t GetCurrentSubresource() const;
	[[nodiscard]] const Texture& GetShadowMapsAsTexture() const;

	RenderTarget m_ShadowMapArray;
	uint32_t m_CubeShadowMapsCapacity;
	uint32_t m_CubeShadowMapsCount;
	uint32_t m_CurrentLightIndex;
	uint32_t m_CurrentCubeMapSideIndex;
};

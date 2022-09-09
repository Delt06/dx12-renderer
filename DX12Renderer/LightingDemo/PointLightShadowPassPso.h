#pragma once
#include "RenderTarget.h"
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
	                                    uint32_t descriptorOffset) const override;

	void ComputePassParameters(const PointLight& pointLight);
	void SetCurrentShadowMap(uint32_t lightIndex, uint32_t cubeMapSideIndex);
	void EnsureSufficientShadowMaps(uint32_t count);

	static constexpr uint32_t TEXTURES_IN_CUBEMAP = 6;


private:
	struct CubeSideOrientation
	{
		DirectX::XMVECTOR m_Forward;
		DirectX::XMVECTOR m_Up;
	};
	
	static constexpr uint32_t DEFAULT_CUBE_SHADOW_MAPS_COUNT = 4;

	inline static const CubeSideOrientation CUBE_SIDE_ORIENTATIONS[TEXTURES_IN_CUBEMAP] =
	{
		// +X
		{DirectX::XMVectorSet(1, 0, 0, 0), DirectX::XMVectorSet(0, 1, 0, 0)},
		// -X
		{DirectX::XMVectorSet(-1, 0, 0, 0), DirectX::XMVectorSet(0, 1, 0, 0)},
		// +Y
		{DirectX::XMVectorSet(0, 1, 0, 0), DirectX::XMVectorSet(1, 0, 0, 0)},
		// -Y
		{DirectX::XMVectorSet(0, -1, 0, 0), DirectX::XMVectorSet(1, 0, 0, 0)},
		// +Z
		{DirectX::XMVectorSet(0, 0, 1, 0), DirectX::XMVectorSet(0, 1, 0, 0)},
		// -Z
		{DirectX::XMVectorSet(0, 0, -1, 0), DirectX::XMVectorSet(0, 1, 0, 0)},
	};

	[[nodiscard]] uint32_t GetCurrentSubresource() const;
	[[nodiscard]] const Texture& GetShadowMapsAsTexture() const;

	RenderTarget m_ShadowMapArray;
	uint32_t m_CubeShadowMapsCount;
	uint32_t m_CurrentLightIndex;
	uint32_t m_CurrentCubeMapSideIndex;
};

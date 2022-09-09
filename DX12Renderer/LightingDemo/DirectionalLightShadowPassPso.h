#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "GameObject.h"
#include "RenderTarget.h"
#include "Scene.h"
#include "ShadowPassPsoBase.h"

class Camera;
struct DirectionalLight;
class CommandList;

class DirectionalLightShadowPassPso final : public ShadowPassPsoBase
{
public:
	explicit DirectionalLightShadowPassPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, UINT resolution = 4096);
	void ComputePassParameters(const Scene& scene, const DirectionalLight& directionalLight);

	[[nodiscard]] DirectX::XMMATRIX ComputeShadowModelViewProjectionMatrix(DirectX::XMMATRIX worldMatrix) const;

	void SetRenderTarget(CommandList& commandList) const override;
	void ClearShadowMap(CommandList& commandList) const override;
	void SetShadowMapShaderResourceView(CommandList& commandList, uint32_t rootParameterIndex,
	                                    uint32_t descriptorOffset = 0) const override;

private:
	[[nodiscard]] const Texture& GetShadowMapAsTexture() const;

	RenderTarget m_ShadowMap;
};

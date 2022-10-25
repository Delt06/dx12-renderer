#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include <Framework/GameObject.h>
#include <DX12Library/RenderTarget.h>
#include "Scene.h"
#include "ShadowPassPsoBase.h"

class Camera;
struct DirectionalLight;
class CommandList;

class DirectionalLightShadowPassPso final : public ShadowPassPsoBase
{
public:
	explicit DirectionalLightShadowPassPso(const std::shared_ptr<CommonRootSignature>& rootSignature, UINT resolution = 4096);
	void ComputePassParameters(const Scene& scene, const DirectionalLight& directionalLight);

	[[nodiscard]] DirectX::XMMATRIX ComputeShadowModelViewProjectionMatrix(DirectX::XMMATRIX worldMatrix) const;

	void SetRenderTarget(CommandList& commandList) const override;
	void ClearShadowMap(CommandList& commandList) const override;

	ShaderResourceView GetShadowMapShaderResourceView() const override;

private:
	[[nodiscard]] const std::shared_ptr<Texture>& GetShadowMapAsTexture() const;

	RenderTarget m_ShadowMap;
};

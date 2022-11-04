#pragma once

#include <DirectXMath.h>

#include <Framework/Material.h>
#include <Framework/BoundingSphere.h>
#include <Framework/Light.h>

class DirectionalLightShadowsPass
{
public:
    explicit DirectionalLightShadowsPass(const std::shared_ptr<CommonRootSignature>& rootSignature, uint32_t size = 2048);

    [[nodiscard]] DirectX::XMMATRIX ComputeShadowViewProjectionMatrix(const BoundingSphere& sceneBounds, const DirectionalLight& directionalLight) const;
    [[nodiscard]] const std::shared_ptr<Texture>& GetShadowMap() const;
    [[nodiscard]] ShaderResourceView GetShadowMapShaderResourceView() const;

    void Begin(CommandList& commandList);
    void End(CommandList& commandList);

private:
    std::shared_ptr<Material> m_Material;
    RenderTarget m_ShadowMapRenderTarget;
};

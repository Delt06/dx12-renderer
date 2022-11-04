#pragma once

#include <DirectXMath.h>

#include <DX12Library/ClearValue.h>

#include <Framework/Material.h>
#include <Framework/BoundingSphere.h>
#include <Framework/Light.h>
#include <Framework/Mesh.h>

class DirectionalLightShadowsPass
{
public:
    explicit DirectionalLightShadowsPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, uint32_t size = 2048);

    struct Matrices
    {
        DirectX::XMMATRIX m_ViewMatrix;
        DirectX::XMMATRIX m_ViewProjectionMatrix;
    };

    [[nodiscard]] Matrices ComputeMatrices(const BoundingSphere& sceneBounds, const DirectionalLight& directionalLight) const;

    [[nodiscard]] ShaderResourceView GetShadowMapShaderResourceView() const;
    [[nodiscard]] ShaderResourceView GetVarianceMapShaderResourceView() const;

    void BeginShadowCasterPass(CommandList& commandList);
    void EndShadowCasterPass(CommandList& commandList);

    void Blur(CommandList& commandList);

private:
    std::shared_ptr<Material> m_ShadowCasterMaterial;
    std::shared_ptr<Material> m_GaussianBlurMaterial;
    std::shared_ptr<Mesh> m_BlitTriangle;

    RenderTarget m_ShadowMapRenderTarget;
    RenderTarget m_VarianceMapRenderTarget1;
    RenderTarget m_VarianceMapRenderTarget2;
    ClearValue m_VarianceMapClearValue;
};

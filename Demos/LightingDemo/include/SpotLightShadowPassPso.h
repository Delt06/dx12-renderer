#pragma once

#include <Framework/Light.h>
#include <DX12Library/RenderTarget.h>
#include "ShadowPassPsoBase.h"

class SpotLightShadowPassPso final : public ShadowPassPsoBase
{
public:
    SpotLightShadowPassPso(const std::shared_ptr<CommonRootSignature>& rootSignature, UINT resolution);
    void SetRenderTarget(CommandList& commandList) const override;
    void ClearShadowMap(CommandList& commandList) const override;

    ShaderResourceView GetShadowMapShaderResourceView() const override;

    void ComputePassParameters(const SpotLight& spotLight);
    void SetCurrentShadowMap(uint32_t lightIndex);
    void SetShadowMapsCount(uint32_t count);

private:
    [[nodiscard]] const std::shared_ptr<Texture>& GetShadowMapsAsTexture() const;

    RenderTarget m_ShadowMapArray;
    uint32_t m_ShadowMapsCapacity;
    uint32_t m_ShadowMapsCount;
    uint32_t m_CurrentLightIndex;
};

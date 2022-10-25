#include <SpotLightShadowPassPso.h>

#include <DX12Library/CommandList.h>

using namespace Microsoft::WRL;
using namespace DirectX;

SpotLightShadowPassPso::SpotLightShadowPassPso(const std::shared_ptr<CommonRootSignature>& rootSignature, UINT resolution)
    : ShadowPassPsoBase(rootSignature, resolution)
    , m_ShadowMapsCapacity(0)
    , m_ShadowMapsCount(0)
    , m_CurrentLightIndex(0)
{
    m_ShadowPassParameters.LightType = ShadowPassParameters::SpotLight;
}

void SpotLightShadowPassPso::SetRenderTarget(CommandList& commandList) const
{
    commandList.SetRenderTarget(m_ShadowMapArray, m_CurrentLightIndex);
}

void SpotLightShadowPassPso::ClearShadowMap(CommandList& commandList) const
{
    commandList.ClearDepthStencilTexture(*GetShadowMapsAsTexture(), D3D12_CLEAR_FLAG_DEPTH);
}

ShaderResourceView SpotLightShadowPassPso::GetShadowMapShaderResourceView() const
{
    const uint32_t numSubresources = m_ShadowMapsCount;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.ArraySize = numSubresources;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.PlaneSlice = 0;
    srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

    return ShaderResourceView(GetShadowMapsAsTexture(), 0, numSubresources, srvDesc);
}

void SpotLightShadowPassPso::ComputePassParameters(const SpotLight& spotLight)
{
    const XMVECTOR eyePosition = XMLoadFloat4(&spotLight.PositionWs);
    const XMVECTOR eyeDirection = XMLoadFloat4(&spotLight.DirectionWs);
    const auto viewMatrix = XMMatrixLookToLH(eyePosition, eyeDirection, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    const auto projectionMatrix = XMMatrixPerspectiveFovLH(spotLight.SpotAngle * 2, 1, 0.1f, 100.0f);
    const auto viewProjection = viewMatrix * projectionMatrix;

    m_ShadowPassParameters.LightDirectionWs = spotLight.PositionWs;
    m_ShadowPassParameters.ViewProjection = viewProjection;
}

void SpotLightShadowPassPso::SetCurrentShadowMap(const uint32_t lightIndex)
{
    if (lightIndex >= m_ShadowMapsCount)
        throw std::exception("Light index out of range");
    m_CurrentLightIndex = lightIndex;
}

void SpotLightShadowPassPso::SetShadowMapsCount(const uint32_t count)
{
    if (count >= m_ShadowMapsCapacity && count > 0)
    {
        const auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(SHADOW_MAP_FORMAT,
            m_Resolution, m_Resolution,
            count, 1,
            1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        D3D12_CLEAR_VALUE depthClearValue;
        depthClearValue.Format = shadowMapDesc.Format;
        depthClearValue.DepthStencil = { 1.0f, 0 };

        const auto shadowMap = std::make_shared<Texture>(shadowMapDesc, &depthClearValue,
            TextureUsageType::Depth,
            L"Spot Light Shadow Map Array");
        m_ShadowMapArray.AttachTexture(DepthStencil, shadowMap);
        m_ShadowMapsCapacity = count;
    }

    m_ShadowMapsCount = count;
}

const std::shared_ptr<Texture>& SpotLightShadowPassPso::GetShadowMapsAsTexture() const
{
    return m_ShadowMapArray.GetTexture(DepthStencil);
}

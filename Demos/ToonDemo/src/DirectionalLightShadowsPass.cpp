#include "DirectionalLightShadowsPass.h"

using namespace DirectX;

DirectionalLightShadowsPass::DirectionalLightShadowsPass(const std::shared_ptr<CommonRootSignature>& rootSignature, uint32_t size)
    : m_Material(Material::Create(
        std::make_shared<Shader>(
            rootSignature,
            ShaderBlob(L"ShadowCaster_VS.cso"),
            ShaderBlob(L"ShadowCaster_PS.cso")
            )
    ))
{
    const auto shadowMapFormat = DXGI_FORMAT_D32_FLOAT;
    auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        shadowMapFormat,
        size, size,
        1, 1,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );
    auto clearValue = ClearValue(shadowMapFormat, ClearValue::DEPTH_STENCIL_VALUE{ 1.0f, 0 });
    auto shadowMap = std::make_shared<Texture>(shadowMapDesc, clearValue, TextureUsageType::Depth, L"Shadow Map");
    m_ShadowMapRenderTarget.AttachTexture(DepthStencil, shadowMap);
}

[[nodiscard]] DirectX::XMMATRIX DirectionalLightShadowsPass::ComputeShadowViewProjectionMatrix(const BoundingSphere& sceneBounds, const DirectionalLight& directionalLight) const
{
    const XMVECTOR lightDirection = XMLoadFloat4(&directionalLight.m_DirectionWs);
    const float radius = sceneBounds.GetRadius();

    const auto viewMatrix = XMMatrixLookToLH(2.0f * radius * lightDirection, -lightDirection,
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMFLOAT3 sceneCenterVs{};
    XMStoreFloat3(&sceneCenterVs, XMVector3TransformCoord(sceneBounds.GetCenter(), viewMatrix));

    const float projLeft = sceneCenterVs.x - radius;
    const float projRight = sceneCenterVs.x + radius;
    const float projBottom = sceneCenterVs.y - radius;
    const float projTop = sceneCenterVs.y + radius;
    const float projNear = sceneCenterVs.z - radius;
    const float projFar = sceneCenterVs.z + radius;

    const auto projectionMatrix = XMMatrixOrthographicOffCenterLH(projLeft, projRight,
        projBottom, projTop,
        projNear, projFar
    );
    const auto viewProjection = viewMatrix * projectionMatrix;
    return viewProjection;
}

[[nodiscard]] const std::shared_ptr<Texture>& DirectionalLightShadowsPass::GetShadowMap() const
{
    return m_ShadowMapRenderTarget.GetTexture(DepthStencil);
}

[[nodiscard]] ShaderResourceView DirectionalLightShadowsPass::GetShadowMapShaderResourceView() const
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;

    return ShaderResourceView(GetShadowMap(), srvDesc);
}

void DirectionalLightShadowsPass::Begin(CommandList& commandList)
{
    commandList.ClearDepthStencilTexture(*m_ShadowMapRenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);

    commandList.SetRenderTarget(m_ShadowMapRenderTarget);
    commandList.SetAutomaticViewportAndScissorRect(m_ShadowMapRenderTarget);

    m_Material->Bind(commandList);
}

void DirectionalLightShadowsPass::End(CommandList& commandList)
{
    m_Material->Unbind(commandList);
}

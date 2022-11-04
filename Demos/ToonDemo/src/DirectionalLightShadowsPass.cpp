#include "DirectionalLightShadowsPass.h"

#include <DX12Library/Helpers.h>

#include <Framework/Blit_VS.h>

using namespace DirectX;

namespace
{
    constexpr DXGI_FORMAT SHADOW_MAP_FORMAT = DXGI_FORMAT_D32_FLOAT;
    constexpr DXGI_FORMAT VARIANCE_MAP_FORMAT = DXGI_FORMAT_R32G32_FLOAT;
}

DirectionalLightShadowsPass::DirectionalLightShadowsPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, uint32_t size)
    : m_ShadowCasterMaterial(Material::Create(
        std::make_shared<Shader>(
            rootSignature,
            ShaderBlob(L"ShadowCaster_VS.cso"),
            ShaderBlob(L"ShadowCaster_PS.cso")
            )
    ))
    , m_GaussianBlurMaterial(Material::Create(
        std::make_shared<Shader>(
            rootSignature,
            ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
            ShaderBlob(L"VarianceShadows_GaussianBlur_PS.cso"),
            [](PipelineStateBuilder& builder)
            {
                builder.WithDisabledDepthStencil();
            }
        ))
    )
    , m_BlitTriangle(Mesh::CreateBlitTriangle(commandList))
    , m_VarianceMapClearValue(VARIANCE_MAP_FORMAT, DirectX::XMFLOAT4{ INFINITY, INFINITY, 0, 0 })
{
    auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        SHADOW_MAP_FORMAT,
        size, size,
        1, 1,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );
    auto shadowMapClearValue = ClearValue(SHADOW_MAP_FORMAT, ClearValue::DEPTH_STENCIL_VALUE{ 1.0f, 0 });
    auto shadowMap = std::make_shared<Texture>(shadowMapDesc, shadowMapClearValue, TextureUsageType::Depth, L"Shadow Map (Depth)");
    m_ShadowMapRenderTarget.AttachTexture(DepthStencil, shadowMap);

    auto varianceMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        VARIANCE_MAP_FORMAT,
        size, size,
        1, 1,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
    );
    auto varianceMapClearValue = ClearValue();
    auto varianceMap = std::make_shared<Texture>(varianceMapDesc, varianceMapClearValue, TextureUsageType::RenderTarget, L"Shadow Map (Variance)");

    m_ShadowMapRenderTarget.AttachTexture(Color0, varianceMap);
    m_VarianceMapRenderTarget1.AttachTexture(Color0, varianceMap);

    auto varianceMap2 = std::make_shared<Texture>(varianceMapDesc, varianceMapClearValue, TextureUsageType::RenderTarget, L"Shadow Map (Variance, Temp)");
    m_VarianceMapRenderTarget2.AttachTexture(Color0, varianceMap2);

    m_GaussianBlurMaterial->SetVariable("texelSize",
        XMFLOAT2{ 1.0f / static_cast<float>(size), 1.0f / static_cast<float>(size) }
    );
}

[[nodiscard]] DirectionalLightShadowsPass::Matrices DirectionalLightShadowsPass::ComputeMatrices(const BoundingSphere& sceneBounds, const DirectionalLight& directionalLight) const
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
    return { viewMatrix, viewProjection };
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

    return ShaderResourceView(m_ShadowMapRenderTarget.GetTexture(DepthStencil), srvDesc);
}

[[nodiscard]] ShaderResourceView DirectionalLightShadowsPass::GetVarianceMapShaderResourceView() const
{
    return ShaderResourceView(m_ShadowMapRenderTarget.GetTexture(Color0));
}

void DirectionalLightShadowsPass::BeginShadowCasterPass(CommandList& commandList)
{
    commandList.ClearRenderTarget(m_ShadowMapRenderTarget, m_VarianceMapClearValue, D3D12_CLEAR_FLAG_DEPTH);

    commandList.SetRenderTarget(m_ShadowMapRenderTarget);
    commandList.SetAutomaticViewportAndScissorRect(m_ShadowMapRenderTarget);

    m_ShadowCasterMaterial->Bind(commandList);
}

void DirectionalLightShadowsPass::EndShadowCasterPass(CommandList& commandList)
{
    m_ShadowCasterMaterial->Unbind(commandList);
}

void DirectionalLightShadowsPass::Blur(CommandList& commandList)
{
    PIXScope(commandList, "Variance Shadow Map: Blur");

    m_GaussianBlurMaterial->BeginBatch(commandList);
    {
        commandList.SetRenderTarget(m_VarianceMapRenderTarget2);
        commandList.SetAutomaticViewportAndScissorRect(m_VarianceMapRenderTarget2);

        m_GaussianBlurMaterial->SetVariable("direction", XMFLOAT2(1.0f, 0.0f));
        m_GaussianBlurMaterial->SetShaderResourceView("sourceTexture", ShaderResourceView(m_VarianceMapRenderTarget1.GetTexture(Color0)));
        m_GaussianBlurMaterial->UploadUniforms(commandList);

        m_BlitTriangle->Draw(commandList);
    }

    {
        commandList.SetRenderTarget(m_VarianceMapRenderTarget1);
        commandList.SetAutomaticViewportAndScissorRect(m_VarianceMapRenderTarget1);

        m_GaussianBlurMaterial->SetVariable("direction", XMFLOAT2(0.0f, 1.0f));
        m_GaussianBlurMaterial->SetShaderResourceView("sourceTexture", ShaderResourceView(m_VarianceMapRenderTarget2.GetTexture(Color0)));
        m_GaussianBlurMaterial->UploadUniforms(commandList);

        m_BlitTriangle->Draw(commandList);
    }
    m_GaussianBlurMaterial->EndBatch(commandList);
}

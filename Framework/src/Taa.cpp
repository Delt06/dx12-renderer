#include "Taa.h"
#include <Framework/Mesh.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Texture.h>
#include <DX12Library/ShaderUtils.h>
#include <Framework/Blit_VS.h>
#include <Framework/TAA_Resolve_PS.h>

namespace
{
    struct RootConstants
    {
        DirectX::XMFLOAT2 m_TexelSize;
    };
}

Taa::Taa(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, DXGI_FORMAT backBufferFormat, uint32_t width, uint32_t height)
    : m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
    , m_RootSignature(rootSignature)
    , m_Width(width)
    , m_Height(height)
{
    auto shader = std::make_shared<Shader>(rootSignature,
        ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
        ShaderBlob(ShaderBytecode_TAA_Resolve_PS, sizeof ShaderBytecode_TAA_Resolve_PS)
        );
    m_Material = Material::Create(shader);

    auto rtColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);;
    auto taaTempTexture = std::make_shared<Texture>(rtColorDesc, nullptr, TextureUsageType::RenderTarget, L"TAA Resolve RT");
    m_ResolveRenderTarget.AttachTexture(Color0, taaTempTexture);

    auto historyBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat, width, height, 1, 1);
    m_HistoryBuffer = std::make_shared<Texture>(historyBufferDesc, nullptr, TextureUsageType::Other, L"TAA History Buffer");
}

DirectX::XMFLOAT2 Taa::ComputeJitterOffset() const
{
    DirectX::XMFLOAT2 jitterOffset = JITTER_OFFSETS[m_FrameIndex];
    DirectX::XMFLOAT2 viewSize = { static_cast<float>(m_Width), static_cast<float>(m_Height) };
    jitterOffset.x = ((jitterOffset.x - 0.5f) / viewSize.x);
    jitterOffset.y = ((jitterOffset.y - 0.5f) / viewSize.y);

    return jitterOffset;
}

const DirectX::XMMATRIX& Taa::GetPreviousViewProjectionMatrix() const
{
    return m_PreviousViewProjectionMatrix;
}

void Taa::Resolve(CommandList& commandList, const std::shared_ptr<Texture>& currentBuffer, const std::shared_ptr<Texture>& velocityBuffer)
{
    PIXScope(commandList, "TAA");

    {
        PIXScope(commandList, "Resolve TAA");

        {
            RootConstants rootConstants;
            rootConstants.m_TexelSize =
            {
                1.0f / static_cast<float>(m_Width),
                1.0f / static_cast<float>(m_Height),
            };
            m_RootSignature->SetGraphicsRootConstants(commandList, rootConstants);
        }

        m_Material->SetShaderResourceView("currentColorBuffer", ShaderResourceView(currentBuffer));
        m_Material->SetShaderResourceView("historyColorBuffer", ShaderResourceView(m_HistoryBuffer));
        m_Material->SetShaderResourceView("velocityColorBuffer", ShaderResourceView(velocityBuffer));

        commandList.SetRenderTarget(m_ResolveRenderTarget);
        commandList.SetAutomaticViewportAndScissorRect(m_ResolveRenderTarget);

        m_Material->Bind(commandList);
        m_BlitMesh->Draw(commandList);
        m_Material->Unbind(commandList);
    }

    {
        PIXScope(commandList, "Capture TAA History");

        commandList.CopyResource(*m_HistoryBuffer, *m_ResolveRenderTarget.GetTexture(Color0));
    }
}

void Taa::Resize(uint32_t width, uint32_t height)
{
    m_Width = width;
    m_Height = height;
    m_HistoryBuffer->Resize(width, height);
    m_ResolveRenderTarget.Resize(width, height);
}

const std::shared_ptr<Texture>& Taa::GetResolvedTexture() const
{
    return m_ResolveRenderTarget.GetTexture(Color0);
}

DirectX::XMFLOAT2 Taa::GetCurrentJitterOffset() const
{
    return JITTER_OFFSETS[m_FrameIndex];
}

void Taa::OnRenderedFrame(const DirectX::XMMATRIX& viewProjectionMatrix)
{
    m_PreviousViewProjectionMatrix = viewProjectionMatrix;
    m_FrameIndex = (m_FrameIndex + 1) % JITTER_OFFSETS_COUNT;
}

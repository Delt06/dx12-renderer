#include "DownsamplePass.h"

#include <DirectXMath.h>

#include <Framework/Blit_VS.h>

DownsamplePass::DownsamplePass(const std::shared_ptr<CommonRootSignature>& rootSignature,
    CommandList& commandList,
    const std::wstring& textureName,
    DXGI_FORMAT format, uint32_t width, uint32_t height,

    uint32_t factor)
    : m_Material(Material::Create(
        std::make_shared<Shader>(rootSignature,
            ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
            ShaderBlob(L"Downsample_PS.cso")
            )
    )),
    m_BlitMesh(Mesh::CreateBlitTriangle(commandList)),
    m_Factor(factor)
{
    width /= m_Factor;
    height /= m_Factor;

    const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height,
        1, 1,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
    );
    const auto texture = std::make_shared<Texture>(textureDesc, nullptr, TextureUsageType::RenderTarget, textureName);
    m_RenderTarget.AttachTexture(Color0, texture);
}

void DownsamplePass::Resize(uint32_t width, uint32_t height)
{
    width /= m_Factor;
    height /= m_Factor;

    m_RenderTarget.Resize(width, height);
}

void DownsamplePass::Execute(CommandList& commandList, const ShaderResourceView& sourceSRV)
{
    const auto resourceDesc = sourceSRV.m_Resource->GetD3D12ResourceDesc();
    const DirectX::XMFLOAT2 texelSize =
    {
        1.0f / static_cast<float>(resourceDesc.Width) / static_cast<float>(m_Factor),
        1.0f / static_cast<float>(resourceDesc.Height) / static_cast<float>(m_Factor)
    };
    m_Material->SetVariable("TexelSize", texelSize);
    m_Material->SetShaderResourceView("sourceTexture", sourceSRV);

    commandList.SetRenderTarget(m_RenderTarget);
    commandList.SetAutomaticViewportAndScissorRect(m_RenderTarget);

    m_Material->Bind(commandList);
    m_BlitMesh->Draw(commandList);
    m_Material->Unbind(commandList);
}

const std::shared_ptr<Texture>& DownsamplePass::GetResultTexture() const
{
    return m_RenderTarget.GetTexture(Color0);
}

#include "SSAOPass.h"

#include <DX12Library/Helpers.h>

#include <Framework/Blit_VS.h>
#include <Framework/SSAOUtils.h>

namespace
{
    constexpr size_t SAMPLES_COUNT = 32;
}

const std::shared_ptr<Texture>& SSAOPass::GetFinalTexture() const
{
    return m_SSAOFinalRenderTarget.GetTexture(Color0);
}

void SSAOPass::Execute(CommandList& commandList, const std::shared_ptr<Texture>& sceneDepth, const std::shared_ptr<Texture>& sceneNormals)
{
    PIXScope(commandList, "SSAO");

    {
        PIXScope(commandList, "SSAO: Main Pass");

        const auto& ssaoTexture = m_SSAORawRenderTarget.GetTexture(Color0);
        const auto ssaoTextureDesc = ssaoTexture->GetD3D12ResourceDesc();
        const auto noiseTextureDesc = m_NoiseTexture->GetD3D12ResourceDesc();

        m_SSAOMaterial->SetVariable<DirectX::XMFLOAT2>("NoiseScale", {
            static_cast<float>(ssaoTextureDesc.Width) / static_cast<float>(noiseTextureDesc.Width),
            static_cast<float>(ssaoTextureDesc.Height) / static_cast<float>(noiseTextureDesc.Height),
            });
        m_SSAOMaterial->SetVariable("Radius", 0.2f);
        m_SSAOMaterial->SetVariable("Power", 15.0f);

        m_SSAOMaterial->SetShaderResourceView("sceneDepth", ShaderResourceView(sceneDepth));
        m_SSAOMaterial->SetShaderResourceView("sceneNormals", ShaderResourceView(sceneNormals));

        commandList.SetRenderTarget(m_SSAORawRenderTarget);
        commandList.SetAutomaticViewportAndScissorRect(m_SSAORawRenderTarget);

        m_SSAOMaterial->Bind(commandList);
        m_BlitMesh->Draw(commandList);
        m_SSAOMaterial->Unbind(commandList);
    }

    {
        PIXScope(commandList, "SSAO: Blur Pass");

        const auto& ssaoTexture = m_SSAORawRenderTarget.GetTexture(Color0);
        const auto ssaoTextureDesc = ssaoTexture->GetD3D12ResourceDesc();

        m_SSAOBlurMaterial->SetVariable<DirectX::XMFLOAT2>("TexelSize", {
            1.0f / static_cast<float>(ssaoTextureDesc.Width),
            1.0f / static_cast<float>(ssaoTextureDesc.Height),
            });

        commandList.SetRenderTarget(m_SSAOFinalRenderTarget);
        commandList.SetAutomaticViewportAndScissorRect(m_SSAOFinalRenderTarget);

        m_SSAOBlurMaterial->Bind(commandList);
        m_BlitMesh->Draw(commandList);
        m_SSAOBlurMaterial->Unbind(commandList);
    }
}

void SSAOPass::Resize(uint32_t width, uint32_t height)
{
    width >>= 1;
    height >>= 1;

    m_SSAORawRenderTarget.Resize(width, height);
    m_SSAOFinalRenderTarget.Resize(width, height);
}

SSAOPass::SSAOPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, uint32_t width, uint32_t height)
    : m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
    width >>= 1;
    height >>= 1;

    {
        SSAOUtils::GenerateRandomSamples(SAMPLES_COUNT, m_Samples);
        m_NoiseTexture = SSAOUtils::GenerateNoiseTexture(commandList);
    }

    {
        auto ssaoTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16_UNORM,
            width, height,
            1, 1,
            1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        );
        auto ssaoRawTexture = std::make_shared<Texture>(ssaoTextureDesc, nullptr, TextureUsageType::RenderTarget, L"SSAO (Raw)");
        m_SSAORawRenderTarget.AttachTexture(Color0, ssaoRawTexture);

        auto ssaoFinalTexture = std::make_shared<Texture>(ssaoTextureDesc, nullptr, TextureUsageType::RenderTarget, L"SSAO");
        m_SSAOFinalRenderTarget.AttachTexture(Color0, ssaoFinalTexture);
    }

    {
        auto ssaoShader = std::make_shared<Shader>(rootSignature,
            ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
            ShaderBlob(L"SSAO_PS.cso")
            );
        m_SSAOMaterial = Material::Create(ssaoShader);
        m_SSAOMaterial->SetShaderResourceView("noiseTexture", ShaderResourceView(m_NoiseTexture));
        m_SSAOMaterial->SetArrayVariable("Samples", m_Samples);
        m_SSAOMaterial->SetVariable<uint32_t>("KernelSize", SAMPLES_COUNT);

        auto ssaoBlurShader = std::make_shared<Shader>(rootSignature,
            ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
            ShaderBlob(L"SSAO_Blur_PS.cso")
            );
        m_SSAOBlurMaterial = Material::Create(ssaoBlurShader);
        m_SSAOBlurMaterial->SetShaderResourceView("ssaoTexture", ShaderResourceView(m_SSAORawRenderTarget.GetTexture(Color0)));
    }
}



#include "Ssao.h"
#include <Framework/Mesh.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/Texture.h>
#include <DX12Library/ShaderUtils.h>
#include <Framework/Blit_VS.h>
#include <Framework/SSAOUtils.h>
#include <random>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace
{
    float LerpUnclamped(float a, float b, float t)
    {
        return a + t * (b - a);
    }

    namespace SsaoPassRootParameters
    {
        enum RootParameters
        {
            CBuffer,
            GBuffer,
            NoiseTexture,
            NumRootParameters
        };

        struct SSAOCBuffer
        {
            static constexpr size_t SAMPLES_COUNT = 64;

            XMFLOAT2 NoiseScale;
            float Radius;
            uint32_t KernelSize;

            float Power;
            float _Padding[3];

            XMMATRIX InverseProjection;
            XMMATRIX InverseView;
            XMMATRIX View;
            XMMATRIX ViewProjection;

            XMFLOAT4 Samples[SAMPLES_COUNT];
        };
    }

    namespace BlurPassRootParameters
    {
        enum RootParameters
        {
            CBuffer,
            SSAO,
            NumRootParameters
        };

        struct BlurCBuffer
        {
            XMFLOAT2 TexelSize;
            float _Padding[2];
        };
    }
}

Ssao::Ssao(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, uint32_t width, uint32_t height, bool downsample)
    : m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
    , m_Downsample(downsample)
{
    DXGI_FORMAT ssaoFormat = DXGI_FORMAT_R8_UNORM;

    if (m_Downsample)
    {
        width >>= 1;
        height >>= 1;
    }

    // Create SSAO RT
    {
        auto ssaoTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(ssaoFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        auto ssaoTexture = std::make_shared<Texture>(ssaoTextureDesc, nullptr, TextureUsageType::RenderTarget, L"SSAO");
        m_RenderTarget.AttachTexture(Color0, ssaoTexture);
    }

    ShaderBlob blitShaderBlob = { ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS };

    {
        auto shader = std::make_shared<Shader>(rootSignature, blitShaderBlob, ShaderBlob(L"SSAO_PS.cso"));
        m_SsaoPassMaterial = Material::Create(shader);
    }

    {
        auto shader = std::make_shared<Shader>(rootSignature, blitShaderBlob, ShaderBlob(L"SSAO_Blur_PS.cso"),
            [](PipelineStateBuilder& builder)
            {
                CD3DX12_BLEND_DESC blendDesc{};
                auto& rtBlendDesc = blendDesc.RenderTarget[0];
                rtBlendDesc.BlendEnable = true;
                rtBlendDesc.BlendOp = D3D12_BLEND_OP_MIN;
                rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_MIN;
                rtBlendDesc.DestBlend = D3D12_BLEND_ONE;
                rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
                rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;
                rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
                rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_BLUE; // only write to the AO channel
                builder.WithBlend(blendDesc);
            });
        m_BlurPassMaterial = Material::Create(shader);
    }

    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::default_random_engine generator;

    SSAOUtils::GenerateRandomSamples(SsaoPassRootParameters::SSAOCBuffer::SAMPLES_COUNT, m_Samples);

    m_NoiseTexture = SSAOUtils::GenerateNoiseTexture(commandList);
}

void Ssao::Resize(uint32_t width, uint32_t height)
{
    if (m_Downsample)
    {
        width >>= 1;
        height >>= 1;
    }

    m_RenderTarget.Resize(width, height);
}

void Ssao::SsaoPass(CommandList& commandList, const Texture& gBufferNormals, const Texture& gBufferDepth, const D3D12_SHADER_RESOURCE_VIEW_DESC* gBufferDepthSrvDesc /*= nullptr*/, float radius /*= 0.5f*/, float power /*= 1.0f*/)
{
    PIXScope(commandList, "SSAO Pass");

    const auto& ssaoTexture = m_RenderTarget.GetTexture(Color0);
    const auto ssaoTextureDesc = ssaoTexture->GetD3D12ResourceDesc();
    const auto noiseTextureDesc = m_NoiseTexture->GetD3D12ResourceDesc();

    m_SsaoPassMaterial->SetVariable<XMFLOAT2>("NoiseScale", {
        static_cast<float>(ssaoTextureDesc.Width) / static_cast<float>(noiseTextureDesc.Width),
        static_cast<float>(ssaoTextureDesc.Height) / static_cast<float>(noiseTextureDesc.Height),
        });
    m_SsaoPassMaterial->SetVariable<float>("Radius", radius);
    m_SsaoPassMaterial->SetVariable<uint32_t>("KernelSize", SsaoPassRootParameters::SSAOCBuffer::SAMPLES_COUNT);
    m_SsaoPassMaterial->SetVariable<float>("Power", power);
    m_SsaoPassMaterial->SetVariable("Samples", m_Samples.size() * sizeof(XMFLOAT4), m_Samples.data());

    m_SsaoPassMaterial->SetShaderResourceView("noiseTexture", ShaderResourceView(m_NoiseTexture));

    commandList.SetRenderTarget(m_RenderTarget);
    commandList.SetAutomaticViewportAndScissorRect(m_RenderTarget);

    m_SsaoPassMaterial->Bind(commandList);
    m_BlitMesh->Draw(commandList);
    m_SsaoPassMaterial->Unbind(commandList);
}

void Ssao::BlurPass(CommandList& commandList, const RenderTarget& surfaceRenderTarget)
{
    PIXScope(commandList, "Blur Pass");

    const auto& ssaoTexture = m_RenderTarget.GetTexture(Color0);
    const auto ssaoTextureDesc = ssaoTexture->GetD3D12ResourceDesc();
    m_BlurPassMaterial->SetVariable<XMFLOAT2>("TexelSize",
        {
        1.0f / static_cast<float>(ssaoTextureDesc.Width),
        1.0f / static_cast<float>(ssaoTextureDesc.Height),
        }
    );
    m_BlurPassMaterial->SetShaderResourceView("ssaoTexture", ShaderResourceView(ssaoTexture));

    commandList.SetRenderTarget(surfaceRenderTarget);
    commandList.SetAutomaticViewportAndScissorRect(surfaceRenderTarget);

    m_BlurPassMaterial->Bind(commandList);
    m_BlitMesh->Draw(commandList);
    m_BlurPassMaterial->Unbind(commandList);
}

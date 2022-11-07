#include "SSAOUtils.h"

using namespace DirectX;

namespace
{
    float LerpUnclamped(float a, float b, float t)
    {
        return a + t * (b - a);
    }
}

void SSAOUtils::GenerateRandomSamples(size_t samplesCount, std::vector<DirectX::XMFLOAT4>& samplesDest)
{
    samplesDest.reserve(samplesCount);

    for (size_t i = 0; i < samplesCount; ++i)
    {
        auto sample = XMVectorSet(
            s_RandomFloats(s_Generator) * 2.0f - 1.0f,
            s_RandomFloats(s_Generator) * 2.0f - 1.0f,
            s_RandomFloats(s_Generator),
            0.0f
        );
        sample = XMVector3Normalize(sample);
        sample *= s_RandomFloats(s_Generator);

        float scale = static_cast<float>(i) / static_cast<float>(samplesCount);
        scale = LerpUnclamped(0.1f, 1.0f, scale * scale);
        sample *= scale;

        XMFLOAT4 sampleF4{};
        XMStoreFloat4(&sampleF4, sample);
        samplesDest.push_back(sampleF4);
    }
}

std::shared_ptr<Texture> SSAOUtils::GenerateNoiseTexture(CommandList& commandList)
{
    constexpr uint32_t noiseTextureWidth = 4;
    constexpr uint32_t noiseTextureHeight = 4;
    constexpr auto totalNoiseSamples = noiseTextureWidth * noiseTextureHeight;

    std::vector<XMFLOAT2> noiseSamples;
    noiseSamples.reserve(totalNoiseSamples);

    for (uint32_t i = 0; i < totalNoiseSamples; ++i)
    {
        XMFLOAT2 noiseSample = {
            s_RandomFloats(s_Generator) * 2.0f - 1.0f,
            s_RandomFloats(s_Generator) * 2.0f - 1.0f
        };
        noiseSamples.push_back(noiseSample);
    }

    auto noiseTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32_FLOAT, noiseTextureWidth, noiseTextureHeight, 1, 1);
    auto noiseTexture = std::make_shared<Texture>(noiseTextureDesc, nullptr, TextureUsageType::Other, L"SSAO Noise Texture");
    D3D12_SUBRESOURCE_DATA subresourceData{};
    subresourceData.pData = noiseSamples.data();
    subresourceData.RowPitch = sizeof(XMFLOAT2) * noiseTextureWidth;
    commandList.CopyTextureSubresource(*noiseTexture, 0, 1, &subresourceData);

    return noiseTexture;
}

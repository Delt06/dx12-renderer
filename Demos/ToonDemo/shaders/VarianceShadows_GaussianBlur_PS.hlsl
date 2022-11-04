#include <ShaderLibrary/Common/RootSignature.hlsli>

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

const static uint BlurKernelSize = 5;

const static float BlurOffsets[BlurKernelSize] =
{
    -3.2307692308f, -1.3846153846f,
    0.0f,
    1.3846153846f, 3.2307692308f
};

const static float BlurWeights[BlurKernelSize] =
{
    0.0702702703f, 0.3162162162f,
    0.2270270270f,
    0.3162162162f, 0.0702702703f
};

cbuffer Parameters : register(b0)
{
    float2 texelSize;
    float2 direction;
};

Texture2D<float2> sourceTexture : register(t0);

float2 main(PixelShaderInput IN) : SV_TARGET
{
    float2 value = 0;

    for (uint i = 0; i < BlurKernelSize; ++i)
    {
        float2 uv = IN.UV + direction * BlurOffsets[i] * texelSize;
        value += sourceTexture.Sample(g_Common_PointClampSampler, uv) * BlurWeights[i];
    }

    return value;
}

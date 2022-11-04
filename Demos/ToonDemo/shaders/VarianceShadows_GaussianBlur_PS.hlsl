#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "ShaderLibrary/Gaussian.hlsli"

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
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

    for (int i = 0; i < GaussianFilterSize; ++i)
    {
        float2 uv = IN.UV + direction * (i - GaussianIndexOffset) * texelSize;
        value += sourceTexture.Sample(g_Common_PointClampSampler, uv) * GaussianCoefficients[i];
    }

    return value;
}

struct PixelShaderInput
{
    float2 Uv : TEXCOORD;
};

Texture2D sourceColorTexture : register(t0);
SamplerState sourceSampler : register(s0);

#include <ShaderLibrary/Blur.hlsli>

struct Parameters
{
    float2 TexelSize;
    float Intensity;
    float Padding;
};

ConstantBuffer<Parameters> parametersCb : register(b0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    return float4(BoxBlur(sourceColorTexture, sourceSampler, IN.Uv, 1.0f, parametersCb.TexelSize), 1.0);
}
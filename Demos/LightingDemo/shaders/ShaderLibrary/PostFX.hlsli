#ifndef POST_FX_HLSLI
#define POST_FX_HLSLI

#include <ShaderLibrary/Blur.hlsli>

struct PixelShaderInput
{
    float2 Uv : TEXCOORD;
};

Texture2D sourceColorTexture : register(t0);
SamplerState sourceSampler : register(s0);

float3 SampleBox(float2 uv, float delta, float2 texelSize)
{
    return BoxBlur(sourceColorTexture, sourceSampler, uv, delta, texelSize);
}

#endif
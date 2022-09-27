#ifndef POST_FX_HLSLI
#define POST_FX_HLSLI

struct PixelShaderInput
{
    float2 Uv : TEXCOORD;
};

Texture2D sourceColorTexture : register(t0);
SamplerState sourceSampler : register(s0);

float4 SampleSourceTexture(float2 uv)
{
    return sourceColorTexture.Sample(sourceSampler, uv);
}

float3 SampleBox(float2 uv, float delta, float2 texelSize)
{
    float4 offset = texelSize.xyxy * float2(-delta, delta).xxyy;
    float3 samples = SampleSourceTexture(uv + offset.xy).rgb + SampleSourceTexture(uv + offset.zy).rgb +
			SampleSourceTexture(uv + offset.xw).rgb + SampleSourceTexture(uv + offset.zw).rgb;
    return samples * 0.25f;
}

#endif
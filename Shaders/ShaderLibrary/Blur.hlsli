#ifndef BLUR_HLSLI
#define BLUR_HLSLI

float3 BoxBlur(Texture2D sourceTexture, SamplerState textureSampler, float2 uv, float delta, float2 texelSize)
{
    float4 offset = texelSize.xyxy * float2(-delta, delta).xxyy;
    float3 samples = sourceTexture.Sample(textureSampler, uv + offset.xy).rgb + sourceTexture.Sample(textureSampler, uv + offset.zy).rgb +
			sourceTexture.Sample(textureSampler, uv + offset.xw).rgb + sourceTexture.Sample(textureSampler, uv + offset.zw).rgb;
    return samples * 0.25f;
}

#endif
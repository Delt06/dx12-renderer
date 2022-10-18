#include <ShaderLibrary/Math.hlsli>
#include <ShaderLibrary/HDR.hlsli>
#include <ShaderLibrary/Common/RootSignature.hlsli>

struct PixelShaderInput
{
    float2 Uv : TEXCOORD;
};

Texture2D sourceColorTexture : register(t0);

cbuffer PrefilterParameters : register(b0)
{
    float4 Filter;
    float2 TexelSize;
    float Intensity;
    float Padding;
};

float3 Prefilter(float3 c)
{
    float brightness = max(c.r, max(c.g, c.b));
    half soft = brightness - Filter.y;
    soft = clamp(soft, 0, Filter.z);
    soft = soft * soft * Filter.w;
    half contribution = max(soft, brightness - Filter.x);
    contribution /= max(brightness, 0.00001);
    return c * contribution;
}

// https://catlikecoding.com/unity/tutorials/custom-srp/hdr/
float3 CrossFilter(float2 uv)
{
    float3 totalColor = 0;
    float weightSum = 0;
    
    float2 offsets[] =
    {
        float2(0.0, 0.0),
		float2(-1.0, -1.0), float2(-1.0, 1.0), float2(1.0, -1.0), float2(1.0, 1.0)
    };
    
    for (uint i = 0; i < 5; ++i)
    {
        float3 color = sourceColorTexture.Sample(g_Common_LinearClampSampler, uv + offsets[i] * TexelSize * 2.0).rgb;
        color = Prefilter(color);
        float weight = 1.0 / (GetLuminance(color) + 1.0);
        totalColor += color * weight;
        weightSum += weight;
    }

    totalColor /= weightSum;
    return totalColor;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    const float3 resultColor = CrossFilter(IN.Uv);
    if (IsNaNAny(resultColor))
        return 0;
    
    return float4(resultColor * Intensity, 1);
}
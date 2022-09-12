#include "PostFx.hlsli"


struct PrefilterParameters
{
    float4 Filter;
    float2 TexelSize;
    float Intensity;
    float Padding;
};

ConstantBuffer<PrefilterParameters> parametersCb : register(b0);

float3 Prefilter(float3 c)
{
    float brightness = max(c.r, max(c.g, c.b));
    half soft = brightness - parametersCb.Filter.y;
    soft = clamp(soft, 0, parametersCb.Filter.z);
    soft = soft * soft * parametersCb.Filter.w;
    half contribution = max(soft, brightness - parametersCb.Filter.x);
    contribution /= max(brightness, 0.00001);
    return c * contribution;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    const float3 srcColor = SampleBox(IN.Uv, 0.5, parametersCb.TexelSize);
    const float3 resultColor = Prefilter(srcColor);
    return float4(resultColor * parametersCb.Intensity, 1);
}
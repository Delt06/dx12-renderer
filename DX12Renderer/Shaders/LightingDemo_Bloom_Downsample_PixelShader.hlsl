#include "PostFx.hlsli"

struct Parameters
{
    float2 TexelSize;
    float Intensity;
    float Padding;
};

ConstantBuffer<Parameters> parametersCb : register(b0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    return float4(SampleBox(IN.Uv, 0.5f, parametersCb.TexelSize) * parametersCb.Intensity, 1.0);
}
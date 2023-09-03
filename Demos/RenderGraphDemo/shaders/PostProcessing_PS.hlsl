#include <ShaderLibrary/Common/RootSignature.hlsli>

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

Texture2D source : register(t0);

#define SOURCE_SAMPLER g_Common_LinearClampSampler

cbuffer CBuffer : register(b0)
{
    float2 TexelSize;
    float Time;
    float Padding;
};

float2 Rotate2D(float2 vec, float angle){
    float2x2 m = float2x2(
        cos(angle), -sin(angle),
        sin(angle), cos(angle)
        );
	return mul(m, vec);
}

float4 main(PixelShaderInput IN) : SV_Target
{
    float2 uv = IN.UV;
    const float rotationAngle = Time * 10;
    const float2 offset = Rotate2D(TexelSize, rotationAngle);
    const float r = source.Sample(SOURCE_SAMPLER, uv + offset * 5.0f).r;
    const float g = source.Sample(SOURCE_SAMPLER, uv + offset * -2.0f).g;
    const float b = source.Sample(SOURCE_SAMPLER, uv + float2(offset.x * -0.5f, offset.y * 3.0f)).b;
    return float4(r, g, b, 1);
}

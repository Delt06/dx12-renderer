#include <ShaderLibrary/Common/RootSignature.hlsli>
#include "ShaderLibrary/ColorSplit.hlsli"

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

Texture2D _Source : register(t0);

#define SOURCE_SAMPLER g_Common_LinearClampSampler

StructuredBuffer<ColorSplitBufferEntry> _ColorSplitBuffer : register(t1);
ByteAddressBuffer _ColorSplitBufferCounter : register(t2);

cbuffer CBuffer : register(b0)
{
    float2 _TexelSize;
    float _Time;
    float _Padding;
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
    const float rotationAngle = _Time * 10;
    const float2 offset = Rotate2D(_TexelSize, rotationAngle);
    float3 accumulatedColor = 0;

    uint count = _ColorSplitBufferCounter.Load(0);

    for (uint i = 0; i < count; ++i)
    {
        ColorSplitBufferEntry entry = _ColorSplitBuffer[i];
        const float3 sourceSample = _Source.Sample(SOURCE_SAMPLER, uv + offset * entry.UvOffset).rgb;
        accumulatedColor += sourceSample * entry.ColorMask;
    }

    return float4(accumulatedColor, 1);
}

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

#define SAMPLES_COUNT 64

cbuffer SSAOCBuffer : register(b0)
{
    float2 NoiseScale;
    float2 _Padding;
    
    matrix Projection;
    
    float3 Samples[SAMPLES_COUNT];
};

Texture2D gBufferNormal : register(t0);
Texture2D gBufferDepth : register(t1);
Texture2D noiseTexture : register(t2);

SamplerState pointSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
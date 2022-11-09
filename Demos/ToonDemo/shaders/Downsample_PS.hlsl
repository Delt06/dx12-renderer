#include <ShaderLibrary/Common/RootSignature.hlsli>

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

Texture2D sourceTexture : register(t0);

#include <ShaderLibrary/Blur.hlsli>

cbuffer Parameters : register(b0)
{
    float2 TexelSize;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    return float4(BoxBlur(sourceTexture, g_Common_LinearClampSampler, IN.UV, 1.0f, TexelSize), 1.0);
}

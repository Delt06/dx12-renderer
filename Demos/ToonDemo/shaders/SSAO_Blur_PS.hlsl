#include <ShaderLibrary/Common/RootSignature.hlsli>

#define BLUR_KERNEL_HALF_SIZE 3
#define BLUR_KERNEL_SIZE (BLUR_KERNEL_HALF_SIZE * 2)

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

cbuffer BlurCBuffer : register(b0)
{
    float2 TexelSize;
};

Texture2D ssaoTexture : register(t0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    // https://learnopengl.com/Advanced-Lighting/SSAO

    float2 uv = IN.UV;
    float result = 0.0;

    for (int x = -BLUR_KERNEL_HALF_SIZE; x < BLUR_KERNEL_HALF_SIZE; ++x)
    {
        for (int y = -BLUR_KERNEL_HALF_SIZE; y < BLUR_KERNEL_HALF_SIZE; ++y)
        {
            float2 offset = float2(x, y) * TexelSize;
            result += ssaoTexture.Sample(g_Common_PointClampSampler, uv + offset).r;
        }
    }

    result /= BLUR_KERNEL_SIZE * BLUR_KERNEL_SIZE;
    return result;

}

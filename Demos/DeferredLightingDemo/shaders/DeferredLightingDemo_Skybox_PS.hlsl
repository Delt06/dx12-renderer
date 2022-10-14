#include <ShaderLibrary/Common/RootSignature.hlsli>

TextureCube skybox : register(t0);

struct PixelShaderInput
{
    float3 CubemapUV : CUBEMAP_UV;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    return skybox.Sample(g_Common_LinearClampSampler, IN.CubemapUV);
}
#include <ShaderLibrary/Common/RootSignature.hlsli>

struct PixelShaderInput
{
    float3 NormalWs : NORMAL;
    float2 Uv : TEXCOORD;
};

Texture2D diffuseMap : register(t1);


float4 main(PixelShaderInput IN) : SV_Target
{
    return diffuseMap.Sample(g_Common_LinearWrapSampler, IN.Uv);
}

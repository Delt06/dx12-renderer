#include "ShaderLibrary/Normals.hlsli"

struct PixelShaderInput
{
    float3 NormalWS : NORMAL;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    return float4(PackNormal(normalize(IN.NormalWS)), 1.0f);
}

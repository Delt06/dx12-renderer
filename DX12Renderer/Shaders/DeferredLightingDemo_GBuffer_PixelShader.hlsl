
struct PixelShaderInput
{
    float3 NormalWs : NORMAL;
    float3 TangentWs : TANGENT;
    float3 BitangentWs : BINORMAL;
    float2 Uv : TEXCOORD0;
};

struct PixelShaderOutput
{
    float4 DiffuseColor : SV_TARGET0;
    float4 Normal : SV_TARGET1;
};

#include <ShaderLibrary/Material.hlsli>

ConstantBuffer<Material> materialCB : register(b1);

Texture2D diffuseMap : register(t0);
Texture2D normalMap : register(t1);
SamplerState defaultSampler : register(s0);

#include <ShaderLibrary/GBufferUtils.hlsli>

float3 ApplyNormalMap(const float3x3 tbn, Texture2D map, const float2 uv)
{
    const float3 normalTs = UnpackNormal(map.Sample(defaultSampler, uv).xyz);
    return normalize(mul(normalTs, tbn));
}

PixelShaderOutput main(PixelShaderInput IN)
{
    PixelShaderOutput OUT;
    
    float2 uv = ApplyTilingOffset(materialCB, IN.Uv);
    
    if (materialCB.HasDiffuseMap)
    {
        OUT.DiffuseColor = diffuseMap.Sample(defaultSampler, uv);
    }
    else
    {
        OUT.DiffuseColor = 1;
    }
    
    float3 normal = normalize(IN.NormalWs);
    
    if (materialCB.HasNormalMap)
    {
        const float3 tangent = normalize(IN.TangentWs);
        const float3 bitangent = normalize(IN.BitangentWs);

        const float3x3 tbn = float3x3(tangent,
		                              bitangent,
		                              normal);
        normal = ApplyNormalMap(tbn, normalMap, uv);
    }
    
    OUT.Normal = float4(PackNormal(normal), 0);
    
    return OUT;
}

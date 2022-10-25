#include <ShaderLibrary/Common/RootSignature.hlsli>

struct PixelShaderInput
{
    float3 NormalWs : NORMAL;
    float3 TangentWs : TANGENT;
    float3 BitangentWs : BINORMAL;
    float2 Uv : TEXCOORD0;
    float4 CurrentPositionCs : TAA_CURRENT_POSITION;
    float4 PrevPositionCs : TAA_PREV_POSITION;
};

struct PixelShaderOutput
{
    float4 DiffuseColor : SV_TARGET0;
    float4 Normal : SV_TARGET1;
    float4 Surface : SV_TARGET2; // metallic, roughtness, ambient occlusion
    float4 Velocity : SV_TARGET3;
};


cbuffer Material : register(b0)
{
    float4 Diffuse = { 1, 1, 1, 1 };

    float Metallic = 1;
    float Roughness = 1;

    bool has_diffuseMap;
    bool has_normalMap;

    bool has_metallicMap;
    bool has_roughnessMap;
    bool has_ambientOcclusionMap;
    float Emission;

    float4 TilingOffset = { 1, 1, 0, 0 };
};

Texture2D diffuseMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D metallicMap : register(t2);
Texture2D roughnessMap : register(t3);
Texture2D ambientOcclusionMap : register(t4);

#include "ShaderLibrary/GBufferUtils.hlsli"

float3 ApplyNormalMap(const float3x3 tbn, Texture2D map, const float2 uv)
{
    const float3 normalTs = UnpackNormal(map.Sample(g_Common_LinearWrapSampler, uv).xyz);
    return normalize(mul(normalTs, tbn));
}

float4 HClipToScreenSpaceUV(float4 positionCS)
{
    positionCS /= positionCS.w;
    positionCS.xy = (positionCS.xy + 1) * 0.5f;
    positionCS.y = 1 - positionCS.y;
    return positionCS;
}

float2 CalculateVelocity(float4 newPositionCS, float4 oldPositionCS)
{
    return (HClipToScreenSpaceUV(newPositionCS) - HClipToScreenSpaceUV(oldPositionCS)).xy;
}

PixelShaderOutput main(PixelShaderInput IN)
{
    PixelShaderOutput OUT;
    
    float2 uv = TilingOffset.xy * IN.Uv + TilingOffset.zw;
    
    float3 diffuseColor;
    
    if (has_diffuseMap)
    {
        diffuseColor = diffuseMap.Sample(g_Common_LinearWrapSampler, uv).rgb;
    }
    else
    {
        diffuseColor = 1;
    }
    
    diffuseColor *= Diffuse.rgb;
    OUT.DiffuseColor = float4(diffuseColor, Emission);
    
    float3 normal = normalize(IN.NormalWs);
    
    if (has_normalMap)
    {
        const float3 tangent = normalize(IN.TangentWs);
        const float3 bitangent = normalize(IN.BitangentWs);

        const float3x3 tbn = float3x3(tangent,
		                              bitangent,
		                              normal);
        normal = ApplyNormalMap(tbn, normalMap, uv);
    }
    
    OUT.Normal = float4(PackNormal(normal), 0);
    
    float metallic = Metallic;
    if (has_metallicMap)
    {
        metallic *= metallicMap.Sample(g_Common_LinearWrapSampler, uv).r;
    }
    
    float roughness = Roughness;
    if (has_roughnessMap)
    {
        roughness *= roughnessMap.Sample(g_Common_LinearWrapSampler, uv).r;
    }
    
    float ambientOcclusion = 1.0f;
    if (has_ambientOcclusionMap)
    {
        ambientOcclusion *= ambientOcclusionMap.Sample(g_Common_LinearWrapSampler, uv).r;
    }
    
    OUT.Surface = PackSurface(metallic, roughness, ambientOcclusion);
    OUT.Velocity = float4(CalculateVelocity(IN.CurrentPositionCs, IN.PrevPositionCs), 0, 0);
    
    return OUT;
}

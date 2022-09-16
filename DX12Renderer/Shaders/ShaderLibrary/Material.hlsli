#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI

struct Material
{
    float4 Emissive;
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;

    float SpecularPower;
    float Reflectivity;
    float2 Padding;

    bool HasDiffuseMap;
    bool HasNormalMap;
    bool HasSpecularMap;
    bool HasGlossMap;
    
    float4 TilingOffset;
};

float2 ApplyTilingOffset(const in Material material, const float2 uv)
{
    return uv * material.TilingOffset.xy + material.TilingOffset.zw;
}

#endif
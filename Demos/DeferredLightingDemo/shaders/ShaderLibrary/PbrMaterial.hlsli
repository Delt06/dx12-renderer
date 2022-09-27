#ifndef PBR_MATERIAL_HLSLI
#define PBR_MATERIAL_HLSLI

struct PbrMaterial
{
    float4 Diffuse;

    float Metallic;
    float Roughness;

    bool HasDiffuseMap;
    bool HasNormalMap;

    bool HasMetallicMap;
    bool HasRoughnessMap;
    bool HasAmbientOcclusionMap;
    float Emission;

    float4 TilingOffset;
};

float2 ApplyTilingOffset(const in PbrMaterial material, const float2 uv)
{
    return uv * material.TilingOffset.xy + material.TilingOffset.zw;
}

#endif
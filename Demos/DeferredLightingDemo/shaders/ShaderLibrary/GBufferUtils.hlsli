#ifndef GBUFFER_UTILS_HLSLI
#define GBUFFER_UTILS_HLSLI

float3 UnpackNormal(const float3 n)
{
    return n * 2.0f - 1.0f;
}

float3 PackNormal(const float3 n)
{
    return (n + 1.0f) * 0.5f;
}

float4 RestorePositionVS(float3 positionNDC, matrix inverseProjection)
{
    float4 positionVS = mul(inverseProjection, float4(positionNDC, 1.0));
    positionVS /= positionVS.w;
    return positionVS;
}

float3 RestorePositionWS(float3 positionNDC, matrix inverseProjection, matrix inverseView)
{
    float4 positionVS = RestorePositionVS(positionNDC, inverseProjection);
    return mul(inverseView, positionVS).xyz;
}

float4 PackSurface(float metallic, float roughness, float ambientOcclusion)
{
    return float4(metallic, roughness, ambientOcclusion, 0.0f);
}

void UnpackSurface(float4 surface, out float metallic, out float roughness, out float ambientOcclusion)
{
    metallic = surface.x;
    roughness = surface.y;
    ambientOcclusion = surface.z;
}

#endif
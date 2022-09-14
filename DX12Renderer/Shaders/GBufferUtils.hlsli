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

#endif
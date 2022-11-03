#ifndef NORMALS_HLSLI
#define NORMALS_HLSLI

float3 PackNormal(float3 normal)
{
    return (normal + 1) * 0.5;
}

float3 UnpackNormal(float3 normal)
{
    return normal * 2 - 1;
}

#endif

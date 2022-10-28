#ifndef MATH_HLSLI
#define MATH_HLSLI

#define PI 3.14159265359f

bool IsNaN(float x)
{
    return (asuint(x) & 0x7fffffff) > 0x7f800000;
}

bool IsNaNAny(float3 v)
{
    return IsNaN(v.x) || IsNaN(v.y) || IsNaN(v.z);
}

#endif

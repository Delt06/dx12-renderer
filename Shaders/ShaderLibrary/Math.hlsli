#ifndef MATH_HLSLI
#define MATH_HLSLI

#define PI 3.14159265359f

bool IsNaN(float x)
{
    return isnan(x) || !(x < 0.f || x > 0.f || x == 0.f);
}

bool IsNaNAny(float3 v)
{
    return IsNaN(v.x) || IsNaN(v.y) || IsNaN(v.z);
}

#endif
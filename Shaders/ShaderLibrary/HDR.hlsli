#ifndef HDR_HLSLI
#define HDR_HLSLI

float GetLuminance(float3 color)
{
    return dot(color, float3(0.2127f, 0.7152f, 0.0722f));
}

#endif
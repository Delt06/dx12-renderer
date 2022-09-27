#ifndef CAPSULE_LIGHT_HLSLI
#define CAPSULE_LIGHT_HLSLI

struct CapsuleLight
{
    float4 PointA;
    float4 PointB;
    float4 Color;
    float Attenuation;
    float3 _Padding;
};

float GetCapsuleLightDistanceAttenuation(const float attenuation, const float distance)
{
    return 1.0f / (1.0f + attenuation * distance * distance);
}

#endif
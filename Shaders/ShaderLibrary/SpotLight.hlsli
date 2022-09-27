#ifndef SPOT_LIGHT_HLSLI
#define SPOT_LIGHT_HLSLI

struct SpotLight
{
    float4 PositionWS;
    float4 DirectionWS;
    float4 Color;
    float Intensity;
    float SpotAngle;
    float Attenuation;
    float _Padding;
};

float GetSpotLightDistanceAttenuation(const float attenuation, const float distance)
{
    return 1.0f / (1.0f + attenuation * distance * distance);
}

float GetSpotLightConeAttenuation(const float3 lightDirection, const float3 directionTowardsLight,
                                  const float spotAngle)
{
    const float minCos = cos(spotAngle);
    const float maxCos = (minCos + 1.0f) / 2.0f;
    const float cosAngle = dot(lightDirection, -directionTowardsLight);
    return smoothstep(minCos, maxCos, cosAngle);
}

#endif
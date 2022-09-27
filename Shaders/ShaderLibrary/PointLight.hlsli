#ifndef POINT_LIGHT_HLSLI
#define POINT_LIGHT_HLSLI

struct PointLight
{
    float4 PositionWS;
    float4 Color;
    float ConstantAttenuation;
    float LinearAttenuation;
    float QuadraticAttenuation;
    float Range;
};

float ComputeDistanceAttenuation(const float cConstant, const float cLinear, const float cQuadratic,
                                 const float distance)
{
    return 1.0f / (cConstant + cLinear * distance + cQuadratic * distance * distance);
}

#endif
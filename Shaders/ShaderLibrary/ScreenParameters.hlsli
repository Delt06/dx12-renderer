#ifndef SCREEN_PARAMETERS_HLSLI
#define SCREEN_PARAMETERS_HLSLI

struct ScreenParameters
{
    float2 ScreenSize;
    float2 OneOverScreenSize;
};

float2 ToScreenSpaceUV(const float4 positionCS, const ScreenParameters screenParameters)
{
    return positionCS.xy * screenParameters.OneOverScreenSize;
}

float3 ScreenSpaceUVToNDC(float2 uv, float zNDC)
{
    float3 positionNDC;
    positionNDC.xy = (uv * 2) - 1;
    positionNDC.y *= -1;
    positionNDC.z = zNDC;
    return positionNDC;
}

#endif
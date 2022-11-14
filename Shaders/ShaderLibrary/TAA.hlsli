#ifndef TAA_HLSLI
#define TAA_HLSLI

static float4 HClipToScreenSpaceUV(float4 positionCS)
{
    positionCS /= positionCS.w;
    positionCS.xy = (positionCS.xy + 1) * 0.5f;
    positionCS.y = 1 - positionCS.y;
    return positionCS;
}

float2 CalculateVelocity(float4 newPositionCS, float4 oldPositionCS)
{
    return (HClipToScreenSpaceUV(newPositionCS) - HClipToScreenSpaceUV(oldPositionCS)).xy;
}

#endif

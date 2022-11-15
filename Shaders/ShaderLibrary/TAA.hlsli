#ifndef TAA_HLSLI
#define TAA_HLSLI

static float2 TAA_HClipToScreenSpaceUV2(float4 positionCS)
{
    positionCS.xy /= positionCS.w;
    positionCS.x = positionCS.x * 0.5f + 0.5f;
    positionCS.y = 0.5f - positionCS.y * 0.5f;
    return positionCS.xy;
}

float2 CalculateVelocity(float4 newPositionCS, float4 oldPositionCS)
{
    return TAA_HClipToScreenSpaceUV2(newPositionCS) - TAA_HClipToScreenSpaceUV2(oldPositionCS);
}

#endif

#ifndef SHADOWS_HLSLI
#define SHADOWS_HLSLI

struct ShadowReceiverParameters
{
	matrix ViewProjection;
	float PoissonSpreadInv;
	float PointLightPoissonSpreadInv;
	float2 Padding;
};

float4 HClipToShadowCoords(const float4 positionCs)
{
	float4 shadowCoords = positionCs;
	shadowCoords.xy = shadowCoords.xy * 0.5f + 0.5f; // [-1; 1] -> [0, 1]
	shadowCoords.y = 1 - shadowCoords.y;
	return shadowCoords;
}

#endif

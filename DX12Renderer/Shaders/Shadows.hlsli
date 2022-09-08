#ifndef SHADOWS_HLSLI
#define SHADOWS_HLSLI

struct ShadowReceiverParameters
{
	matrix ViewProjection;
	float PoissonSpreadInv;
	float3 Padding;
};

#endif


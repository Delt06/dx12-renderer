#ifndef POISSON_DISK_DEFINED

#define POISSON_DISK_DEFINED
#define POISSON_DISK_SIZE 16
#define POISSON_EARLY_BAIL_SIZE 4

static const float2 POISSON_DISK[] = {
	float2(0.94558609, -0.76890725),
	float2(-0.81544232, -0.87912464),
	float2(0.97484398, 0.75648379),
	float2(-0.91588581, 0.45771432),
	float2(-0.94201624, -0.39906216),
	float2(-0.094184101, -0.92938870),
	float2(0.34495938, 0.29387760),
	float2(-0.38277543, 0.27676845),
	float2(0.44323325, -0.97511554),
	float2(0.53742981, -0.47373420),
	float2(-0.26496911, -0.41893023),
	float2(0.79197514, 0.19090188),
	float2(-0.24188840, 0.99706507),
	float2(-0.81409955, 0.91437590),
	float2(0.19984126, 0.78641367),
	float2(0.14383161, -0.14100790)
};
#endif

#ifdef POISSON_SAMPLING_POINT_LIGHT
float PoissonSampling_PointLight(const float4 shadowCoords, const uint shadowSliceIndex)
#else
float PoissonSampling_MainLight(const float4 shadowCoords)
#endif
{
	if (any(shadowCoords.xyz < 0) || any(shadowCoords.xyz > 1))
	{
		return 1.0f;
	}

	float attenuation = 0.0f;
#ifdef POISSON_SAMPLING_POINT_LIGHT
	const float poissonSpreadInv = shadowReceiverParameters.PointLightPoissonSpreadInv;
#else
	const float poissonSpreadInv = shadowReceiverParameters.PoissonSpreadInv;
#endif

	for (uint i = 0; i < POISSON_DISK_SIZE; i++)
	{
		const float2 poissonDiskSample = POISSON_DISK[i];

		const float3 sampleShadowCoords = shadowCoords.xyz + float3(
			poissonDiskSample * poissonSpreadInv, 0);

#ifdef POISSON_SAMPLING_POINT_LIGHT
		attenuation += pointLightShadowMaps.SampleCmpLevelZero(shadowMapSampler, float3(sampleShadowCoords.xy, shadowSliceIndex),
			sampleShadowCoords.z).x;
#else
		attenuation += directionalLightShadowMap.SampleCmpLevelZero(
			shadowMapSampler, sampleShadowCoords.xy, sampleShadowCoords.z).x;
#endif


		if (i != POISSON_EARLY_BAIL_SIZE - 1)
		{
			continue;
		}

		// try early bail

		// all were in shadow
		if (attenuation == 0.0f)
		{
			break;
		}

		// none were in shadow
		if (attenuation == POISSON_EARLY_BAIL_SIZE)
		{
			attenuation = POISSON_DISK_SIZE;
			break;
		}

		// penumbra, thus continue
	}

	return attenuation / POISSON_DISK_SIZE;
}

#include "Shadows.hlsli"

struct PixelShaderInput
{
	float4 PositionVs : POSITION;
	float3 NormalVs : NORMAL;
	float3 TangentVs : TANGENT;
	float3 BitangentVs : BINORMAL;
	float2 Uv : TEXCOORD;
	float4 ShadowCoords : SHADOW_COORD;
};

struct Material
{
	float4 Emissive;
	float4 Ambient;
	float4 Diffuse;
	float4 Specular;

	float SpecularPower;
	float3 Padding;

	bool HasDiffuseMap;
	bool HasNormalMap;
	bool HasSpecularMap;
	bool HasGlossMap;
};

struct DirectionalLight
{
	float4 DirectionVs;
	float4 Color;
};

struct PointLight
{
	float4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	float4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	float4 Color;
	//----------------------------------- (16 byte boundary)
	float ConstantAttenuation;
	float LinearAttenuation;
	float QuadraticAttenuation;
	float Range;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 4 = 64 bytes
};

struct LightingResult
{
	float3 Diffuse;
	float3 Specular;
};

struct LightPropertiesCb
{
	uint NumPointLights;
};

ConstantBuffer<Material> materialCb : register(b0, space1);
ConstantBuffer<DirectionalLight> dirLightCb : register(b1, space1);
ConstantBuffer<LightPropertiesCb> lightPropertiesCb : register(b2);
StructuredBuffer<PointLight> pointLights : register(t4);

ConstantBuffer<ShadowReceiverParameters> shadowReceiverParameters : register(b3);

Texture2D directionalLightShadowMap : register(t5);
SamplerComparisonState shadowMapSampler : register(s1);

Texture2D diffuseMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D specularMap : register(t2);
Texture2D glossMap : register(t3);
SamplerState defaultSampler : register(s0);

struct Light
{
	float3 Color;
	float3 DirectionVs;
	float DistanceAttenuation;
	float ShadowAttenuation;
};

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

float SampleShadowMap(Texture2D shadowMapName, float3 shadowCoords)
{
	return shadowMapName.SampleCmpLevelZero(
		shadowMapSampler, shadowCoords.xy, shadowCoords.z).x;
}

Light GetMainLight(const float4 shadowCoords)
{
	Light light;
	light.Color = dirLightCb.Color.rgb;
	light.DirectionVs = dirLightCb.DirectionVs.xyz;
	light.DistanceAttenuation = 1.0f;

	if (any(shadowCoords.xyz < 0) || any(shadowCoords.xyz > 1))
	{
		light.ShadowAttenuation = 1.0f;
	}
	else
	{
		float attenuation = 0.0f;

		[unroll]
		for (int i = 0; i < POISSON_DISK_SIZE; i++)
		{
			const float2 poissonDiskSample = POISSON_DISK[i];
			const float3 sampleShadowCoords = shadowCoords.xyz + float3(
				poissonDiskSample * shadowReceiverParameters.PoissonSpreadInv, 0);
			attenuation += SampleShadowMap(directionalLightShadowMap, sampleShadowCoords);

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

			// penumbra
		}

		light.ShadowAttenuation = attenuation / POISSON_DISK_SIZE;
	}

	return light;
}

float ComputeAttenuation(const float cConstant, const float cLinear, const float cQuadratic, const float distance)
{
	return 1.0f / (cConstant + cLinear * distance + cQuadratic * distance * distance);
}

Light GetPointLight(const uint index, const float3 positionVs)
{
	Light light;

	const PointLight pointLight = pointLights[index];
	const float3 offset = pointLight.PositionVS.xyz - positionVs;
	const float distance = length(offset);

	light.Color = pointLight.Color.rgb;
	light.DirectionVs = offset / distance;
	light.DistanceAttenuation = ComputeAttenuation(pointLight.ConstantAttenuation, pointLight.LinearAttenuation,
	                                               pointLight.QuadraticAttenuation, distance);
	light.ShadowAttenuation = 1.0f; // TODO: implement point light shadows

	return light;
}

float3 UnpackNormal(const float3 n)
{
	return n * 2.0f - 1.0f;
}

float3 ApplyNormalMap(const float3x3 tbn, Texture2D map, const float2 uv)
{
	const float3 normalTs = UnpackNormal(map.Sample(defaultSampler, uv).xyz);
	return normalize(mul(normalTs, tbn));
}

float Diffuse(const float3 n, const float3 l)
{
	return max(0, dot(n, l));
}

float Specular(const float3 v, const float3 n, const float3 l, const float specularPower)
{
	const float3 r = normalize(reflect(-l, n));
	const float rDotV = max(0, dot(r, v));

	return pow(rDotV, specularPower);
}

LightingResult Phong(const in Light light, const float3 positionVs, const float3 normalVs, const float specularPower)
{
	LightingResult lightResult;

	const float3 viewDir = normalize(-positionVs);
	const float3 lightDir = light.DirectionVs.xyz;
	const float3 attenuatedColor = light.Color * light.DistanceAttenuation * light.ShadowAttenuation;

	lightResult.Diffuse = Diffuse(normalVs, lightDir) * attenuatedColor;
	lightResult.Specular = Specular(viewDir, normalVs, lightDir, specularPower) * attenuatedColor;

	return lightResult;
}

void Combine(inout LightingResult destination, const in LightingResult source)
{
	destination.Diffuse += source.Diffuse;
	destination.Specular += source.Specular;
}

LightingResult ComputeLighting(const float3 positionVs, const float3 normalVs, const float specularPower,
                               const float4 shadowCoords)
{
	LightingResult totalLightingResult;
	totalLightingResult.Diffuse = 0;
	totalLightingResult.Specular = 0;

	// main light
	{
		const Light light = GetMainLight(shadowCoords);
		const LightingResult lightingResult = Phong(light, positionVs, normalVs, specularPower);
		Combine(totalLightingResult, lightingResult);
	}

	// point lights
	for (uint i = 0; i < lightPropertiesCb.NumPointLights; ++i)
	{
		const Light light = GetPointLight(i, positionVs);
		const LightingResult lightingResult = Phong(light, positionVs, normalVs, specularPower);
		Combine(totalLightingResult, lightingResult);
	}

	return totalLightingResult;
}

float4 main(PixelShaderInput IN) : SV_Target
{
	float3 normalVs;
	if (materialCb.HasNormalMap)
	{
		const float3 tangent = normalize(IN.TangentVs);
		const float3 bitangent = normalize(IN.BitangentVs);
		const float3 normal = normalize(IN.NormalVs);

		const float3x3 tbn = float3x3(tangent,
		                              bitangent,
		                              normal);
		normalVs = ApplyNormalMap(tbn, normalMap, IN.Uv);
	}
	else
	{
		normalVs = normalize(IN.NormalVs);
	}


	float specularPower = materialCb.SpecularPower;
	if (materialCb.HasGlossMap)
	{
		specularPower *= saturate(1 - glossMap.Sample(defaultSampler, IN.Uv).x);
	}

	const LightingResult lightingResult = ComputeLighting(IN.PositionVs.xyz, normalVs, specularPower, IN.ShadowCoords);

	const float4 emissive = materialCb.Emissive;
	const float4 ambient = materialCb.Ambient;
	const float4 diffuse = materialCb.Diffuse * float4(lightingResult.Diffuse, 1);
	float4 specular = materialCb.Specular * float4(lightingResult.Specular, 1);
	if (materialCb.HasSpecularMap)
	{
		specular *= specularMap.Sample(defaultSampler, IN.Uv).x;
	}
	const float4 texColor = materialCb.HasDiffuseMap
		                        ? diffuseMap.Sample(defaultSampler, IN.Uv)
		                        : float4(1.0f, 1.0f, 1.0f, 1.0f);

	//return directionalLightShadowMap.SampleCmpLevelZero(shadowMapSampler, IN.ShadowCoords.xy, IN.ShadowCoords.z);
	return emissive + (ambient + diffuse + specular) * texColor;
}

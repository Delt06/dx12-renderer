#include "Shadows.hlsli"

struct PixelShaderInput
{
    float4 PositionVs : POSITION;
    float3 NormalVs : NORMAL;
    float3 TangentVs : TANGENT;
    float3 BitangentVs : BINORMAL;
    float2 Uv : TEXCOORD;
    float3 PositionWs : POSITION_WS;
    float4 ShadowCoords : SHADOW_COORD;
    float3 NormalWs : NORMAL_WS;
    float3 EyeWs : EYE_WS;
};

struct Material
{
    float4 Emissive;
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;

    float SpecularPower;
    float Reflectivity;
    float2 Padding;

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

struct SpotLight
{
    float4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
    float4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
    float4 DirectionWS; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
    float4 DirectionVS; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
    float4 Color;
	//----------------------------------- (16 byte boundary)
    float m_Intensity;
    float m_SpotAngle;
    float m_Attenuation;
    float m_Padding; // Pad to 16 bytes.
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 6 = 96 bytes
};

struct LightingResult
{
    float3 Diffuse;
    float3 Specular;
};

struct LightPropertiesCb
{
    uint NumPointLights;
    uint NumSpotLights;
};

ConstantBuffer<Material> materialCb : register(b0, space1);
ConstantBuffer<DirectionalLight> dirLightCb : register(b1, space1);
ConstantBuffer<LightPropertiesCb> lightPropertiesCb : register(b2);
StructuredBuffer<PointLight> pointLights : register(t4);
StructuredBuffer<SpotLight> spotLights : register(t8);

ConstantBuffer<ShadowReceiverParameters> shadowReceiverParameters : register(b3);

Texture2D directionalLightShadowMap : register(t5);
SamplerComparisonState shadowMapSampler : register(s1);
Texture2DArray pointLightShadowMaps : register(t6);
StructuredBuffer<matrix> pointLightViewProjectionMatrices : register(t7);
Texture2DArray spotLightShadowMaps : register(t9);
StructuredBuffer<matrix> spotLightLightViewProjectionMatrices : register(t10);

TextureCube envrironmentReflectionsCubemap : register(t11);

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

#include "ShadowsPoissonSampling.hlsli"

#define POISSON_SAMPLING_POINT_LIGHT
#include "ShadowsPoissonSampling.hlsli"
#undef POISSON_SAMPLING_POINT_LIGHT

#define POISSON_SAMPLING_SPOT_LIGHT
#include "ShadowsPoissonSampling.hlsli"
#undef POISSON_SAMPLING_SPOT_LIGHT

Light GetMainLight(const float4 shadowCoords)
{
    Light light;
    light.Color = dirLightCb.Color.rgb;
    light.DirectionVs = dirLightCb.DirectionVs.xyz;
    light.DistanceAttenuation = 1.0f;
    light.ShadowAttenuation = PoissonSampling_MainLight(shadowCoords);
    return light;
}

float ComputeDistanceAttenuation(const float cConstant, const float cLinear, const float cQuadratic,
                                 const float distance)
{
    return 1.0f / (cConstant + cLinear * distance + cQuadratic * distance * distance);
}

// Unity URP
// com.unity.render-pipelines.core@12.1.6\ShaderLibrary\Common.hlsl
#define CUBEMAPFACE_POSITIVE_X 0
#define CUBEMAPFACE_NEGATIVE_X 1
#define CUBEMAPFACE_POSITIVE_Y 2
#define CUBEMAPFACE_NEGATIVE_Y 3
#define CUBEMAPFACE_POSITIVE_Z 4
#define CUBEMAPFACE_NEGATIVE_Z 5

float CubeMapFaceId(float3 dir)
{
    float faceId;

    if (abs(dir.z) >= abs(dir.x) && abs(dir.z) >= abs(dir.y))
    {
        faceId = (dir.z < 0.0) ? CUBEMAPFACE_NEGATIVE_Z : CUBEMAPFACE_POSITIVE_Z;
    }
    else if (abs(dir.y) >= abs(dir.x))
    {
        faceId = (dir.y < 0.0) ? CUBEMAPFACE_NEGATIVE_Y : CUBEMAPFACE_POSITIVE_Y;
    }
    else
    {
        faceId = (dir.x < 0.0) ? CUBEMAPFACE_NEGATIVE_X : CUBEMAPFACE_POSITIVE_X;
    }

    return faceId;
}

float PointLightShadowAttenuation(const uint lightIndex, const float3 positionWs, const half3 lightDirection)
{
	// This is a point light, we have to find out which shadow slice to sample from
    const float cubemapFaceId = CubeMapFaceId(-lightDirection);
    const uint shadowSliceIndex = lightIndex * 6 + cubemapFaceId;

    const float4 shadowHClip = mul(pointLightViewProjectionMatrices[shadowSliceIndex],
	                               float4(positionWs, 1.0));
    const float4 shadowCoords = HClipToShadowCoords(shadowHClip, true);
    return PoissonSampling_PointLight(shadowCoords, shadowSliceIndex);
}

Light GetPointLight(const uint index, const float3 positionVs, const float3 positionWs)
{
    Light light;

    const PointLight pointLight = pointLights[index];
    const float3 offsetVs = pointLight.PositionVS.xyz - positionVs;
    const float distance = length(offsetVs);

    light.Color = pointLight.Color.rgb;
    light.DirectionVs = offsetVs / distance;
    light.DistanceAttenuation = ComputeDistanceAttenuation(pointLight.ConstantAttenuation, pointLight.LinearAttenuation,
	                                                       pointLight.QuadraticAttenuation, distance);
    const float3 directionWs = (pointLight.PositionWS.xyz - positionWs) / distance;
    light.ShadowAttenuation = PointLightShadowAttenuation(index, positionWs, directionWs);

    return light;
}

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

Light GetSpotLight(const uint index, const float3 positionVs, const float3 positionWs)
{
    Light light;

    const SpotLight spotLight = spotLights[index];
    const float3 offsetVs = spotLight.PositionVS.xyz - positionVs;
    const float distance = length(offsetVs);
    const float3 directionTowardsLightVs = offsetVs / distance;

    light.Color = spotLight.Color.rgb * spotLight.m_Intensity;
    light.DirectionVs = directionTowardsLightVs;

    const float attenuation = GetSpotLightDistanceAttenuation(spotLight.m_Attenuation, distance) *
		GetSpotLightConeAttenuation(spotLight.DirectionVS.xyz, directionTowardsLightVs, spotLight.m_SpotAngle);
    light.DistanceAttenuation = attenuation;

    const float4 shadowCoordsCs = mul(spotLightLightViewProjectionMatrices[index], float4(positionWs, 1.0f));
    const float4 shadowCoords = HClipToShadowCoords(shadowCoordsCs, true);
    light.ShadowAttenuation = PoissonSampling_SpotLight(shadowCoords, index);

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
                               const float4 shadowCoords, const float3 positionWs)
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
	{
        for (uint i = 0; i < lightPropertiesCb.NumPointLights; ++i)
        {
            const Light light = GetPointLight(i, positionVs, positionWs);
            const LightingResult lightingResult = Phong(light, positionVs, normalVs, specularPower);
            Combine(totalLightingResult, lightingResult);
        }
    }

	// spot lights
	{
        for (uint i = 0; i < lightPropertiesCb.NumSpotLights; ++i)
        {
            const Light light = GetSpotLight(i, positionVs, positionWs);
            const LightingResult lightingResult = Phong(light, positionVs, normalVs, specularPower);
            Combine(totalLightingResult, lightingResult);
        }
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

    const LightingResult lightingResult = ComputeLighting(IN.PositionVs.xyz, normalVs, specularPower, IN.ShadowCoords,
	                                                      IN.PositionWs);

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

    float3 reflectWs = reflect(IN.EyeWs, normalize(IN.NormalWs));
    float4 reflection = float4(envrironmentReflectionsCubemap.Sample(defaultSampler, reflectWs).rgb * materialCb.Reflectivity, 1);
    return emissive + (ambient + diffuse + specular + reflection) * texColor;
}

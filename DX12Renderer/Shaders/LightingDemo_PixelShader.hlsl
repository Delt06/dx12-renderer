#include "Shadows.hlsli"

struct PixelShaderInput
{
    float3 PositionWs : POSITION;
    float3 NormalWs : NORMAL;
    float3 TangentWs : TANGENT;
    float3 BitangentWs : BINORMAL;
    float2 Uv : TEXCOORD;
    float4 ShadowCoords : SHADOW_COORD;
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
    
    float4 TilingOffset;
};

struct DirectionalLight
{
    float4 DirectionWs; // update on CPU
    float4 Color;
};

#include <ShaderLibrary/PointLight.hlsli>

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
    float3 DirectionWs;
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
    light.DirectionWs = dirLightCb.DirectionWs.xyz;
    light.DistanceAttenuation = 1.0f;
    light.ShadowAttenuation = PoissonSampling_MainLight(shadowCoords);
    return light;
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

Light GetPointLight(const uint index, const float3 positionWs)
{
    Light light;

    const PointLight pointLight = pointLights[index];
    const float3 offsetWs = pointLight.PositionWS.xyz - positionWs;
    const float distance = length(offsetWs);

    light.Color = pointLight.Color.rgb;
    light.DirectionWs = offsetWs / distance;
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

Light GetSpotLight(const uint index, const float3 positionWs)
{
    Light light;

    const SpotLight spotLight = spotLights[index];
    const float3 offsetWs = spotLight.PositionWS.xyz - positionWs;
    const float distance = length(offsetWs);
    const float3 directionTowardsLightWs = offsetWs / distance;

    light.Color = spotLight.Color.rgb * spotLight.Intensity;
    light.DirectionWs = directionTowardsLightWs;

    const float attenuation = GetSpotLightDistanceAttenuation(spotLight.Attenuation, distance) *
		GetSpotLightConeAttenuation(spotLight.DirectionWS.xyz, directionTowardsLightWs, spotLight.SpotAngle);
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

LightingResult Phong(const in Light light, const float3 normalWs, const float3 eyeWs, const float specularPower)
{
    LightingResult lightResult;
    
    const float3 lightDir = light.DirectionWs.xyz;
    const float3 attenuatedColor = light.Color * light.DistanceAttenuation * light.ShadowAttenuation;

    lightResult.Diffuse = Diffuse(normalWs, lightDir) * attenuatedColor;
    lightResult.Specular = Specular(-eyeWs, normalWs, lightDir, specularPower) * attenuatedColor;

    return lightResult;
}

void Combine(inout LightingResult destination, const in LightingResult source)
{
    destination.Diffuse += source.Diffuse;
    destination.Specular += source.Specular;
}

LightingResult ComputeLighting(const float3 positionWs, const float3 normalWs, const float specularPower,
                               const float4 shadowCoords, const float3 eyeWs)
{
    LightingResult totalLightingResult;
    totalLightingResult.Diffuse = 0;
    totalLightingResult.Specular = 0;

	// main light
	{
        const Light light = GetMainLight(shadowCoords);
        const LightingResult lightingResult = Phong(light, normalWs, eyeWs, specularPower);
        Combine(totalLightingResult, lightingResult);
    }

	// point lights
	{
        for (uint i = 0; i < lightPropertiesCb.NumPointLights; ++i)
        {
            const Light light = GetPointLight(i, positionWs);
            const LightingResult lightingResult = Phong(light, normalWs, eyeWs, specularPower);
            Combine(totalLightingResult, lightingResult);
        }
    }

	// spot lights
	{
        for (uint i = 0; i < lightPropertiesCb.NumSpotLights; ++i)
        {
            const Light light = GetSpotLight(i, positionWs);
            const LightingResult lightingResult = Phong(light, normalWs, eyeWs, specularPower);
            Combine(totalLightingResult, lightingResult);
        }
    }


    return totalLightingResult;
}

float4 main(PixelShaderInput IN) : SV_Target
{
    float2 uv = IN.Uv * materialCb.TilingOffset.xy + materialCb.TilingOffset.zw;
    
    float3 normalWs;
    if (materialCb.HasNormalMap)
    {
        // move to WS
        const float3 tangent = normalize(IN.TangentWs);
        const float3 bitangent = normalize(IN.BitangentWs);
        const float3 normal = normalize(IN.NormalWs);

        const float3x3 tbn = float3x3(tangent,
		                              bitangent,
		                              normal);
        normalWs = ApplyNormalMap(tbn, normalMap, uv);
    }
    else
    {
        normalWs = normalize(IN.NormalWs);
    }


    float specularPower = materialCb.SpecularPower;
    if (materialCb.HasGlossMap)
    {
        specularPower *= saturate(1 - glossMap.Sample(defaultSampler, uv).x);
    }

    const float3 eyeWs = normalize(IN.EyeWs);
    const LightingResult lightingResult = ComputeLighting(IN.PositionWs, normalWs, specularPower, IN.ShadowCoords, eyeWs);

    const float4 emissive = materialCb.Emissive;
    const float4 ambient = materialCb.Ambient;
    const float4 diffuse = materialCb.Diffuse * float4(lightingResult.Diffuse, 1);
    float4 specular = materialCb.Specular * float4(lightingResult.Specular, 1);
    if (materialCb.HasSpecularMap)
    {
        specular *= specularMap.Sample(defaultSampler, uv).x;
    }
    const float4 texColor = materialCb.HasDiffuseMap
		                        ? diffuseMap.Sample(defaultSampler, uv)
		                        : float4(1.0f, 1.0f, 1.0f, 1.0f);

    float3 reflectWs = reflect(eyeWs, normalWs);
    float4 reflection = float4(envrironmentReflectionsCubemap.Sample(defaultSampler, reflectWs).rgb * materialCb.Reflectivity, 1);
    return emissive + (ambient + diffuse + specular + reflection) * texColor;
}

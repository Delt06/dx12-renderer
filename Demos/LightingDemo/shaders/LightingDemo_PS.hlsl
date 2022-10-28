#include <ShaderLibrary/Common/RootSignature.hlsli>

#include <ShaderLibrary/Shadows.hlsli>
#include "ShaderLibrary/Pipeline.hlsli"

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

#include <ShaderLibrary/PointLight.hlsli>
#include <ShaderLibrary/SpotLight.hlsli>

struct LightingResult
{
    float3 Diffuse;
    float3 Specular;
};

cbuffer MaterialCBuffer : register(b0)
{
    float4 Emissive = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float4 Ambient = float4(0.1f, 0.1f, 0.1f, 1.0f);
    float4 Diffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
    float4 Specular = float4(1.0f, 1.0f, 1.0f, 1.0f);

    float SpecularPower = 128.0f;
    float Reflectivity = 0.0f;
    float2 Padding;

    bool has_diffuseMap = false;
    bool has_normalMap = false;
    bool has_specularMap = false;
    bool has_glossMap = false;

    float4 TilingOffset = float4(1, 1, 0, 0);
};

Texture2D directionalLightShadowMap : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

StructuredBuffer<PointLight> pointLights : register(t1, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
Texture2DArray pointLightShadowMaps : register(t2, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
StructuredBuffer<matrix> pointLightViewProjectionMatrices : register(t3, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

StructuredBuffer<SpotLight> spotLights : register(t4, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
Texture2DArray spotLightShadowMaps : register(t5, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
StructuredBuffer<matrix> spotLightLightViewProjectionMatrices : register(t6, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

TextureCube envrironmentReflectionsCubemap : register(t7, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

Texture2D diffuseMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D specularMap : register(t2);
Texture2D glossMap : register(t3);

struct Light
{
    float3 Color;
    float3 DirectionWs;
    float DistanceAttenuation;
    float ShadowAttenuation;
};

#include "ShaderLibrary/ShadowsPoissonSampling.hlsli"

#define POISSON_SAMPLING_POINT_LIGHT
#include "ShaderLibrary/ShadowsPoissonSampling.hlsli"
#undef POISSON_SAMPLING_POINT_LIGHT

#define POISSON_SAMPLING_SPOT_LIGHT
#include "ShaderLibrary/ShadowsPoissonSampling.hlsli"
#undef POISSON_SAMPLING_SPOT_LIGHT

Light GetMainLight(const float4 shadowCoords)
{
    Light light;
    light.Color = g_Pipeline_DirectionalLight.Color.rgb;
    light.DirectionWs = g_Pipeline_DirectionalLight.DirectionWs.xyz;
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
    const float3 normalTs = UnpackNormal(map.Sample(g_Common_LinearWrapSampler, uv).xyz);
    return normalize(mul(normalTs, tbn));
}

float ComputeDiffuse(const float3 n, const float3 l)
{
    return max(0, dot(n, l));
}

float ComputeSpecular(const float3 v, const float3 n, const float3 l, const float specularPower)
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

    lightResult.Diffuse = ComputeDiffuse(normalWs, lightDir) * attenuatedColor;
    lightResult.Specular = ComputeSpecular(-eyeWs, normalWs, lightDir, specularPower) * attenuatedColor;

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
        for (uint i = 0; i < g_Pipeline_NumPointLights; ++i)
        {
            const Light light = GetPointLight(i, positionWs);
            const LightingResult lightingResult = Phong(light, normalWs, eyeWs, specularPower);
            Combine(totalLightingResult, lightingResult);
        }
    }

	// spot lights
	{
        for (uint i = 0; i < g_Pipeline_NumSpotLights; ++i)
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
    float2 uv = IN.Uv * TilingOffset.xy + TilingOffset.zw;

    float3 normalWs;
    if (has_normalMap)
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


    float specularPower = SpecularPower;
    if (has_glossMap)
    {
        specularPower *= saturate(1 - glossMap.Sample(g_Common_LinearWrapSampler, uv).x);
    }

    const float3 eyeWs = normalize(IN.EyeWs);
    const LightingResult lightingResult = ComputeLighting(IN.PositionWs, normalWs, specularPower, IN.ShadowCoords, eyeWs);

    const float4 emissive = Emissive;
    const float4 ambient = Ambient;
    const float4 diffuse = Diffuse * float4(lightingResult.Diffuse, 1);
    float4 specular = Specular * float4(lightingResult.Specular, 1);
    if (has_specularMap)
    {
        specular *= specularMap.Sample(g_Common_LinearWrapSampler, uv).x;
    }
    const float4 texColor = has_diffuseMap
		                        ? diffuseMap.Sample(g_Common_LinearWrapSampler, uv)
		                        : float4(1.0f, 1.0f, 1.0f, 1.0f);

    float3 reflectWs = reflect(eyeWs, normalWs);
    float4 reflection = float4(envrironmentReflectionsCubemap.Sample(g_Common_LinearWrapSampler, reflectWs).rgb * Reflectivity, 1);
    return emissive + (ambient + diffuse + specular + reflection) * texColor;
}

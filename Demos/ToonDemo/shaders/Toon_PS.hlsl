#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "ShaderLibrary/Pipeline.hlsli"
#include "ToonVaryings.hlsli"

cbuffer Material : register(b0)
{
    float4 mainColor;
    float4 shadowColorOpacity;
    bool has_mainTexture;

    float4 specularColor;
    float specularRampThreshold;
    float specularRampSmoothness;
    float specularExponent;

    float fresnelRampThreshold;
    float fresnelRampSmoothness;

    float4 crossHatchTilingOffset;
    float crossHatchThreshold;
    float crossHatchSmoothness;

    float4 ambientLightingColor;
    float ambientLightingThreshold;
    float ambientLightingSmoothness;
};

Texture2D mainTexture : register(t0);
Texture2D<float> toonRamp : register(t1);
Texture2D<float> crossHatchPatternTexture : register(t2);

Texture2D<float2> varianceShadowMap : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
Texture2D<float> ssaoTexture : register(t1, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

#define APPLY_RAMP(threshold, smoothness, value) (smoothstep((threshold), (threshold) + (smoothness), (value)))

inline float ApplyRamp(float value)
{
    float2 uv = float2((value + 1) * 0.5f, 0);
    return toonRamp.Sample(g_Common_PointClampSampler, uv);
}

inline float ApplySpecularRamp(float value)
{
    return APPLY_RAMP(specularRampThreshold, specularRampSmoothness, value);
}

inline float ApplyFresnelRamp(float value)
{
    return APPLY_RAMP(fresnelRampThreshold, fresnelRampSmoothness, value);
}

inline float ApplyCrossHatchRamp(float value)
{
    return APPLY_RAMP(crossHatchThreshold, crossHatchSmoothness, value);
}

inline float ApplyAmbientRamp(float value)
{
    return APPLY_RAMP(ambientLightingThreshold, ambientLightingSmoothness, value);
}

inline float SampleShadowAttenuation(float4 shadowCoords)
{
    float2 varianceSample = varianceShadowMap.Sample(g_Common_LinearClampSampler, shadowCoords.xy);
    float variance = varianceSample.y - varianceSample.x * varianceSample.x;
    float difference = shadowCoords.z - varianceSample.x;
    if (difference > 0.00001)
    {
        return smoothstep(0.4, 1.0, variance / (variance + difference * difference));
    }

    return 1.0;
}

float2 GetScreenSpaceUV(float4 positionCS)
{
    return positionCS.xy /= g_Pipeline_Screen_Resolution;
}

float GetCrossHatchAttenuation(float2 crossHatchingUV, float rawDiffuseAttenuation)
{
    const float2 uv = crossHatchingUV * crossHatchTilingOffset.xy + crossHatchTilingOffset.zw;
    float patternSample = crossHatchPatternTexture.Sample(g_Common_LinearWrapSampler, uv);
    return lerp(patternSample, 1.0, ApplyCrossHatchRamp(rawDiffuseAttenuation));
}

float4 main(ToonVaryings IN) : SV_TARGET
{
    const float3 normalWS = normalize(IN.NormalWS);
    float3 albedo = mainColor.rgb;
    if (has_mainTexture)
        albedo *= mainTexture.Sample(g_Common_LinearWrapSampler, IN.UV).rgb;

    const float3 lightDirectionWS = g_Pipeline_DirectionalLight.DirectionWs.xyz;
    const float ao = ssaoTexture.Sample(g_Common_LinearClampSampler, GetScreenSpaceUV(IN.PositionCS));
    const float shadowAttenuation = SampleShadowAttenuation(IN.ShadowCoords) * ao;
    const float NdotL = dot(normalWS, lightDirectionWS);

    const float rawDiffuseAttenuation = min(NdotL, NdotL * shadowAttenuation);
    const float diffuseAttenuation = ApplyRamp(rawDiffuseAttenuation);
    const float crossHatchAttenuation = GetCrossHatchAttenuation(IN.CrossHatchingUV, rawDiffuseAttenuation);
    const float brightness = diffuseAttenuation * crossHatchAttenuation;
    const float3 shadowColor = lerp(albedo, shadowColorOpacity.rgb, shadowColorOpacity.a);
    const float3 diffuse = lerp(shadowColor, albedo, brightness);

    const float3 r = normalize(reflect(lightDirectionWS , normalWS));
    const float RdotV = max(0, dot(r, IN.EyeWS));
    const float specularIntensity = pow(RdotV, specularExponent);
    const float specularAttenuation = ApplySpecularRamp(min(specularIntensity, specularIntensity * shadowAttenuation));

    const float fresnelIntensity = 1 - saturate(dot(-IN.EyeWS, normalWS));
    const float fresnelAttenuation = ApplyFresnelRamp(min(fresnelIntensity, fresnelIntensity * shadowAttenuation * diffuseAttenuation));
    const float3 specular = specularColor.rgb * max(specularAttenuation, fresnelAttenuation);

    const float ambientFactor = normalWS.y;
    const float ambientIntensity = ApplyAmbientRamp(ambientFactor);
    const float3 ambient = albedo * ambientLightingColor.rgb * ambientIntensity;

    const float3 resultColor = (diffuse + specular) * g_Pipeline_DirectionalLight.Color.rgb + ambient;

    return float4(resultColor, 1.0f);
}

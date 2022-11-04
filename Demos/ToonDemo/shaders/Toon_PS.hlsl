#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "ShaderLibrary/Pipeline.hlsli"

struct PixelShaderInput
{
    float3 NormalWS : NORMAL;
    float2 UV : TEXCOORD;
    float3 EyeWS : EYE_WS;
    float4 ShadowCoords : SHADOW_COORDS;
};

cbuffer Material : register(b0)
{
    float4 mainColor;
    float4 shadowColorOpacity;
    bool has_mainTexture;

    float rampThreshold;
    float rampSmoothness;

    float4 specularColor;
    float specularRampThreshold;
    float specularRampSmoothness;
    float specularExponent;
};

Texture2D mainTexture : register(t0);

Texture2D<float2> varianceShadowMap : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

#define APPLY_RAMP(threshold, smoothness, value) (smoothstep((threshold), (threshold) + (smoothness), (value)))

inline float ApplyRamp(float value)
{
    return APPLY_RAMP(rampThreshold, rampSmoothness, value);
}

inline float ApplySpecularRamp(float value)
{
    return APPLY_RAMP(specularRampThreshold, specularRampSmoothness, value);
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

float4 main(PixelShaderInput IN) : SV_TARGET
{
    const float3 normalWS = normalize(IN.NormalWS);
    float3 albedo = mainColor.rgb;
    if (has_mainTexture)
        albedo *= mainTexture.Sample(g_Common_LinearWrapSampler, IN.UV).rgb;

    const float3 lightDirectionWS = g_Pipeline_DirectionalLight.DirectionWs.xyz;
    const float shadowAttenuation = SampleShadowAttenuation(IN.ShadowCoords);
    const float NdotL = dot(normalWS, lightDirectionWS);

    const float diffuseAttenuation = ApplyRamp(min(NdotL, NdotL * shadowAttenuation));
    const float3 shadowColor = lerp(albedo, shadowColorOpacity.rgb, shadowColorOpacity.a);
    const float3 diffuse = lerp(shadowColor, albedo, diffuseAttenuation);

    const float3 r = normalize(reflect(lightDirectionWS , normalWS));
    const float RdotV = pow(max(0, dot(r, IN.EyeWS)), specularExponent);
    const float specularAttenuation = ApplySpecularRamp(min(RdotV, RdotV * shadowAttenuation));
    const float3 specular = specularColor.rgb * specularAttenuation;

    const float3 resultColor = (diffuse + specular) * g_Pipeline_DirectionalLight.Color.rgb;

    return float4(resultColor, 1.0f);
}

#include "ShaderLibrary/Pipeline.hlsli"

struct PixelShaderInput
{
    float3 NormalWS : NORMAL;
    float2 UV : TEXCOORD;
    float3 EyeWS : EYE_WS;
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

#define APPLY_RAMP(threshold, smoothness, value) (smoothstep((threshold), (threshold) + (smoothness), (value)))

inline float ApplyRamp(float value)
{
    return APPLY_RAMP(rampThreshold, rampSmoothness, value);
}

inline float ApplySpecularRamp(float value)
{
    return APPLY_RAMP(specularRampThreshold, specularRampSmoothness, value);
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    const float3 normalWS = normalize(IN.NormalWS);
    float3 albedo = mainColor.rgb;
    if (has_mainTexture)
        albedo *= mainTexture.Sample(g_Common_LinearWrapSampler, IN.UV).rgb;

    const float3 lightDirectionWS = g_Pipeline_DirectionalLight.DirectionWs.xyz;
    const float NdotL = dot(normalWS, lightDirectionWS);

    const float diffuseAttenuation = ApplyRamp(NdotL);
    const float3 shadowColor = lerp(albedo, shadowColorOpacity.rgb, shadowColorOpacity.a);
    const float3 diffuse = lerp(shadowColor, albedo, diffuseAttenuation);

    const float3 r = normalize(reflect(lightDirectionWS , normalWS));
    const float RdotV = max(0, dot(r, IN.EyeWS));
    const float specularAttenuation = ApplySpecularRamp(pow(RdotV, specularExponent));
    const float3 specular = specularColor.rgb * specularAttenuation;

    const float3 resultColor = (diffuse + specular) * g_Pipeline_DirectionalLight.Color.rgb;

    return float4(resultColor, 1.0f);
}

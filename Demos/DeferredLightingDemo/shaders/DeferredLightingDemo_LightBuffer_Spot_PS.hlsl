#include "ShaderLibrary/SpotLight.hlsli"
#include "ShaderLibrary/GBufferUtils.hlsli"
#include "ShaderLibrary/ScreenParameters.hlsli"
#include "ShaderLibrary/BRDF.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

cbuffer CBuffer : register(b0)
{
    SpotLight spotLightCB;
}

#include "ShaderLibrary/GBuffer.hlsli"

struct PixelShaderInput
{
    float3 PositionWS : POSITION;
    float4 PositionCS : SV_POSITION;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    ScreenParameters screenParameters = GetScreenParameters();
    float2 uv = ToScreenSpaceUV(IN.PositionCS, screenParameters);
    float3 diffuseColor = gBufferDiffuse.Sample(gBufferSampler, uv).rgb;
    float3 normalWS = normalize(UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz));
    
    float zNDC = gBufferDepth.Sample(gBufferSampler, uv).x;
    float3 positionNDC = ScreenSpaceUVToNDC(uv, zNDC);
    float3 positionWS = RestorePositionWS(positionNDC, g_Pipeline_InverseProjection, g_Pipeline_InverseView);
    
    const float3 offsetWS = spotLightCB.PositionWS.xyz - positionWS;
    const float distance = length(offsetWS);
    const float3 directionTowardsLightWS = offsetWS / distance;

    const float attenuation = GetSpotLightDistanceAttenuation(spotLightCB.Attenuation, distance) *
		GetSpotLightConeAttenuation(spotLightCB.DirectionWS.xyz, directionTowardsLightWS, spotLightCB.SpotAngle);
    
    BRDFInput brdfInput;
    brdfInput.CameraPositionWS = g_Pipeline_CameraPosition.xyz;
    brdfInput.LightColor = spotLightCB.Color.rgb * spotLightCB.Intensity * attenuation;
    brdfInput.LightDirectionWS = directionTowardsLightWS;
    brdfInput.NormalWS = normalWS;
    brdfInput.PositionWS = positionWS;
    brdfInput.DiffuseColor = diffuseColor;
    brdfInput.Irradiance = 0;
    
    const float4 surface = gBufferSurface.Sample(gBufferSampler, uv);
    UnpackSurface(surface, brdfInput.Metallic, brdfInput.Roughness, brdfInput.AmbientOcclusion);
    
    float3 color = ComputeBRDF(brdfInput);
    return float4(color, 1.0);
}
#include "ShaderLibrary/GBufferUtils.hlsli"
#include "ShaderLibrary/BRDF.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

cbuffer CBuffer : register(b0)
{
    float4 DirectionWS;
    float4 Color;
};

#include "ShaderLibrary/GBuffer.hlsli"

TextureCube irradianceMap : register(t5);
SamplerState ambientMapSampler : register(s1);

float3 ComputeBRDFAmbient(in BRDFInput input)
{
    float3 eyeWS = normalize(input.CameraPositionWS - input.PositionWS);
    float3 reflectionWS = reflect(-eyeWS, input.NormalWS);

    float3 F0 = ComputeF0(input);
    float3 F = FresnelSchlick(max(dot(input.NormalWS, eyeWS), 0.0), F0, input.Roughness);;
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - input.Metallic;

    float3 diffuse = input.Irradiance * input.DiffuseColor;
    return (kD * diffuse) * input.AmbientOcclusion;
}

struct PixelShaderInput
{
    float4 PositionCS : SV_Position;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    ScreenParameters screenParameters = GetScreenParameters();
    float2 uv = ToScreenSpaceUV(IN.PositionCS, screenParameters);
    float4 gBufferDiffuseSample = gBufferDiffuse.Sample(gBufferSampler, uv);
    float3 diffuseColor = gBufferDiffuseSample.rgb;
    float emission = gBufferDiffuseSample.a;
    float3 normalWS = normalize(UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz));

    float zNDC = gBufferDepth.Sample(gBufferSampler, uv).x;
    float3 positionNDC = ScreenSpaceUVToNDC(uv, zNDC);
    float3 positionWS = RestorePositionWS(positionNDC, g_Pipeline_InverseProjection, g_Pipeline_InverseView);

    BRDFInput brdfInput;
    brdfInput.CameraPositionWS = g_Pipeline_CameraPosition.xyz;
    brdfInput.LightColor = Color.rgb;
    brdfInput.LightDirectionWS = DirectionWS.xyz;
    brdfInput.NormalWS = normalWS;
    brdfInput.PositionWS = positionWS;
    brdfInput.DiffuseColor = diffuseColor;
    brdfInput.Irradiance = irradianceMap.SampleLevel(ambientMapSampler, normalWS, 0).rgb;

    const float4 surface = gBufferSurface.Sample(gBufferSampler, uv);
    UnpackSurface(surface, brdfInput.Metallic, brdfInput.Roughness, brdfInput.AmbientOcclusion);

    float3 color = ComputeBRDF(brdfInput) + g_Pipeline_AmbientIntensity * ComputeBRDFAmbient(brdfInput) + diffuseColor * emission;
    return float4(color, 1.0);
}

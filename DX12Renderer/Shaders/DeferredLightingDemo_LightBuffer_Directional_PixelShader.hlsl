#include <ShaderLibrary/MatricesCB.hlsli>
#include <ShaderLibrary/PointLight.hlsli>
#include <ShaderLibrary/GBufferUtils.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>
#include <ShaderLibrary/BRDF.hlsli>

struct DirectionalLight
{
    float4 DirectionWS;
    float4 Color;
};

ConstantBuffer<Matrices> matricesCB : register(b0);
ConstantBuffer<DirectionalLight> directionalLightCB : register(b1);
ConstantBuffer<ScreenParameters> screenParametersCB : register(b2);

TextureCube irradianceMap : register(t4);
SamplerState irradianceMapSampler : register(s1);

#include <ShaderLibrary/GBuffer.hlsli>

struct PixelShaderInput
{
    float4 PositionCS : SV_Position;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = ToScreenSpaceUV(IN.PositionCS, screenParametersCB);
    float3 diffuseColor = gBufferDiffuse.Sample(gBufferSampler, uv).rgb;
    float3 normalWS = normalize(UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz));
    
    float zNDC = gBufferDepth.Sample(gBufferSampler, uv).x;
    float3 positionNDC = ScreenSpaceUVToNDC(uv, zNDC);
    float3 positionWS = RestorePositionWS(positionNDC, matricesCB.InverseProjection, matricesCB.InverseView);
    
    BRDFInput brdfInput;
    brdfInput.CameraPositionWS = matricesCB.CameraPosition.xyz;
    brdfInput.LightColor = directionalLightCB.Color.rgb;
    brdfInput.LightDirectionWS = directionalLightCB.DirectionWS.xyz;
    brdfInput.NormalWS = normalWS;
    brdfInput.PositionWS = positionWS;
    brdfInput.DiffuseColor = diffuseColor;
    brdfInput.Irradiance = irradianceMap.SampleLevel(irradianceMapSampler, normalWS, 0).rgb;
    
    const float4 surface = gBufferSurface.Sample(gBufferSampler, uv);
    UnpackSurface(surface, brdfInput.Metallic, brdfInput.Roughness, brdfInput.AmbientOcclusion);
    
    float3 color = ComputeBRDF(brdfInput) + ComputeBRDFAmbient(brdfInput);
    return float4(color, 1.0);
}
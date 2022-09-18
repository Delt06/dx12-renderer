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

#include <ShaderLibrary/GBuffer.hlsli>

TextureCube irradianceMap : register(t4);
TextureCube prefilterMap : register(t5);
Texture2D brdfLUT : register(t6);
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
    
    // https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/2.2.1.ibl_specular/2.2.1.pbr.fs
    static const float MAX_REFLECTION_LOD = 4.0;
    float3 prefilteredColor = prefilterMap.SampleLevel(ambientMapSampler, reflectionWS, input.Roughness * MAX_REFLECTION_LOD).rgb;
    float2 brdf = brdfLUT.Sample(ambientMapSampler, float2(max(dot(input.NormalWS, eyeWS), 0.0), input.Roughness)).rg;
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    return (kD * diffuse + specular) * input.AmbientOcclusion;
}

struct PixelShaderInput
{
    float4 PositionCS : SV_Position;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = ToScreenSpaceUV(IN.PositionCS, screenParametersCB);
    float4 gBufferDiffuseSample = gBufferDiffuse.Sample(gBufferSampler, uv);
    float3 diffuseColor = gBufferDiffuseSample.rgb;
    float emission = gBufferDiffuseSample.a;
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
    brdfInput.Irradiance = irradianceMap.SampleLevel(ambientMapSampler, normalWS, 0).rgb;
    
    const float4 surface = gBufferSurface.Sample(gBufferSampler, uv);
    UnpackSurface(surface, brdfInput.Metallic, brdfInput.Roughness, brdfInput.AmbientOcclusion);
    
    float3 color = ComputeBRDF(brdfInput) + ComputeBRDFAmbient(brdfInput) + diffuseColor * emission;
    return float4(color, 1.0);
}
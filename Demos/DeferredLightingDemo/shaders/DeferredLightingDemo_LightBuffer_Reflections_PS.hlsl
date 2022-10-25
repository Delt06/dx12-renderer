#include "ShaderLibrary/PointLight.hlsli"
#include "ShaderLibrary/GBufferUtils.hlsli"
#include "ShaderLibrary/ScreenParameters.hlsli"
#include "ShaderLibrary/BRDF.hlsli"
#include <ShaderLibrary/Common/RootSignature.hlsli>
#include <ShaderLibrary/Pipeline.hlsli>

#include "ShaderLibrary/GBuffer.hlsli"

TextureCube preFilterMap : register(t0);
Texture2D brdfLut : register(t1);
Texture2D ssrTexture : register(t2);

float3 ComputeBRDFReflections(in BRDFInput input, float4 ssrSample)
{
    float3 eyeWS = normalize(input.CameraPositionWS - input.PositionWS);
    float3 reflectionWS = reflect(-eyeWS, input.NormalWS);
    
    float3 F0 = ComputeF0(input);
    float3 F = FresnelSchlick(max(dot(input.NormalWS, eyeWS), 0.0), F0, input.Roughness);;
    
    // https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/2.2.1.ibl_specular/2.2.1.pbr.fs
    static const float MAX_REFLECTION_LOD = 4.0;
    float3 prefilteredColor = preFilterMap.SampleLevel(g_Common_LinearClampSampler, reflectionWS, input.Roughness * MAX_REFLECTION_LOD).rgb;
    float2 brdf = brdfLut.Sample(g_Common_LinearClampSampler, float2(max(dot(input.NormalWS, eyeWS), 0.0), input.Roughness)).rg;
    float3 specular = lerp(prefilteredColor, ssrSample.rgb, ssrSample.a) * (F * brdf.x + brdf.y); // A channel stores fade amount
    return specular * input.AmbientOcclusion;
}

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    float4 gBufferDiffuseSample = gBufferDiffuse.Sample(gBufferSampler, uv);
    float3 diffuseColor = gBufferDiffuseSample.rgb;
    float3 normalWS = normalize(UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz));
    
    float zNDC = gBufferDepth.Sample(gBufferSampler, uv).x;
    float3 positionNDC = ScreenSpaceUVToNDC(uv, zNDC);
    float3 positionWS = RestorePositionWS(positionNDC, g_Pipeline_InverseProjection, g_Pipeline_InverseView);
    
    BRDFInput brdfInput;
    brdfInput.CameraPositionWS = g_Pipeline_CameraPosition.xyz;
    brdfInput.NormalWS = normalWS;
    brdfInput.PositionWS = positionWS;
    brdfInput.DiffuseColor = diffuseColor;
    
    const float4 surface = gBufferSurface.Sample(gBufferSampler, uv);
    UnpackSurface(surface, brdfInput.Metallic, brdfInput.Roughness, brdfInput.AmbientOcclusion);
    
    float4 ssrSample = ssrTexture.Sample(gBufferSampler, uv);
    
    float3 color = ComputeBRDFReflections(brdfInput, ssrSample);
    return float4(color, 1.0);
}
#include <ShaderLibrary/MatricesCB.hlsli>
#include <ShaderLibrary/PointLight.hlsli>
#include <ShaderLibrary/GBufferUtils.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>
#include <ShaderLibrary/BRDF.hlsli>


ConstantBuffer<Matrices> matricesCB : register(b0);

#include <ShaderLibrary/GBuffer.hlsli>

TextureCube prefilterMap : register(t4);
Texture2D brdfLUT : register(t5);
Texture2D ssrReflections : register(t6);
SamplerState ambientMapSampler : register(s1);

float3 ComputeBRDFReflections(in BRDFInput input, float4 ssrSample)
{
    float3 eyeWS = normalize(input.CameraPositionWS - input.PositionWS);
    float3 reflectionWS = reflect(-eyeWS, input.NormalWS);
    
    float3 F0 = ComputeF0(input);
    float3 F = FresnelSchlick(max(dot(input.NormalWS, eyeWS), 0.0), F0, input.Roughness);;
    
    // https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/2.2.1.ibl_specular/2.2.1.pbr.fs
    static const float MAX_REFLECTION_LOD = 4.0;
    float3 prefilteredColor = prefilterMap.SampleLevel(ambientMapSampler, reflectionWS, input.Roughness * MAX_REFLECTION_LOD).rgb;
    float2 brdf = brdfLUT.Sample(ambientMapSampler, float2(max(dot(input.NormalWS, eyeWS), 0.0), input.Roughness)).rg;
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
    float3 positionWS = RestorePositionWS(positionNDC, matricesCB.InverseProjection, matricesCB.InverseView);
    
    BRDFInput brdfInput;
    brdfInput.CameraPositionWS = matricesCB.CameraPosition.xyz;
    brdfInput.NormalWS = normalWS;
    brdfInput.PositionWS = positionWS;
    brdfInput.DiffuseColor = diffuseColor;
    
    const float4 surface = gBufferSurface.Sample(gBufferSampler, uv);
    UnpackSurface(surface, brdfInput.Metallic, brdfInput.Roughness, brdfInput.AmbientOcclusion);
    
    float4 ssrSample = ssrReflections.Sample(gBufferSampler, uv);
    
    float3 color = ComputeBRDFReflections(brdfInput, ssrSample);
    return float4(color, 1.0);
}
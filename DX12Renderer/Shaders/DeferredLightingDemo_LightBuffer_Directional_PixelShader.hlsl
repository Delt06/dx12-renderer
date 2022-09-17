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

TextureCube skybox : register(t4);
SamplerState skyboxSampler : register(s1);

float3 SampleSkybox(float3 uv)
{
    return skybox.SampleLevel(skyboxSampler, uv, 0).rgb;
}

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
    
    const float4 surface = gBufferSurface.Sample(gBufferSampler, uv);
    UnpackSurface(surface, brdfInput.Metallic, brdfInput.Roughness, brdfInput.AmbientOcclusion);
    
    float3 color = ComputeBRDF(brdfInput);
    
    color += SampleSkybox(normalWS) * diffuseColor * brdfInput.AmbientOcclusion;
    
    return float4(color, 1.0);
}
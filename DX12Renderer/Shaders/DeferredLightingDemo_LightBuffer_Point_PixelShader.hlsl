#include <ShaderLibrary/MatricesCB.hlsli>
#include <ShaderLibrary/PointLight.hlsli>
#include <ShaderLibrary/GBufferUtils.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>
#include <ShaderLibrary/BRDF.hlsli>

ConstantBuffer<Matrices> matricesCB : register(b0);
ConstantBuffer<PointLight> pointLightCB : register(b1);
ConstantBuffer<ScreenParameters> screenParametersCB : register(b2);

#include <ShaderLibrary/GBuffer.hlsli>

struct PixelShaderInput
{
    float3 PositionWS : POSITION;
    float4 PositionCS : SV_POSITION;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = ToScreenSpaceUV(IN.PositionCS, screenParametersCB);
    float3 diffuseColor = gBufferDiffuse.Sample(gBufferSampler, uv).rgb;
    float3 normalWS = normalize(UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz));
    
    float zNDC = gBufferDepth.Sample(gBufferSampler, uv).x;
    float3 positionNDC = ScreenSpaceUVToNDC(uv, zNDC);
    float3 positionWS = RestorePositionWS(positionNDC, matricesCB.InverseProjection, matricesCB.InverseView);
    
    float3 lightOffsetWS = pointLightCB.PositionWS.xyz - positionWS;
    float lightDistance = length(lightOffsetWS);
    float3 lightDirectionWS = lightOffsetWS / lightDistance;
    
    float distanceAttenuation = ComputeDistanceAttenuation(pointLightCB.ConstantAttenuation, pointLightCB.LinearAttenuation, pointLightCB.QuadraticAttenuation, lightDistance);
    
    BRDFInput brdfInput;
    brdfInput.CameraPositionWS = matricesCB.CameraPosition.xyz;
    brdfInput.LightColor = distanceAttenuation * pointLightCB.Color.rgb;
    brdfInput.LightDirectionWS = lightDirectionWS;
    brdfInput.NormalWS = normalWS;
    brdfInput.PositionWS = positionWS;
    brdfInput.DiffuseColor = diffuseColor;
    brdfInput.Irradiance = 0;
    
    const float4 surface = gBufferSurface.Sample(gBufferSampler, uv);
    UnpackSurface(surface, brdfInput.Metallic, brdfInput.Roughness, brdfInput.AmbientOcclusion);
    
    float3 color = ComputeBRDF(brdfInput);
    return float4(color, 1.0);
}
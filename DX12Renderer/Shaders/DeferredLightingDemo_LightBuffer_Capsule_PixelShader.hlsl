#include <ShaderLibrary/MatricesCB.hlsli>
#include <ShaderLibrary/CapsuleLight.hlsli>
#include <ShaderLibrary/GBufferUtils.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>
#include <ShaderLibrary/BRDF.hlsli>

ConstantBuffer<Matrices> matricesCB : register(b0);
ConstantBuffer<CapsuleLight> capsuleLightCB : register(b1);
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
    
    // https://iquilezles.org/articles/distfunctions/
    float3 pa = positionWS - capsuleLightCB.PointA.xyz;
    float3 ab = capsuleLightCB.PointB.xyz - capsuleLightCB.PointA.xyz;
    float h = saturate(dot(pa, ab) / dot(ab, ab));
    float3 lightOffsetWS = ab * h - pa;
    float lightDistance = length(lightOffsetWS);
    float3 lightDirectionWS = lightOffsetWS / lightDistance;
    
    float distanceAttenuation = GetCapsuleLightDistanceAttenuation(capsuleLightCB.Attenuation, lightDistance);
    
    BRDFInput brdfInput;
    brdfInput.CameraPositionWS = matricesCB.CameraPosition.xyz;
    brdfInput.LightColor = distanceAttenuation * capsuleLightCB.Color.rgb;
    brdfInput.LightDirectionWS = lightDirectionWS;
    brdfInput.NormalWS = normalWS;
    brdfInput.PositionWS = positionWS;
    brdfInput.DiffuseColor = diffuseColor;
    
    float3 color = ComputeBRDF(brdfInput);
    return float4(color, 1.0);
}
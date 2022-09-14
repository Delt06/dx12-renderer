#include <ShaderLibrary/MatricesCB.hlsli>
#include <ShaderLibrary/PointLight.hlsli>
#include <ShaderLibrary/GBufferUtils.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>

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
    
    float lambert = max(0.0, dot(lightDirectionWS, normalWS));
    float distanceAttenuation = ComputeDistanceAttenuation(pointLightCB.ConstantAttenuation, pointLightCB.LinearAttenuation, pointLightCB.QuadraticAttenuation, lightDistance);
    return float4(lambert * distanceAttenuation * diffuseColor * pointLightCB.Color.rgb, 1.0);
}
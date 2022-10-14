#include "ShaderLibrary/GBufferUtils.hlsli"
#include "ShaderLibrary/ScreenParameters.hlsli"
#include "ShaderLibrary/GBuffer.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"
#include "ShaderLibrary/Common/RootSignature.hlsli"

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

#define SAMPLES_COUNT 64
#define NORMAL_BIAS 0.2
#define DEPTH_BIAS 0.025

cbuffer SSAOCBuffer : register(b0)
{
    float2 NoiseScale;
    float Radius;
    uint KernelSize;
    
    float Power;
    float3 _Padding;
    
    float4 Samples[SAMPLES_COUNT];
};

Texture2D noiseTexture : register(t0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    // https://learnopengl.com/Advanced-Lighting/SSAO
    
    float2 uv = IN.UV;
    float3 normal = normalize(UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz));
    
    float zNDC = gBufferDepth.Sample(gBufferSampler, uv).x;
    if (zNDC > 0.999)
    {
        return 1;
    }
    
    float3 positionNDC = ScreenSpaceUVToNDC(uv, zNDC);
    float4 positionVS = RestorePositionVS(positionNDC, g_Pipeline_InverseProjection);
    float3 positionWS = mul(g_Pipeline_InverseView, positionVS).xyz;
    
    float3 randomVector = float3(noiseTexture.Sample(g_Common_PointWrapSampler, uv * NoiseScale).xy, 0);
    
    const float3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
    const float3 bitangent = cross(normal, tangent);
    const float3x3 tbn = float3x3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    
    for (uint i = 0; i < KernelSize; ++i)
    {
        float3 samplePositionWS = positionWS + normal * Radius * 0.5 + mul(tbn, Samples[i].xyz) * Radius;
        
        float4 samplePositionCS = mul(g_Pipeline_ViewProjection, float4(samplePositionWS, 1.0));
        samplePositionCS /= samplePositionCS.w;
        
        float2 sampleScreenSpaceUV = samplePositionCS.xy;
        sampleScreenSpaceUV = sampleScreenSpaceUV * 0.5 + 0.5;
        sampleScreenSpaceUV.y = 1 - sampleScreenSpaceUV.y;
        
        float sampleDepthZNDC = gBufferDepth.SampleLevel(gBufferSampler, sampleScreenSpaceUV, 0).x;
        float3 sampleDepthPositionNDC = ScreenSpaceUVToNDC(sampleScreenSpaceUV, sampleDepthZNDC);
        float3 sampleDepthPositionVS = RestorePositionVS(sampleDepthPositionNDC, g_Pipeline_InverseProjection).xyz;
        
        float3 samplePositionVS = mul(g_Pipeline_View, float4(samplePositionWS, 1.0)).xyz;
        
        float rangeCheck = smoothstep(0.0, 1.0, Radius / abs(positionVS.z - sampleDepthPositionVS.z));
        occlusion += (samplePositionVS.z >= sampleDepthPositionVS.z + DEPTH_BIAS ? 1.0 : 0.0) * rangeCheck;
    }
    
    occlusion = 1.0 - (occlusion / KernelSize);
    return float4(pow(occlusion, Power), 1.0f, 1.0f, 1.0f);
}
#include <ShaderLibrary/GBufferutils.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

#define SAMPLES_COUNT 32
#define NORMAL_BIAS 0.2
#define DEPTH_BIAS 0.025

cbuffer SSAOCBuffer : register(b0)
{
    float2 NoiseScale;
    float Radius;
    uint KernelSize;
    
    float Power;
    float3 _Padding;
    
    matrix InverseProjection;
    matrix InverseView;
    matrix View;
    matrix ViewProjection;
    
    float3 Samples[SAMPLES_COUNT];
};

Texture2D gBufferNormalsWS : register(t0);
Texture2D gBufferDepth : register(t1);
Texture2D noiseTexture : register(t2);

SamplerState gBufferSampler : register(s0);
SamplerState noiseTextureSampler : register(s1);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    // https://learnopengl.com/Advanced-Lighting/SSAO
    
    float2 uv = IN.UV;
    float3 normal = normalize(UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz));
    
    float zNDC = gBufferDepth.Sample(gBufferSampler, uv).x;
    float3 positionNDC = ScreenSpaceUVToNDC(uv, zNDC);
    float4 positionVS = RestorePositionVS(positionNDC, InverseProjection);
    float3 positionWS = mul(InverseView, positionVS).xyz;
    
    float3 randomVector = float3(noiseTexture.Sample(noiseTextureSampler, uv * NoiseScale).xy, 0);
    
    const float3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
    const float3 bitangent = cross(normal, tangent);
    const float3x3 tbn = float3x3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    
    for (uint i = 0; i < KernelSize; ++i)
    {
        float3 samplePositionWS = positionWS + normal * NORMAL_BIAS + mul(tbn, Samples[i]) * Radius;
        
        float4 samplePositionCS = mul(ViewProjection, float4(samplePositionWS, 1.0));
        samplePositionCS /= samplePositionCS.w;
        
        float2 sampleScreenSpaceUV = samplePositionCS.xy;
        sampleScreenSpaceUV = sampleScreenSpaceUV * 0.5 + 0.5;
        sampleScreenSpaceUV.y = 1 - sampleScreenSpaceUV.y;
        
        float sampleDepthZNDC = gBufferDepth.Sample(gBufferSampler, sampleScreenSpaceUV).x;
        float3 sampleDepthPositionNDC = ScreenSpaceUVToNDC(sampleScreenSpaceUV, sampleDepthZNDC);
        float3 sampleDepthPositionVS = RestorePositionVS(sampleDepthPositionNDC, InverseProjection).xyz;
        
        float3 samplePositionVS = mul(View, float4(samplePositionWS, 1.0)).xyz;
        
        float rangeCheck = smoothstep(0.0, 1.0, Radius / abs(positionVS.z - sampleDepthPositionVS.z));
        occlusion += (samplePositionVS.z >= sampleDepthPositionVS.z + DEPTH_BIAS ? 1.0 : 0.0) * rangeCheck;
    }
    
    occlusion = 1.0 - (occlusion / KernelSize);
    return float4(pow(occlusion, Power), 1.0f, 1.0f, 1.0f);
}
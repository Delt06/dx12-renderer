#include "ShaderLibrary/Math.hlsli"
#include "ShaderLibrary/IBLUtils.hlsli"
#include "ShaderLibrary/BRDF.hlsli"

struct PixelShaderInput
{
    float3 Normal : TEXCOORD;
};

struct Parameters
{
    float4 Forward;
    float4 Up;
    
    float Roughness;
    float2 SourceResolution;
    float _Padding;
};

ConstantBuffer<Parameters> parametersCB : register(b0);
TextureCube source : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_TARGET
{    
    // https://learnopengl.com/PBR/IBL/Specular-IBL
    float3 N = normalize(IN.Normal);
    float3 R = N;
    float3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;
    float3 prefilteredColor = 0;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, parametersCB.Roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            // sample from the environment's mip level based on roughness/pdf
            float D = DistributionGGX(N, H, parametersCB.Roughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

            float resolution = 512.0; // resolution of source cubemap (per face)
            float saTexel = 4.0 * PI / (6.0 * parametersCB.SourceResolution.x * parametersCB.SourceResolution.y);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = parametersCB.Roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
            
            prefilteredColor += source.SampleLevel(sourceSampler, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefilteredColor = prefilteredColor / totalWeight;

    return float4(prefilteredColor, 1.0);
}
#include <ShaderLibrary/GBufferUtils.hlsli>

struct DirectionalLight
{
    float4 DirectionWS;
    float4 Color;
};

ConstantBuffer<DirectionalLight> directionalLightCB : register(b0);

#include <ShaderLibrary/GBuffer.hlsli>

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    float3 diffuse = gBufferDiffuse.Sample(gBufferSampler, uv).xyz;
    float3 normalWs = normalize(UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz));

    float3 color = directionalLightCB.Color.xyz * diffuse * max(0, dot(directionalLightCB.DirectionWS.xyz, normalWs));
    return float4(color, 1.0);
}
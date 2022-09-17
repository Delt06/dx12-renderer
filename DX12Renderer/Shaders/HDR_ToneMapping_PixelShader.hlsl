struct VertexShaderInput
{
    float2 UV : TEXCOORD;
};

struct Parameters
{
    float Exposure;
};

Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);
ConstantBuffer<Parameters> parametersCB : register(b0);

float4 main(VertexShaderInput IN) : SV_TARGET
{
    float3 hdrColor = source.Sample(sourceSampler, IN.UV).rgb;
  
    // exposure tone mapping
    float3 mapped = 1 - exp(-hdrColor * parametersCB.Exposure);
    // no need for gamma correction is using an SRGB render target
    return float4(mapped, 1.0);
}
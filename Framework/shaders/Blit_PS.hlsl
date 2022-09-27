struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_Target
{
    float2 uv = IN.UV;
    return source.Sample(sourceSampler, uv);
}
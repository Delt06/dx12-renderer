struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

Texture2D traceResult : register(t0);
SamplerState traceSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float4 traceSample = traceResult.Sample(traceSampler, IN.UV);
    return traceSample * traceSample.a * 10;
}
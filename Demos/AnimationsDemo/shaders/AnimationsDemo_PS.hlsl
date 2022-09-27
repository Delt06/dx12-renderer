struct PixelShaderInput
{
    float3 NormalWs : NORMAL;
    float2 Uv : TEXCOORD;
};

Texture2D diffuseMap : register(t0, space1);
SamplerState defaultSampler : register(s0);


float4 main(PixelShaderInput IN) : SV_Target
{
    return diffuseMap.Sample(defaultSampler, IN.Uv);
}

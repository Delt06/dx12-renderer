struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

cbuffer ParametersCBuffer : register(b0)
{
    matrix InverseProjection;
    matrix InverseView;
    matrix View;
    matrix ViewProjection;
    
    float MaxDistance;
    float Resolution;
    uint Steps;
    float Thickness;
};

Texture2D sceneColor : register(t0);
Texture2D normals : register(t1);
Texture2D depth : register(t2);

SamplerState defaultSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    return normals.Sample(defaultSampler, uv);
}
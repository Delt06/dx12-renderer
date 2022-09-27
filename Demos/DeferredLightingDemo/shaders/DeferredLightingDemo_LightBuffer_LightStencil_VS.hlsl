#include "ShaderLibrary/MatricesCB.hlsli"

ConstantBuffer<Matrices> matricesCB : register(b0);

struct VertexShaderInput
{
    float3 PositionOS : POSITION;
};

struct VertexShaderOutput
{
    float4 PositionCS : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput IN)
{
    VertexShaderOutput OUT;
    OUT.PositionCS = mul(matricesCB.ModelViewProjection, float4(IN.PositionOS, 1.0));    
    return OUT;
}
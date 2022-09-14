#include <ShaderLibrary/MatricesCB.hlsli>

ConstantBuffer<Matrices> matricesCb : register(b0);

struct VertexAttributes
{
    float3 PositionOs : POSITION;
    float3 Normal : NORMAL;
    float2 Uv : TEXCOORD;
    float3 TangentOs : TANGENT;
    float3 BitangentOs : BINORMAL;
};

struct VertexShaderOutput
{
    float3 NormalWs : NORMAL;
    float3 TangentWs : TANGENT;
    float3 BitangentWs : BINORMAL;
    float2 Uv : TEXCOORD0;
    float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;

    OUT.PositionCs = mul(matricesCb.ModelViewProjection, float4(IN.PositionOs, 1.0f));
    OUT.NormalWs = mul((float3x3) matricesCb.InverseTransposeModel, IN.Normal);
    OUT.TangentWs = mul((float3x3) matricesCb.InverseTransposeModel, IN.TangentOs);
    OUT.BitangentWs = mul((float3x3) matricesCb.InverseTransposeModel, IN.BitangentOs);
    OUT.Uv = IN.Uv;

    return OUT;
}

#include <ShaderLibrary/MatricesCB.hlsli>
#include <ShaderLibrary/TAABuffer.hlsli>

ConstantBuffer<Matrices> matricesCB : register(b0);
ConstantBuffer<TAABuffer> taaCB : register(b1, space1);

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
    float4 CurrentPositionCs : TAA_CURRENT_POSITION;
    float4 PrevPositionCs : TAA_PREV_POSITION;
    float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;

    OUT.PositionCs = mul(matricesCB.ModelViewProjection, float4(IN.PositionOs, 1.0f));
    OUT.NormalWs = mul((float3x3) matricesCB.InverseTransposeModel, IN.Normal);
    OUT.TangentWs = mul((float3x3) matricesCB.InverseTransposeModel, IN.TangentOs);
    OUT.BitangentWs = mul((float3x3) matricesCB.InverseTransposeModel, IN.BitangentOs);
    OUT.Uv = IN.Uv;
    
    OUT.CurrentPositionCs = mul(matricesCB.ModelViewProjection, float4(IN.PositionOs, 1.0f));
    OUT.PrevPositionCs = mul(taaCB.PreviousModelViewProjection, float4(IN.PositionOs, 1.0f));

    return OUT;
}

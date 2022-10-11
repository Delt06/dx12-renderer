#include "ShaderLibrary/Pipeline.hlsli"
#include "ShaderLibrary/Model.hlsli"

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

    float4 originalPositionCS = mul(g_Model_ModelViewProjection, float4(IN.PositionOs, 1.0f));
    float4 positionCS = originalPositionCS;
    positionCS.xy += g_Pipeline_Taa_JitterOffset * positionCS.w;
    OUT.PositionCs = positionCS;
    
    OUT.NormalWs = mul((float3x3) g_Model_InverseTransposeModel, IN.Normal);
    OUT.TangentWs = mul((float3x3) g_Model_InverseTransposeModel, IN.TangentOs);
    OUT.BitangentWs = mul((float3x3) g_Model_InverseTransposeModel, IN.BitangentOs);
    OUT.Uv = IN.Uv;
    
    OUT.CurrentPositionCs = originalPositionCS;
    OUT.PrevPositionCs = mul(g_Model_Taa_PreviousModelViewProjectionMatrix, float4(IN.PositionOs, 1.0f));

    return OUT;
}

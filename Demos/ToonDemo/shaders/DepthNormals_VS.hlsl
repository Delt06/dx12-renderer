#include "ShaderLibrary/Model.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

struct VertexAttributes
{
    float3 PositionOS : POSITION;
    float3 Normal : NORMAL;
};

struct VertexShaderOutput
{
    float3 NormalWS : NORMAL;
    float4 PositionCS : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;

    OUT.NormalWS = mul((float3x3) g_Model_InverseTransposeModel, IN.Normal);
    OUT.PositionCS = mul(g_Model_ModelViewProjection, float4(IN.PositionOS, 1.0f));

    return OUT;
}

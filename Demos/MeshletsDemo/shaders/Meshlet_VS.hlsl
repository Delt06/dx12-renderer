#include "ShaderLibrary/Model.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

#include "Meshlet_VertexShaderOutput.hlsli"

struct VertexAttributes
{
    float3 PositionOS : POSITION;
    float3 NormalOS : NORMAL;
};

VertexShaderOutput main(const VertexAttributes IN)
{
    VertexShaderOutput OUT;

    OUT.NormalWS = mul((float3x3) g_Model_InverseTransposeModel, IN.NormalOS);

    const float4 positionWS = mul(g_Model_Model, float4(IN.PositionOS, 1.0f));
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, positionWS);

    return OUT;
}

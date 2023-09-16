#include "ShaderLibrary/Model.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

#include "Meshlet_VertexShaderOutput.hlsli"

VertexShaderOutput main(const CommonVertexAttributes IN
    // , uint vertexId : SV_VertexID
)
{
    VertexShaderOutput OUT;

    OUT.NormalWS = mul((float3x3) g_Model_InverseTransposeModel, IN.normal);

    const float4 positionWS = mul(g_Model_Model, float4(IN.position, 1.0f));
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, positionWS);

    return OUT;
}

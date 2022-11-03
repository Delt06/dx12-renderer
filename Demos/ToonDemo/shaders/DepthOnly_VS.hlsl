#include "ShaderLibrary/Model.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

struct VertexAttributes
{
    float3 PositionOS : POSITION;
};

struct VertexShaderOutput
{
    float4 PositionCS : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;

    OUT.PositionCS = mul(g_Model_ModelViewProjection, float4(IN.PositionOS, 1.0f));

    return OUT;
}

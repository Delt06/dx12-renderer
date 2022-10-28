#include "ShaderLibrary/Model.hlsli"

struct VertexAttributes
{
    float3 PositionOs : POSITION;
};

struct VertexShaderOutput
{
    float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;
    
    OUT.PositionCs = mul(g_Model_ModelViewProjection, float4(IN.PositionOs, 1.0));

    return OUT;
}

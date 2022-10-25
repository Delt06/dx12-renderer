#include "ShaderLibrary/Model.hlsli"

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
    OUT.PositionCS = mul(g_Model_ModelViewProjection, float4(IN.PositionOS, 1.0));
    return OUT;
}
#include "ShaderLibrary/Model.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

#include "Meshlet_VertexShaderOutput.hlsli"

uint AlignDown(uint value, const uint alignment, out uint outRemainder)
{
    outRemainder = value % alignment;
    value -= outRemainder;
    return value;
}

uint LoadIndex(const in Meshlet meshlet, uint vertexId)
{
    uint remainder;
    const uint address = AlignDown((meshlet.triangleOffset + vertexId) * 2, 4, remainder);
    const uint index = _CommonIndexBuffer.Load<uint>(address);
    // the remainder is either 0 or 2
    return remainder == 0 ? index & 0xFFFF : ((index & 0xFFFF0000) >> 16);
}

VertexShaderOutput main(
    uint id : SV_VertexID
)
{
    VertexShaderOutput OUT;

    const Meshlet meshlet = _MeshletsBuffer[g_Model_Index];
    const uint index = LoadIndex(meshlet, id);
    const CommonVertexAttributes IN = _CommonVertexBuffer[meshlet.vertexOffset + index];

    OUT.NormalWS = mul((float3x3) g_Model_InverseTransposeModel, IN.normal.xyz);

    const float4 positionWS = mul(g_Model_Model, float4(IN.position.xyz, 1.0f));
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, positionWS);

    return OUT;
}

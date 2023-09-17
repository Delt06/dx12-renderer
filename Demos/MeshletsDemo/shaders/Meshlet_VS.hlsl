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
    return remainder == 0 ? index & 0xFFFF : ((index & 0xFFFF0000) >> 16);
    // return remainder > 0 ? index.y : index.x;
    // // the remainder is either 0 or 2, so the shift is 0 or 16
    // const uint shift = 8 * remainder;
    // const uint mask = 0xFFFF << shift;
    // index = (index & mask) >> shift;
    // return index;
}

VertexShaderOutput main(
    uint id : SV_VertexID
)
{
    VertexShaderOutput OUT;

    const Meshlet meshlet = _MeshletsBuffer[g_Model_Index];
    const uint index = LoadIndex(meshlet, id);
    const CommonVertexAttributes IN = _CommonVertexBuffer[meshlet.vertexOffset + index];

    OUT.NormalWS = mul((float3x3) g_Model_InverseTransposeModel, IN.normal);

    const float4 positionWS = mul(g_Model_Model, float4(IN.position, 1.0f));
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, positionWS);

    return OUT;
}

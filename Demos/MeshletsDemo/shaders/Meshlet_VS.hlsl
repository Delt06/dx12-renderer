#include "ShaderLibrary/Meshlets.hlsli"

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
    const uint address = AlignDown((meshlet.indexOffset + vertexId) * 2, 4, remainder);
    const uint index = _CommonIndexBuffer.Load<uint>(address);
    // the remainder is either 0 or 2
    const uint shift = remainder * 8;
    const uint mask = 0xFFFF << shift;
    return (index & mask) >> shift;
}

VertexShaderOutput main(
    uint id : SV_VertexID
)
{
    VertexShaderOutput OUT;

    const Meshlet meshlet = _MeshletsBuffer[g_Meshlet_Index];
    const uint index = LoadIndex(meshlet, id);
    const CommonVertexAttributes IN = _CommonVertexBuffer[meshlet.vertexOffset + index];

    Transform transform = _TransformsBuffer[meshlet.transformIndex];

    OUT.NormalWS = mul((float3x3) transform.inverseTransposeWorldMatrix, IN.normal.xyz);

    const float4 positionWS = mul(transform.worldMatrix, float4(IN.position.xyz, 1.0f));
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, positionWS);

    return OUT;
}

#include "ShaderLibrary/Pipeline.hlsli"
#include "Occluder_VertexShaderOutput.hlsli"

cbuffer CBuffer : register(b0)
{
    float4x4 _WorldMatrix;
    float4x4 _InverseTransposeWorldMatrix;
}

VertexShaderOutput main(const in CommonVertexAttributes IN)
{
    VertexShaderOutput OUT;
    OUT.NormalWS = mul((float3x3) _InverseTransposeWorldMatrix, IN.normal.xyz);

    const float4 positionWs = mul(_WorldMatrix, float4(IN.position.xyz, 1.0f));
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, positionWs);

    return OUT;
}

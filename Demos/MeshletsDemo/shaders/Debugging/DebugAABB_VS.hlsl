#include "../ShaderLibrary/Meshlets.hlsli"
#include "../ShaderLibrary/Geometry.hlsli"
#include "../ShaderLibrary/Pipeline.hlsli"

float2 UvToNdc(const float2 uv)
{
    float2 ndc = uv;
    ndc.y = 1 - ndc.y;
    ndc = ndc * 2 - 1;
    return ndc;
}

float4 main(const CommonVertexAttributes IN) : SV_POSITION
{
    const Meshlet meshlet = _MeshletsBuffer[g_Pipeline_SelectedMeshletIndex];
    const MeshletBounds bounds = meshlet.bounds;
    const Transform transform = _TransformsBuffer[meshlet.transformIndex];

    const float3 positionOs = IN.position.xyz * bounds.aabbHalfSize + bounds.aabbCenter;
    const float4 positionWs = mul(transform.worldMatrix, float4(positionOs, 1.0f));
    return mul(g_Pipeline_ViewProjection, positionWs);
}

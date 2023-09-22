#include "../ShaderLibrary/Meshlets.hlsli"
#include "../ShaderLibrary/Pipeline.hlsli"

float4 main(const CommonVertexAttributes IN) : SV_POSITION
{
    const Meshlet meshlet = _MeshletsBuffer[g_Pipeline_SelectedMeshletIndex];
    const MeshletBounds bounds = meshlet.bounds;
    const Transform transform = _TransformsBuffer[meshlet.transformIndex];
    const float3 positionOs = IN.position.xyz * bounds.radius + bounds.center;
    const float4 positionWs = mul(transform.worldMatrix, float4(positionOs, 1.0f));
    return mul(g_Pipeline_ViewProjection, positionWs);
}

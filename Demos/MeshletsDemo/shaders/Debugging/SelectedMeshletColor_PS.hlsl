#include "../ShaderLibrary/Meshlets.hlsli"
#include "../ShaderLibrary/Pipeline.hlsli"

float4 main() : SV_TARGET
{
    const float3 color = MESHLET_COLORS[g_Pipeline_SelectedMeshletIndex % MESHLET_COLORS_COUNT];
    return float4(color, 1);
}

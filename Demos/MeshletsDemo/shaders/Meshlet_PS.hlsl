#include "ShaderLibrary/Meshlets.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

#include "Meshlet_VertexShaderOutput.hlsli"

#define DEBUG_FLAGS 1

float4 main(const VertexShaderOutput IN): SV_TARGET
{
#if DEBUG_FLAGS == 1

    if ((g_Meshlet_Flags & MESHLET_FLAGS_PASSED_CONE_CULLING) == 0)
    {
        return float4(1, 0, 0, 1);
    }

    if ((g_Meshlet_Flags & MESHLET_FLAGS_PASSED_FRUSTUM_CULLING) == 0)
    {
        return float4(0, 1, 0, 1);
    }

    if ((g_Meshlet_Flags & MESHLET_FLAGS_PASSED_OCCLUSION_CULLING) == 0)
    {
        return float4(0, 0, 1, 1);
    }

#endif

    if (g_Pipeline_SelectedMeshletIndex == g_Meshlet_Index)
    {
        return float4(1, 1, 1, 1);
    }

    const float3 normalWs = normalize(IN.NormalWS);
    const float diffuse = saturate(dot(g_Pipeline_DirectionalLight.DirectionWs.xyz, normalWs)) * 0.5 + 0.5;
    const float3 albedo = MESHLET_COLORS[g_Meshlet_Index % MESHLET_COLORS_COUNT] * 0.5f;
    const float3 ambient = float3(0, 0.1f, 0.35f);
    return float4((ambient + diffuse) * albedo, 1);
}

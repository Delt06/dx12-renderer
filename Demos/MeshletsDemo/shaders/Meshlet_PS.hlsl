#include "ShaderLibrary/Meshlets.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

#include "Meshlet_VertexShaderOutput.hlsli"

#define MESHLET_COLORS_COUNT 5
#define DEBUG_FLAGS 1

const static float3 MESHLET_COLORS[MESHLET_COLORS_COUNT] =
{
    float3(1, 0, 0),
    float3(1, 1, 0),
    float3(1, 1, 1),
    float3(0, 1, 1),
    float3(0, 0, 1),
};

float4 main(const VertexShaderOutput IN): SV_TARGET
{
    #ifdef DEBUG_FLAGS
    if ((g_Meshlet_Flags & MESHLET_FLAGS_PASSED_CONE_CULLING) == 0)
    {
        return float4(1, 0, 0, 0);
    }
    #endif

    const float3 normalWs = normalize(IN.NormalWS);
    const float diffuse = dot(g_Pipeline_DirectionalLight.DirectionWs.xyz, normalWs) * 0.5 + 0.5;
    const float3 albedo = MESHLET_COLORS[g_Meshlet_Index % MESHLET_COLORS_COUNT] * 0.5f;
    const float3 ambient = float3(0, 0.1f, 0.35f);
    return float4((ambient + diffuse) * albedo , 1);
}

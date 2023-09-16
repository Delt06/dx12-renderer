#include "ShaderLibrary/Model.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

#include "Meshlet_VertexShaderOutput.hlsli"

#define MESHLET_COLORS_COUNT 5

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
    const float3 normalWS = normalize(IN.NormalWS);
    const float diffuse = dot(g_Pipeline_DirectionalLight.DirectionWs.xyz, normalWS) * 0.5 + 0.5;
    const float3 albedo = MESHLET_COLORS[g_Model_Index % MESHLET_COLORS_COUNT] * 0.75f;
    return float4(diffuse * albedo, 1);
}

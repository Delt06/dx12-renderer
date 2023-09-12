#include "ShaderLibrary/Pipeline.hlsli"

#include "Meshlet_VertexShaderOutput.hlsli"

float4 main(const VertexShaderOutput IN): SV_TARGET
{
    const float3 normalWS = normalize(IN.NormalWS);
    return dot(g_Pipeline_DirectionalLight.DirectionWs.xyz, normalWS);
}

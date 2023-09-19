#include "ShaderLibrary/Pipeline.hlsli"
#include "Occluder_VertexShaderOutput.hlsli"

float4 main(const in VertexShaderOutput IN) : SV_TARGET
{
    const float3 normalWs = normalize(IN.NormalWS);
    const float diffuse = saturate(dot(g_Pipeline_DirectionalLight.DirectionWs.xyz, normalWs)) * 0.5 + 0.5;
    const float3 color = diffuse * float3(1, 0, 0);
    return float4(color, 0.5f);
}

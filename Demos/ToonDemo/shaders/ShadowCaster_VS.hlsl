#include "ShaderLibrary/Pipeline.hlsli"
#include "ShaderLibrary/Model.hlsli"

struct VertexAttributes
{
    float3 PositionOs : POSITION;
    float3 Normal : NORMAL;
    float2 Uv : TEXCOORD;
};

// From Unity URP Shader Library
// com.unity.render-pipelines.universal@12.1.6/ShaderLibrary/Shadows.hlsl
float3 ApplyBias(float3 position, const float3 normal, const float3 lightDirection)
{
    float invNdotL = 1.0 - saturate(dot(lightDirection, normal));
    float scale = invNdotL * g_Pipeline_DirectionalLight_ShadowsBias.y;

	// normal bias is negative since we want to apply an inset normal offset
    position = lightDirection * g_Pipeline_DirectionalLight_ShadowsBias.xxx + position;
    position = normal * scale.xxx + position;
    return position;
}

float4 main(VertexAttributes IN) : SV_POSITION
{
    float3 positionWs = mul(g_Model_Model, float4(IN.PositionOs, 1.0f)).xyz;
    const float3 normalWs = normalize(mul((float3x3) g_Model_InverseTransposeModel, IN.Normal));

    const float3 lightDirectionWs = g_Pipeline_DirectionalLight.DirectionWs.xyz;
    positionWs = ApplyBias(positionWs, normalWs, lightDirectionWs);
    return mul(g_Pipeline_DirectionalLight_ViewProjection, float4(positionWs, 1.0f));
}

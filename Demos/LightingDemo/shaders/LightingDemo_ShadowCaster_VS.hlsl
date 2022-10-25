#include <ShaderLibrary/ShadowPassModel.hlsli>

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
    float scale = invNdotL * g_Model_Bias.y;

	// normal bias is negative since we want to apply an inset normal offset
    position = lightDirection * g_Model_Bias.xxx + position;
    position = normal * scale.xxx + position;
    return position;
}

float4 main(VertexAttributes IN) : SV_POSITION
{
    float3 positionWs = mul(g_Model_Model, float4(IN.PositionOs, 1.0f)).xyz;
    const float3 normalWs = normalize(mul((float3x3) g_Model_InverseTransposeModel, IN.Normal));

    const float3 lightDirectionWs = g_Model_LightType == 0
		                                ? g_Model_LightDirectionWs.xyz
		                                : normalize(g_Model_LightDirectionWs.xyz - positionWs);
    positionWs = ApplyBias(positionWs, normalWs, lightDirectionWs);
    return mul(g_Model_ViewProjection, float4(positionWs, 1.0f));
}

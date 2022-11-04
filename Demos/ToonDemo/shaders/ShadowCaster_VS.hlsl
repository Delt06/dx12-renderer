#include "ShaderLibrary/Pipeline.hlsli"
#include "ShaderLibrary/Model.hlsli"

struct VertexShaderInput
{
    float3 PositionOs : POSITION;
    float3 Normal : NORMAL;
    float2 Uv : TEXCOORD;
};

struct VertexShaderOutput
{
    float Depth : VIEW_Z;
    float4 PositionCS : SV_POSITION;
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

VertexShaderOutput main(VertexShaderInput IN)
{
    VertexShaderOutput OUT;

    float4 positionWs = mul(g_Model_Model, float4(IN.PositionOs, 1.0f));
    const float3 normalWs = normalize(mul((float3x3) g_Model_InverseTransposeModel, IN.Normal));

    const float3 lightDirectionWs = g_Pipeline_DirectionalLight.DirectionWs.xyz;
    positionWs = float4(ApplyBias(positionWs.xyz, normalWs, lightDirectionWs), 1.0);

    float depth = mul(g_Pipeline_DirectionalLight_View, positionWs).z;
    OUT.Depth = depth * GetShadowMapDepthScale();
    OUT.PositionCS = mul(g_Pipeline_DirectionalLight_ViewProjection, positionWs);

    return OUT;
}

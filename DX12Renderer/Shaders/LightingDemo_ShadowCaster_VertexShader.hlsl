struct ShadowPassParameters
{
	matrix Model;
	matrix InverseTransposeModel;
	matrix ViewProjection;
	float4 LightDirectionWs;
};

ConstantBuffer<ShadowPassParameters> parametersCb : register(b0);

struct VertexAttributes
{
	float3 PositionOs : POSITION;
	float3 Normal : NORMAL;
	float2 Uv : TEXCOORD;
};

// x - depth bias, y - normal bias (should be negative)
static const float2 SHADOW_BIAS = float2(-5.0f, -0.002f);

// From Unity URP Shader Library
// com.unity.render-pipelines.universal@12.1.6/ShaderLibrary/Shadows.hlsl
float3 ApplyBias(float3 position, const float3 normal, const float3 lightDirection)
{
	float invNdotL = 1.0 - saturate(dot(lightDirection, normal));
	float scale = invNdotL * SHADOW_BIAS.y;

	// normal bias is negative since we want to apply an inset normal offset
	position = lightDirection * SHADOW_BIAS.xxx + position;
	position = normal * scale.xxx + position;
	return position;
}

float4 main(VertexAttributes IN) : SV_POSITION
{
	float3 positionWs = mul(parametersCb.Model, float4(IN.PositionOs, 1.0f)).xyz;
	const float3 normalWs = normalize(mul((float3x3)parametersCb.InverseTransposeModel, IN.Normal));
	positionWs = ApplyBias(positionWs, normalWs, parametersCb.LightDirectionWs.xyz);
	return mul(parametersCb.ViewProjection, float4(positionWs, 1.0f));
}

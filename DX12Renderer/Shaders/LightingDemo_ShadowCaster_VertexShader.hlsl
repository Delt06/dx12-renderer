struct ShadowPassParameters
{
	matrix Model;
	matrix InverseTransposeModel;
	matrix ViewProjection;
	float4 LightDirectionWs;
	float4 Bias; // x - depth bias, y - normal bias (should be negative)
};

ConstantBuffer<ShadowPassParameters> parametersCb : register(b0);

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
	float scale = invNdotL * parametersCb.Bias.y;

	// normal bias is negative since we want to apply an inset normal offset
	position = lightDirection * parametersCb.Bias.xxx + position;
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

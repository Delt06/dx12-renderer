struct PixelShaderInput
{
	float4 PositionVs : POSITION;
	float3 NormalVs : NORMAL;
	float2 Uv : TEXCOORD;
};

struct Material
{
	float4 Emissive;
	float4 Ambient;
	float4 Diffuse;
	float4 Specular;
	float SpecularPower;
	float3 Padding;
};

struct DirectionalLight
{
	float4 DirectionVs;
	float4 Color;
};

struct LightingResult
{
	float4 Diffuse;
	float4 Specular;
};

ConstantBuffer<Material> materialCb : register(b0, space1);
ConstantBuffer<DirectionalLight> dirLightCb : register(b1);

Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

float Diffuse(const float3 n, const float3 l)
{
	return max(0, dot(n, l));
}

float Specular(const float3 v, const float3 n, const float3 l)
{
	const float3 r = normalize(reflect(-l, n));
	const float rDotV = max(0, dot(r, v));

	return pow(rDotV, materialCb.SpecularPower);
}

LightingResult Phong(const float3 positionVs, const float3 normalVs)
{
	LightingResult lightResult;

	const float3 viewDir = normalize(-positionVs);
	const float3 lightDir = dirLightCb.DirectionVs.xyz;
	lightResult.Diffuse = Diffuse(normalVs, lightDir) * dirLightCb.Color;
	lightResult.Specular = Specular(viewDir, normalVs, lightDir) * dirLightCb.Color;

	return lightResult;
}

float4 main(PixelShaderInput IN) : SV_Target
{
	const LightingResult result = Phong(IN.PositionVs.xyz, normalize(IN.NormalVs));

	const float4 emissive = materialCb.Emissive;
	const float4 ambient = materialCb.Ambient;
	const float4 diffuse = materialCb.Diffuse * result.Diffuse;
	const float4 specular = materialCb.Specular * result.Specular;
	const float4 texColor = diffuseTexture.Sample(diffuseSampler, IN.Uv);

	return emissive + (ambient + diffuse + specular) * texColor;
}

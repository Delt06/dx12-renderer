struct PixelShaderInput
{
	float4 PositionVs : POSITION;
	float3 NormalVs : NORMAL;
	float3 TangentVs : TANGENT;
	float3 BitangentVs : BINORMAL;
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

	bool HasDiffuseMap;
	bool HasNormalMap;
	bool HasSpecularMap;
	bool HasGlossMap;
};

struct DirectionalLight
{
	float4 DirectionVs;
	float4 Color;
};

struct LightResult
{
	float4 Diffuse;
	float4 Specular;
};

ConstantBuffer<Material> materialCb : register(b0, space1);
ConstantBuffer<DirectionalLight> dirLightCb : register(b1);

Texture2D diffuseMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D specularMap : register(t2);
Texture2D glossMap : register(t3);
SamplerState defaultSampler : register(s0);

float3 UnpackNormal(const float3 n)
{
	return n * 2.0f - 1.0f;
}

float3 ApplyNormalMap(const float3x3 tbn, Texture2D map, const float2 uv)
{
	const float3 normalTs = UnpackNormal(map.Sample(defaultSampler, uv).xyz);
	return normalize(mul(normalTs, tbn));
}

float Diffuse(const float3 n, const float3 l)
{
	return max(0, dot(n, l));
}

float Specular(const float3 v, const float3 n, const float3 l, const float specularPower)
{
	const float3 r = normalize(reflect(-l, n));
	const float rDotV = max(0, dot(r, v));

	return pow(rDotV, specularPower);
}

LightResult Phong(const float3 positionVs, const float3 normalVs, const float specularPower)
{
	LightResult lightResult;

	const float3 viewDir = normalize(-positionVs);
	const float3 lightDir = dirLightCb.DirectionVs.xyz;
	lightResult.Diffuse = Diffuse(normalVs, lightDir) * dirLightCb.Color;
	lightResult.Specular = Specular(viewDir, normalVs, lightDir, specularPower) * dirLightCb.Color;

	return lightResult;
}

float4 main(PixelShaderInput IN) : SV_Target
{
	float3 normalVs;
	if (materialCb.HasNormalMap)
	{
		const float3 tangent = normalize(IN.TangentVs);
		const float3 bitangent = normalize(IN.BitangentVs);
		const float3 normal = normalize(IN.NormalVs);

		const float3x3 tbn = float3x3(tangent,
		                              bitangent,
		                              normal);
		normalVs = ApplyNormalMap(tbn, normalMap, IN.Uv);
	}
	else
	{
		normalVs = normalize(IN.NormalVs);
	}


	float specularPower = materialCb.SpecularPower;
	if (materialCb.HasGlossMap)
	{
		specularPower *= (1 - glossMap.Sample(defaultSampler, IN.Uv).x);
	}

	const LightResult result = Phong(IN.PositionVs.xyz, normalVs, specularPower);

	const float4 emissive = materialCb.Emissive;
	const float4 ambient = materialCb.Ambient;
	const float4 diffuse = materialCb.Diffuse * result.Diffuse;
	float4 specular = materialCb.Specular * result.Specular;
	if (materialCb.HasSpecularMap)
	{
		specular *= specularMap.Sample(defaultSampler, IN.Uv).x;
	}
	const float4 texColor = materialCb.HasDiffuseMap
		                        ? diffuseMap.Sample(defaultSampler, IN.Uv)
		                        : float4(1.0f, 1.0f, 1.0f, 1.0f);

	return emissive + (ambient + diffuse + specular) * texColor;
}

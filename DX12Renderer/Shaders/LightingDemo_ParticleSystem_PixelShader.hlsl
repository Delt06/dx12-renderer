struct PixelInput
{
	float2 Uv : TEXCOORD;
	float4 Color : COLOR;
};

Texture2D albedoTexture : register(t0);

SamplerState textureSampler : register(s0);

float4 main(const PixelInput IN) : SV_TARGET
{
	const float4 albedo = albedoTexture.Sample(textureSampler, IN.Uv) * IN.Color;
	return albedo;
}

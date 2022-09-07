struct PixelInput
{
	float2 Uv : TEXCOORD;
};

Texture2D albedoTexture : register(t0);

SamplerState textureSampler : register(s0);

float4 main(PixelInput IN) : SV_TARGET
{
	return albedoTexture.Sample(textureSampler, IN.Uv).a;
}

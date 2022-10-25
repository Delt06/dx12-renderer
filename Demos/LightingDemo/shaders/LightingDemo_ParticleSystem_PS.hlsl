#include <ShaderLibrary/Common/RootSignature.hlsli>

struct PixelInput
{
	float2 Uv : TEXCOORD;
	float4 Color : COLOR;
};

Texture2D albedoTexture : register(t0);

float4 main(const PixelInput IN) : SV_TARGET
{
	const float4 albedo = albedoTexture.Sample(g_Common_LinearWrapSampler, IN.Uv) * IN.Color;
	return albedo;
}

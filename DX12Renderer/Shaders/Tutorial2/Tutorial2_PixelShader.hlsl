struct PixelShaderInput
{
	float4 Color : Color;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
	return IN.Color;
}

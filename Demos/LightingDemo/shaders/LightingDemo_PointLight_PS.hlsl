cbuffer CBuffer : register(b0) 
{
	float4 Color;
};

float4 main() : SV_TARGET
{
	return Color;
}

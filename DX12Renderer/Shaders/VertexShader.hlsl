struct VertexAppData
{
	float3 PositionOs : POSITION;
	float3 Color : COLOR;
};

cbuffer PerMaterial : register(b0)
{
	float4x4 Mvp;
};

struct VertexShaderOutput
{
	float4 Color : COLOR;
	float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAppData IN)
{
	VertexShaderOutput OUT;

	OUT.PositionCs = mul(Mvp, float4(IN.PositionOs, 1));
	OUT.Color = float4(IN.Color, 1);

	return OUT;
}

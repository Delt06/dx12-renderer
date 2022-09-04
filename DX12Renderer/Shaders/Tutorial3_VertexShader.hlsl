struct Matrices
{
	matrix Model;
	matrix ModelView;
	matrix InverseTransposeModelView;
	matrix ModelViewProjection;
};

ConstantBuffer<Matrices> matricesCb : register(b0);

struct VertexAttributes
{
	float3 PositionOs : POSITION;
	float3 Normal : NORMAL;
	float2 Uv : TEXCOORD;
};

struct VertexShaderOutput
{
	float4 PositionVs : POSITION;
	float3 NormalVs : NORMAL;
	float2 Uv : TEXCOORD;
	float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
	VertexShaderOutput OUT;

	OUT.PositionCs = mul(matricesCb.ModelViewProjection, float4(IN.Normal, 1.0f));
	OUT.PositionVs = mul(matricesCb.ModelView, float4(IN.PositionOs, 1.0f));
	OUT.NormalVs = mul((float3x3)matricesCb.InverseTransposeModelView, IN.Normal);
	OUT.Uv = IN.Uv;

	return OUT;
}

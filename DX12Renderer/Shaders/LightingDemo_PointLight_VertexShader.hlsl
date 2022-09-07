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
};

float4 main(float3 positionOs : POSITION) : SV_POSITION
{
	return mul(matricesCb.ModelViewProjection, float4(positionOs, 1.0f));
}

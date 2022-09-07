struct Matrices
{
	matrix Model;
	matrix ModelView;
	matrix InverseTransposeModelView;
	matrix ModelViewProjection;
	matrix View;
	matrix Projection;
};

ConstantBuffer<Matrices> matricesCb : register(b0);

struct VertexAttributes
{
	float3 PositionOs : POSITION;
	float3 Normal : NORMAL;
	float2 Uv : TEXCOORD;
};

struct VertexOutput
{
	float2 Uv : TEXCOORD;
	float4 PositionCs : SV_POSITION;
};

VertexOutput main(const VertexAttributes IN)
{
	VertexOutput OUT;

	// Billboard effect
	// https://gist.github.com/kaiware007/8ebad2d28638ff83b6b74970a4f70c9a
	float3 positionWs = mul((float3x3)matricesCb.Model, IN.PositionOs);
	const float4 pivotWs = float4(matricesCb.Model._m03, matricesCb.Model._m13, matricesCb.Model._m23, 1);
	const float4 positionVs = mul(matricesCb.View, pivotWs) + float4(positionWs, 0);
	OUT.PositionCs = mul(matricesCb.Projection, positionVs);

	OUT.Uv = IN.Uv;
	return OUT;
}

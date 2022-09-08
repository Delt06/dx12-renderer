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
	float3 InstancePivot : INSTANCE_PIVOT;
	float4 InstanceColor : INSTANCE_COLOR;
	float InstanceScale : INSTANCE_SCALE;
};

struct VertexOutput
{
	float2 Uv : TEXCOORD;
	float4 Color : COLOR;
	float4 PositionCs : SV_POSITION;
};

VertexOutput main(const VertexAttributes IN)
{
	VertexOutput OUT;

	OUT.Uv = IN.Uv;
	OUT.Color = IN.InstanceColor;

	// Billboard effect
	float3 vertexOffset = IN.PositionOs * IN.InstanceScale;
	const float4 pivotWs = float4(IN.InstancePivot, 1);
	const float4 positionVs = mul(matricesCb.View, pivotWs) + float4(vertexOffset, 0);
	OUT.PositionCs = mul(matricesCb.Projection, positionVs);

	return OUT;
}

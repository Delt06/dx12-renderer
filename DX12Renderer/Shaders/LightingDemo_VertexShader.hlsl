struct Matrices
{
	matrix Model;
	matrix ModelView;
	matrix InverseTransposeModelView;
	matrix ModelViewProjection;
};

struct ShadowMatrices
{
	matrix ViewProjection;
};

ConstantBuffer<Matrices> matricesCb : register(b0);
ConstantBuffer<ShadowMatrices> shadowMatricesCb : register(b1);

struct VertexAttributes
{
	float3 PositionOs : POSITION;
	float3 Normal : NORMAL;
	float2 Uv : TEXCOORD;
	float3 TangentOs : TANGENT;
	float3 BitangentOs : BINORMAL;
};

struct VertexShaderOutput
{
	float4 PositionVs : POSITION;
	float3 NormalVs : NORMAL;
	float3 TangentVs : TANGENT;
	float3 BitangentVs : BINORMAL;
	float2 Uv : TEXCOORD0;
	float4 ShadowCoords : SHADOW_COORD;
	float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
	VertexShaderOutput OUT;

	OUT.PositionCs = mul(matricesCb.ModelViewProjection, float4(IN.PositionOs, 1.0f));
	OUT.PositionVs = mul(matricesCb.ModelView, float4(IN.PositionOs, 1.0f));
	OUT.NormalVs = mul((float3x3)matricesCb.InverseTransposeModelView, IN.Normal);
	OUT.TangentVs = mul((float3x3)matricesCb.InverseTransposeModelView, IN.TangentOs);
	OUT.BitangentVs = mul((float3x3)matricesCb.InverseTransposeModelView, IN.BitangentOs);
	OUT.Uv = IN.Uv;

	// See http://www.opengl-tutorial.org/ru/intermediate-tutorials/tutorial-16-shadow-mapping/
	// "Using the shadow map"
	float4 shadowCoords = mul(shadowMatricesCb.ViewProjection, mul(matricesCb.Model, float4(IN.PositionOs, 1.0f)));
	shadowCoords.xy = shadowCoords.xyz * 0.5f + 0.5f; // [-1; 1] -> [0, 1]
	shadowCoords.y = 1 - shadowCoords.y;
	OUT.ShadowCoords = shadowCoords;

	return OUT;
}

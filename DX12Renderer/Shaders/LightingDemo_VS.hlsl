struct Matrices
{
    matrix Model;
    matrix ModelView;
    matrix InverseTransposeModelView;
    matrix ModelViewProjection;
    matrix View;
    matrix Projection;
    matrix InverseTransposeModel;
    float4 CameraPosition;
};

#include "Shadows.hlsli"

ConstantBuffer<Matrices> matricesCb : register(b0);
ConstantBuffer<ShadowReceiverParameters> shadowReceiverParameters : register(b3);

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
	float3 PositionWs : POSITION;
	float3 NormalWs : NORMAL;
	float3 TangentWs : TANGENT;
	float3 BitangentWs : BINORMAL;
	float2 Uv : TEXCOORD0;
	float4 ShadowCoords : SHADOW_COORD;
    float3 EyeWs : EYE_WS;
	float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
	VertexShaderOutput OUT;

	OUT.PositionCs = mul(matricesCb.ModelViewProjection, float4(IN.PositionOs, 1.0f));
    OUT.NormalWs = mul((float3x3) matricesCb.InverseTransposeModel, IN.Normal);
    OUT.TangentWs = mul((float3x3) matricesCb.InverseTransposeModel, IN.TangentOs);
    OUT.BitangentWs = mul((float3x3) matricesCb.InverseTransposeModel, IN.BitangentOs);
	OUT.Uv = IN.Uv;

	const float4 positionWs = mul(matricesCb.Model, float4(IN.PositionOs, 1.0f));
	OUT.PositionWs = positionWs.xyz;

	// See http://www.opengl-tutorial.org/ru/intermediate-tutorials/tutorial-16-shadow-mapping/
	// "Using the shadow map"
	OUT.ShadowCoords = HClipToShadowCoords(mul(shadowReceiverParameters.ViewProjection, positionWs));
	
    OUT.EyeWs = normalize(positionWs.xyz - matricesCb.CameraPosition.xyz);

	return OUT;
}

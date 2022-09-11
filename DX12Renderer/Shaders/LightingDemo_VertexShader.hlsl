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
	float4 PositionVs : POSITION;
	float3 NormalVs : NORMAL;
	float3 TangentVs : TANGENT;
	float3 BitangentVs : BINORMAL;
	float2 Uv : TEXCOORD0;
	float3 PositionWs : POSITION_WS;
	float4 ShadowCoords : SHADOW_COORD;
    float3 NormalWs : NORMAL_WS;
    float3 EyeWs : EYE_WS;
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

	const float4 positionWs = mul(matricesCb.Model, float4(IN.PositionOs, 1.0f));
	OUT.PositionWs = positionWs.xyz;

	// See http://www.opengl-tutorial.org/ru/intermediate-tutorials/tutorial-16-shadow-mapping/
	// "Using the shadow map"
	OUT.ShadowCoords = HClipToShadowCoords(mul(shadowReceiverParameters.ViewProjection, positionWs));
	
    OUT.NormalWs = mul((float3x3) matricesCb.InverseTransposeModel, IN.Normal);
    OUT.EyeWs = normalize(positionWs.xyz - matricesCb.CameraPosition.xyz);

	return OUT;
}

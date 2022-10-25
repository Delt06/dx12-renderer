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

#include <ShaderLibrary/Shadows.hlsli>
#include <ShaderLibrary/Model.hlsli>
#include <ShaderLibrary/Pipeline.hlsli>

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

    OUT.PositionCs = mul(g_Model_ModelViewProjection, float4(IN.PositionOs, 1.0f));
    OUT.NormalWs = mul((float3x3) g_Model_InverseTransposeModel, IN.Normal);
    OUT.TangentWs = mul((float3x3) g_Model_InverseTransposeModel, IN.TangentOs);
    OUT.BitangentWs = mul((float3x3) g_Model_InverseTransposeModel, IN.BitangentOs);
    OUT.Uv = IN.Uv;

    const float4 positionWs = mul(g_Model_Model, float4(IN.PositionOs, 1.0f));
    OUT.PositionWs = positionWs.xyz;

	// See http://www.opengl-tutorial.org/ru/intermediate-tutorials/tutorial-16-shadow-mapping/
	// "Using the shadow map"
    OUT.ShadowCoords = HClipToShadowCoords(mul(g_Pipeline_DirectionalLightViewProjection, positionWs));
	
    OUT.EyeWs = normalize(positionWs.xyz - g_Pipeline_CameraPosition.xyz);

    return OUT;
}

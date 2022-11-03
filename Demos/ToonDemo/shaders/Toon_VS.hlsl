#include "ShaderLibrary/Model.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

struct VertexAttributes
{
    float3 PositionOS : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

struct VertexShaderOutput
{
    float3 NormalWS : NORMAL;
    float2 UV : TEXCOORD;
    float3 EyeWS : EYE_WS;
    float4 PositionCS : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;

    OUT.NormalWS = mul((float3x3) g_Model_InverseTransposeModel, IN.Normal);
    OUT.PositionCS = mul(g_Model_ModelViewProjection, float4(IN.PositionOS, 1.0f));
    OUT.UV = IN.UV;

    float3 positionWS = mul(g_Model_Model, float4(IN.PositionOS, 1.0f)).xyz;
    OUT.EyeWS = normalize(positionWS.xyz - g_Pipeline_CameraPosition.xyz);

    return OUT;
}

#include <ShaderLibrary/Shadows.hlsli>

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
    float4 ShadowCoords : SHADOW_COORDS;
    float4 PositionCS : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;

    OUT.NormalWS = mul((float3x3) g_Model_InverseTransposeModel, IN.Normal);
    OUT.PositionCS = mul(g_Model_ModelViewProjection, float4(IN.PositionOS, 1.0f));
    OUT.UV = IN.UV;

    float4 positionWS = mul(g_Model_Model, float4(IN.PositionOS, 1.0f));
    OUT.EyeWS = normalize(positionWS.xyz - g_Pipeline_CameraPosition.xyz);

    OUT.ShadowCoords = HClipToShadowCoords(mul(g_Pipeline_DirectionalLight_ViewProjection, positionWS));

    return OUT;
}

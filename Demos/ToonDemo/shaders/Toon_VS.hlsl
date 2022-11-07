#include <ShaderLibrary/Shadows.hlsli>

#include "ShaderLibrary/Model.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"
#include "ToonVaryings.hlsli"

struct VertexAttributes
{
    float3 PositionOS : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

ToonVaryings main(VertexAttributes IN)
{
    ToonVaryings OUT;

    OUT.NormalWS = mul((float3x3) g_Model_InverseTransposeModel, IN.Normal);
    OUT.PositionCS = mul(g_Model_ModelViewProjection, float4(IN.PositionOS, 1.0f));
    OUT.UV = IN.UV;

    float4 positionWS = mul(g_Model_Model, float4(IN.PositionOS, 1.0f));
    OUT.EyeWS = normalize(positionWS.xyz - g_Pipeline_CameraPosition.xyz);

    float4 shadowCoords = HClipToShadowCoords(mul(g_Pipeline_DirectionalLight_ViewProjection, positionWS));
    float4 shadowCoordsVS = mul(g_Pipeline_DirectionalLight_View, positionWS);
    OUT.ShadowCoords = float4(shadowCoords.xy, shadowCoordsVS.z * GetShadowMapDepthScale(), 1.0);

    OUT.PositionVS = mul(g_Pipeline_View, positionWS);

    return OUT;
}

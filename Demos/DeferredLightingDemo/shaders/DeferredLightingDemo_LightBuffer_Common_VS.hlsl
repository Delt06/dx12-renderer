#include "ShaderLibrary/Model.hlsli"

struct VertexShaderInput
{
    float3 PositionOS : POSITION;
};

struct VertexShaderOutput
{
    float3 PositionWS : POSITION;
    float4 PositionCS : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput IN)
{
    float4 positionOS = float4(IN.PositionOS, 1.0);
    float3 positionWS = mul(g_Model_Model, positionOS).xyz;
    float4 positionCS = mul(g_Model_ModelViewProjection, positionOS);
    
    VertexShaderOutput OUT;
    OUT.PositionWS = positionWS;
    OUT.PositionCS = positionCS;
    
    return OUT;
}

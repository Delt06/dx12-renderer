#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "ShaderLibrary/Pipeline.hlsli"
#include "GrassModel.hlsli"
#include "GrassConstants.hlsli"
#include "GrassVaryings.hlsli"

StructuredBuffer<GrassModel> _GrassModelsSB : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

struct VertexAttributes
{
    float3 PositionOS : POSITION;
};

Varyings main(VertexAttributes IN)
{
    Varyings OUT;

    const float4 positionWS = mul(_GrassModelsSB[_Index].Model, float4(IN.PositionOS, 1.0));
    const float4 positionCS = mul(g_Pipeline_ViewProjection, positionWS);
    float4 jitterPositionCS = positionCS;
    jitterPositionCS.xy += g_Pipeline_TAA_JitterOffset * positionCS.w;
    OUT.PositionCS = jitterPositionCS;

    OUT.CurrentPositionCS = positionCS;
    OUT.PreviousPositionCS = mul(g_Pipeline_TAA_PreviousViewProjection, positionWS);

    return OUT;
}

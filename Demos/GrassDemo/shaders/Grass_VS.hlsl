#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "ShaderLibrary/Pipeline.hlsli"
#include "GrassModel.hlsli"
#include "GrassConstants.hlsli"

StructuredBuffer<GrassModel> _GrassModelsSB : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

struct VertexAttributes
{
    float3 PositionOS : POSITION;
};

struct Varyings
{
    float4 PositionCS : SV_POSITION;
};

Varyings main(VertexAttributes IN)
{
    Varyings OUT;
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, mul(_GrassModelsSB[_Index].Model, float4(IN.PositionOS, 1.0)));
    return OUT;
}

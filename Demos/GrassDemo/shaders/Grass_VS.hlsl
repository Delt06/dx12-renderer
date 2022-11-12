#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "ShaderLibrary/Pipeline.hlsli"
#include "GrassModel.hlsli"

ConstantBuffer<GrassModel> _GrassModelCB : register(b0, COMMON_ROOT_SIGNATURE_MODEL_SPACE);

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
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, mul(_GrassModelCB.Model, float4(IN.PositionOS, 1.0)));
    return OUT;
}

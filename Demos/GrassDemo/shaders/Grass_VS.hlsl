#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "ShaderLibrary/Pipeline.hlsli"

cbuffer Model : register(b0, COMMON_ROOT_SIGNATURE_MODEL_SPACE)
{
    matrix g_Model;
};

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
    OUT.PositionCS = mul(g_Pipeline_ViewProjection, mul(g_Model, float4(IN.PositionOS, 1.0)));
    return OUT;
}

#include <ShaderLibrary/TAA.hlsli>

#include "GrassConstants.hlsli"
#include "GrassVaryings.hlsli"

StructuredBuffer<float4> _Colors : register(t1, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

struct Targets
{
    float4 Color : SV_TARGET0;
    float4 Velocity : SV_TARGET1;
};

Targets main(Varyings IN)
{
    Targets OUT;
    OUT.Color = lerp(_Colors[_Index], 0, IN.UV.y);
    OUT.Velocity.xy = CalculateVelocity(IN.CurrentPositionCS, IN.PreviousPositionCS);
    return OUT;
}

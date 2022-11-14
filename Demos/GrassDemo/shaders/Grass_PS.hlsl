#include "GrassConstants.hlsli"

StructuredBuffer<float4> _Colors : register(t1, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

float4 main() : SV_TARGET
{
    return _Colors[_Index];
}

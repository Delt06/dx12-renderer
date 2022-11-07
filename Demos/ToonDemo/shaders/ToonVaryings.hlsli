#ifndef TOON_VARYINGS
#define TOON_VARYINGS

struct ToonVaryings
{
    float3 NormalWS : NORMAL;
    float2 UV : TEXCOORD;
    float3 EyeWS : EYE_WS;
    float4 PositionVS : POSITION_VS;
    float4 ShadowCoords : SHADOW_COORDS;
    float4 PositionCS : SV_POSITION;
};

#endif

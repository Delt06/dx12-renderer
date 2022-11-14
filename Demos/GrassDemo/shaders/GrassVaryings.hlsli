#ifndef GRASS_VARYINGS_HLSLI
#define GRASS_VARYINGS_HLSLI

struct Varyings
{
    float4 CurrentPositionCS : CURRENT_POSITION_CS;
    float4 PreviousPositionCS : PREVIOUS_POSITION_CS;

    float4 PositionCS : SV_POSITION;
};

#endif

#ifndef COLOR_SPLIT_HLSLI
#define COLOR_SPLIT_HLSLI

struct ColorSplitBufferEntry
{
    float2 UvOffset;
    float2 _Padding1;
    float3 ColorMask;
    float _Padding2;
};

#endif

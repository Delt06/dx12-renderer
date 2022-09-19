#ifndef TAA_BUFFER_HLSLI
#define TAA_BUFFER_HLSLI

struct TAABuffer
{
    matrix PreviousModelViewProjection;
    
    float2 JitterOffset;
    float2 _Padding;
};

#endif 
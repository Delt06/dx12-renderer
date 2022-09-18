// https://www.alextardif.com/HistogramLuminance.html

#include "ShaderLibrary/HDR.hlsli"

#define NUM_HISTOGRAM_BINS 256
#define HISTOGRAM_THREADS_PER_DIMENSION 16

#define EPSILON 0.0000001

Texture2D HDRTexture : register(t0);
RWByteAddressBuffer LuminanceHistogram : register(u0);

cbuffer LuminanceHistogramParametersCB : register(b0)
{
    uint inputWidth;
    uint inputHeight;
    float minLogLuminance;
    float oneOverLogLuminanceRange; 
};

groupshared uint HistogramShared[NUM_HISTOGRAM_BINS];

uint HDRToHistogramBin(float3 hdrColor)
{
    float luminance = GetLuminance(hdrColor);
    
    if (luminance < EPSILON)
    {
        return 0;
    }
    
    float logLuminance = saturate((log2(luminance) - minLogLuminance) * oneOverLogLuminanceRange);
    return (uint) (logLuminance * (NUM_HISTOGRAM_BINS - 2) + 1.0);

}

[numthreads(HISTOGRAM_THREADS_PER_DIMENSION, HISTOGRAM_THREADS_PER_DIMENSION, 1)]
void main(uint groupIndex : SV_GroupIndex, uint3 threadId : SV_DispatchThreadID)
{
    HistogramShared[groupIndex] = 0;
    
    GroupMemoryBarrierWithGroupSync();
    
    if (threadId.x < inputWidth && threadId.y < inputHeight)
    {
        float3 hdrColor = HDRTexture.Load(int3(threadId.xy, 0)).rgb;
        uint binIndex = HDRToHistogramBin(hdrColor);
        InterlockedAdd(HistogramShared[binIndex], 1);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    uint originalValue; // unused
    LuminanceHistogram.InterlockedAdd(groupIndex * 4, HistogramShared[groupIndex], originalValue);
}
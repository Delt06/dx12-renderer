#define NUM_HISTOGRAM_BINS 256

RWByteAddressBuffer LuminanceHistogram : register(u0);
RWTexture2D<float> LuminanceOutput : register(u1);

cbuffer LuminanceHistogramParametersCB : register(b0)
{
    uint PixelCount;
    float MinLogLuminance;
    float LogLuminanceRange;
    float DeltaTime;
    
    float Tau;
    float3 _Padding;
};

groupshared float HistogramShared[NUM_HISTOGRAM_BINS];


[numthreads(NUM_HISTOGRAM_BINS, 1, 1)]
void main(uint3 localThreadIndex : SV_GroupThreadID)
{
    uint localIndex = localThreadIndex.x;
    float countForThisBin = (float) LuminanceHistogram.Load(localIndex * 4);
    HistogramShared[localIndex] = countForThisBin * (float) localIndex;
    
    GroupMemoryBarrierWithGroupSync();
    
    [unroll]
    for (uint histogramSampleIndex = (NUM_HISTOGRAM_BINS >> 1); histogramSampleIndex > 0; histogramSampleIndex >>= 1)
    {
        if (localIndex < histogramSampleIndex)
        {
            HistogramShared[localIndex] += HistogramShared[localIndex + histogramSampleIndex];
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
    if (localIndex == 0)
    {
        float weightedLogAverage = (HistogramShared[0] / max((float) PixelCount - countForThisBin, 1.0)) - 1.0;
        float weightedAverageLuminance = exp2(((weightedLogAverage / (NUM_HISTOGRAM_BINS - 2)) * LogLuminanceRange) + MinLogLuminance);
        float luminanceLastFrame = LuminanceOutput[uint2(0, 0)];
        float adaptedLuminance = luminanceLastFrame + (weightedAverageLuminance - luminanceLastFrame) * (1 - exp(-DeltaTime * Tau));
        LuminanceOutput[uint2(0, 0)] = adaptedLuminance;
    }
}
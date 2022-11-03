Texture2DMS<float> input : register(t0);

RWTexture2D<float> output : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 dim;
    uint sampleCount;
    input.GetDimensions(dim.x, dim.y, sampleCount);

    if (dispatchThreadId.x > dim.x || dispatchThreadId.y > dim.y)
    {
        return;
    }

    float result = 1;

    for (uint i = 0; i < sampleCount; ++i)
    {
        result = min(result, input.Load(dispatchThreadId.xy, i).r);
    }

    output[dispatchThreadId.xy] = result;
}

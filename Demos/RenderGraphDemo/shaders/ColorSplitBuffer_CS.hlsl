#define THREAD_BLOCK_SIZE 16

#include "ShaderLibrary/ColorSplit.hlsli"

AppendStructuredBuffer<ColorSplitBufferEntry> _ColorSplitBuffer : register(u0);

cbuffer CBuffer : register(b0)
{
    uint64_t _FrameIndex;
};

void AppendEntry(float2 uvOffset, float3 colorMask)
{
    ColorSplitBufferEntry entry = (ColorSplitBufferEntry) 0;
    entry.UvOffset = uvOffset;
    entry.ColorMask = colorMask;
    _ColorSplitBuffer.Append(entry);
}

const static float3 MASKS[3][3] =
{
    {float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1)},
    {float3(0.5f, 0.5f, 0), float3(0, 0.25f, 0.5f), float3(0.5f, 0.25f, 0.5f)},
    {float3(0.3333f, 0.5f, 0), float3(0.3333f, 0.5f, 0.0f), float3(0.3333f, 0, 1.0f)},
};

[numthreads(THREAD_BLOCK_SIZE, 1, 1)]
void main(in uint3 dispatchID : SV_DispatchThreadID)
{
    uint index = dispatchID.x;
    if (index >= 3)
    {
        return;
    }

    uint64_t setIndex = (_FrameIndex / 128) % 3;

    switch(index)
    {
        case 0:
            AppendEntry(5.0f, MASKS[setIndex][0]);
            break;
        case 1:
            AppendEntry(-2.0f, MASKS[setIndex][1]);
            break;
        case 2:
            AppendEntry(float2(-0.5f, 3.0f), MASKS[setIndex][2]);
            break;
    }
}

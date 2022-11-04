#include <ShaderLibrary/Common/RootSignature.hlsli>
#include "ShaderLibrary/Pipeline.hlsli"
#include "ShaderLibrary/Normals.hlsli"

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

cbuffer Parameters : register(b0)
{
    float4 outlineColor;
    float depthThreshold;
    float normalsThreshold;
};

Texture2D sourceColor : register(t0);
Texture2D sourceDepth : register(t1);
Texture2D sourceNormals : register(t2);

// https://www.prkz.de/blog/1-linearizing-depth-buffer-samples-in-hlsl
float GetDepthLinear(float nonLinearDepth)
{
    // We are only interested in the depth here
    const float4 ndcCoords = float4(0, 0, nonLinearDepth, 1.0f);
    // Unproject the vector into (homogenous) view-space vector
    const float4 viewCoords = mul(g_Pipeline_InverseProjection, ndcCoords);
    // Divide by w, which results in actual view-space z value
    const float linearDepth = viewCoords.z / viewCoords.w;
    return linearDepth;
}

float SampleDepth(float2 uv)
{
    return sourceDepth.Sample(g_Common_PointClampSampler, uv).r;
}

float SampleLinearDepth(float2 uv)
{
    return GetDepthLinear(SampleDepth(uv));
}

float4 SampleColor(float2 uv)
{
    return sourceColor.Sample(g_Common_PointClampSampler, uv);
}

float3 SampleNormal(float2 uv)
{
    return UnpackNormal(sourceNormals.Sample(g_Common_PointClampSampler, uv).xyz);
}

#define KERNEL_SIZE 9

// https://alexanderameye.github.io/notes/rendering-outlines/
static const int SobelX[KERNEL_SIZE] =
{
    0, 0, 0,
    1, 0, -1,
    0, 0, 0
};

static const int SobelY[KERNEL_SIZE] =
{
    0, 1, 0,
    0, 0, 0,
    0, -1, 0
};

static const float2 SampleOffsets[KERNEL_SIZE] =
{
    float2(-1, 1), float2(0, 1), float2(1, 1),
    float2(-1, 0), float2(0, 0), float2(1, 0),
    float2(-1, -1), float2(0, -1), float2(1, -1),
};

float SampleDepthEdge(float2 uv)
{
    uint i;
    float horizontal = 0;
    float threshold = depthThreshold * SampleDepth(uv);

    [unroll]
    for (i = 0; i < KERNEL_SIZE; ++i)
    {
        horizontal += SampleDepth(uv + g_Pipeline_Screen_TexelSize * SampleOffsets[i]) * SobelX[i];
    }

    float vertical = 0;
    [unroll]
    for (i = 0; i < KERNEL_SIZE; ++i)
    {
        vertical += SampleDepth(uv + g_Pipeline_Screen_TexelSize * SampleOffsets[i]) * SobelY[i];
    }

    return step(threshold, length(float2(horizontal, vertical)));
}

float SampleNormalsEdge(float2 uv)
{
    uint i;
    float3 horizontal = 0;

    [unroll]
    for (i = 0; i < KERNEL_SIZE; ++i)
    {
        horizontal += SampleNormal(uv + g_Pipeline_Screen_TexelSize * SampleOffsets[i]) * SobelX[i];
    }

    float3 vertical = 0;
    [unroll]
    for (i = 0; i < KERNEL_SIZE; ++i)
    {
        vertical += SampleNormal(uv + g_Pipeline_Screen_TexelSize * SampleOffsets[i]) * SobelY[i];
    }

    return step(normalsThreshold, sqrt(dot(horizontal, horizontal) + dot(vertical, vertical)));
}

float4 main(PixelShaderInput IN) : SV_Target
{
    float depthEdge = SampleDepthEdge(IN.UV);
    float normalsEdge = SampleNormalsEdge(IN.UV);
    float combinedEdge = max(depthEdge, normalsEdge);
    return lerp(SampleColor(IN.UV), outlineColor, combinedEdge);
}

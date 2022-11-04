// Source: https://catlikecoding.com/unity/tutorials/custom-srp/fxaa/

#include <ShaderLibrary/Common/RootSignature.hlsli>
#include <ShaderLibrary/HDR.hlsli>

#include "ShaderLibrary/Pipeline.hlsli"
#include "ShaderLibrary/Normals.hlsli"

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

cbuffer Parameters : register(b0)
{
    float fixedContrastThreshold;
    float relativeContrastThreshold;
    float subpixelBlendingFactor;
};

Texture2D sourceTexture : register(t0);

float4 SampleSceneColor(float2 uv)
{
    return sourceTexture.Sample(g_Common_LinearClampSampler, uv);
}

float GetSceneLuminance(float2 uv, float uOffset = 0.0, float vOffset = 0.0)
{
    uv += float2(uOffset, vOffset) * g_Pipeline_Screen_TexelSize;
    return sqrt(GetLuminance(SampleSceneColor(uv).rgb));
}

struct LuminanceNeighborhood
{
    float Center;
    float North;
    float East;
    float South;
    float West;

    float NorthEast;
    float SouthEast;
    float NorthWest;
    float SouthWest;


    float Highest;
    float Lowest;
    float Range;
};

LuminanceNeighborhood GetLumianceNeighborhood(float2 uv)
{
    LuminanceNeighborhood luminance;

    luminance.Center = GetSceneLuminance(uv);
    luminance.North = GetSceneLuminance(uv, 0, 1);
    luminance.East = GetSceneLuminance(uv, 1, 0);
    luminance.South = GetSceneLuminance(uv, 0, -1);
    luminance.West = GetSceneLuminance(uv, -1, 0);

    luminance.NorthEast = GetSceneLuminance(uv, 1, 1);
    luminance.SouthEast = GetSceneLuminance(uv, 1, -1);
    luminance.NorthWest = GetSceneLuminance(uv, -1, 1);
    luminance.SouthWest = GetSceneLuminance(uv, -1, -1);

    luminance.Highest = max(luminance.Center, max(luminance.North, max(luminance.East, max(luminance.South, luminance.West))));
    luminance.Lowest = min(luminance.Center, min(luminance.North, min(luminance.East, min(luminance.South, luminance.West))));
    luminance.Range = luminance.Highest - luminance.Lowest;

    return luminance;
}

bool CanSkipFXAA(in LuminanceNeighborhood luminance)
{
    return luminance.Range < max(fixedContrastThreshold, relativeContrastThreshold * luminance.Highest);
}

float GetSubpixelBlendFactor(in LuminanceNeighborhood luminance)
{
    float filter = 2.0 * (luminance.North + luminance.East + luminance.South + luminance.West);
    filter += luminance.NorthEast + luminance.NorthWest + luminance.SouthEast + luminance.SouthWest;
    filter *= 1.0f / 12.0f;
    filter = abs(filter - luminance.Center);
    filter = saturate(filter / luminance.Range);
    filter = smoothstep(0, 1, filter);
    return filter * filter * subpixelBlendingFactor;
}

bool IsHorizontalEdge(LuminanceNeighborhood luminance)
{
    float horizontal =
        2.0 * abs(luminance.North + luminance.South - 2.0 * luminance.Center) +
        abs(luminance.NorthEast + luminance.SouthEast - 2.0 * luminance.East) +
        abs(luminance.NorthWest + luminance.SouthWest - 2.0 * luminance.West)
        ;
    float vertical =
        2.0 * abs(luminance.East + luminance.West - 2.0 * luminance.Center) +
        abs(luminance.NorthEast + luminance.NorthWest - 2.0 * luminance.North) +
        abs(luminance.SouthEast + luminance.SouthWest - 2.0 * luminance.South)
        ;
    return horizontal >= vertical;
}

struct Edge
{
    bool IsHorizontal;
    float PixelStep;
};

Edge GetEdge(in LuminanceNeighborhood lumiance)
{
    Edge edge;
    edge.IsHorizontal = IsHorizontalEdge(lumiance);

    float luminancePositive, luminanceNegative;
    if (edge.IsHorizontal)
    {
        edge.PixelStep = g_Pipeline_Screen_TexelSize.y;
        luminancePositive = lumiance.North;
        luminanceNegative = lumiance.South;
    }
    else
    {
        edge.PixelStep = g_Pipeline_Screen_TexelSize.x;
        luminancePositive = lumiance.East;
        luminanceNegative = lumiance.West;
    }

    float gradientPositive = abs(luminancePositive - lumiance.Center);
    float gradientNegative = abs(luminanceNegative - lumiance.Center);

    if (gradientPositive < gradientNegative)
    {
        edge.PixelStep = -edge.PixelStep;
    }

    return edge;
}

float4 main(PixelShaderInput IN) : SV_Target
{
    const float2 uv = IN.UV;
    LuminanceNeighborhood luminance = GetLumianceNeighborhood(uv);

    if (CanSkipFXAA(luminance))
    {
        return SampleSceneColor(uv);
    }

    Edge edge = GetEdge(luminance);
    float blendFactor = GetSubpixelBlendFactor(luminance);
    float2 blendUV = uv;
    if (edge.IsHorizontal)
    {
        blendUV.y += blendFactor * edge.PixelStep;
    }
    else
    {
        blendUV.x += blendFactor * edge.PixelStep;
    }

    return SampleSceneColor(blendUV);
}

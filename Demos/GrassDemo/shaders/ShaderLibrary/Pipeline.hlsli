#ifndef PIPELINE_HLSLI
#define PIPELINE_HLSLI

#include <ShaderLibrary/Common/RootSignature.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>

struct DirectionalLight
{
    float4 DirectionWs;
    float4 Color;
};

cbuffer PipelineCBuffer : register(b0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE)
{
    matrix g_Pipeline_View;
    matrix g_Pipeline_Projection;
    matrix g_Pipeline_ViewProjection;

    matrix g_Pipeline_TAA_PreviousViewProjection;
    float2 g_Pipeline_TAA_JitterOffset;
};

#endif

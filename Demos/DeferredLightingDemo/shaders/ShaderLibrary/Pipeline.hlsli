#ifndef PIPELINE_HLSLI
#define PIPELINE_HLSLI

#include <ShaderLibrary/Common/RootSignature.hlsli>

cbuffer PipelineCBuffer : register(b0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE)
{
    matrix g_Pipeline_View;
    matrix g_Pipeline_Projection;
    matrix g_Pipeline_ViewProjection;

    float2 g_Pipeline_Taa_JitterOffset;
    float _g_Pipeline_Padding[2];
};

#endif
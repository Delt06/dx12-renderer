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

    float4 g_Pipeline_CameraPosition;

    matrix g_Pipeline_InverseView;
    matrix g_Pipeline_InverseProjection;

    float2 g_Pipeline_Screen_Resolution;
    float2 g_Pipeline_Screen_TexelSize;

    matrix g_Pipeline_DirectionalLight_View;
    matrix g_Pipeline_DirectionalLight_ViewProjection;
    float4 g_Pipeline_DirectionalLight_ShadowsBias; // x - depth, y - normal, z - shadow map depth scale

    DirectionalLight g_Pipeline_DirectionalLight;
};

ScreenParameters GetScreenParameters()
{
    ScreenParameters result;
    result.ScreenSize = g_Pipeline_Screen_Resolution;
    result.OneOverScreenSize = g_Pipeline_Screen_TexelSize;
    return result;
}

float GetShadowMapDepthScale()
{
    return g_Pipeline_DirectionalLight_ShadowsBias.z;
}

#endif

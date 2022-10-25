#ifndef PIPELINE_HLSLI
#define PIPELINE_HLSLI

#include <ShaderLibrary/Common/RootSignature.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>
#include <ShaderLibrary/Shadows.hlsli>

struct DirectionalLight
{
    float4 DirectionWs; // update on CPU
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
    
    matrix g_Pipeline_DirectionalLightViewProjection;
    
    DirectionalLight g_Pipeline_DirectionalLight;

    uint g_Pipeline_NumPointLights;
    uint g_Pipeline_NumSpotLights;
    
    ShadowReceiverParameters g_Pipeline_ShadowReceiverParameters;
};

ScreenParameters GetScreenParameters()
{
    ScreenParameters result;
    result.ScreenSize = g_Pipeline_Screen_Resolution;
    result.OneOverScreenSize = g_Pipeline_Screen_TexelSize;
    return result;
}

#endif
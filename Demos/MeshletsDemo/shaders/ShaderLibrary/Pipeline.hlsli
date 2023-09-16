#ifndef PIPELINE_HLSLI
#define PIPELINE_HLSLI

#include <ShaderLibrary/Common/RootSignature.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>
#include <ShaderLibrary/Shadows.hlsli>

#include "CommonVertexAttributes.hlsli"

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

    DirectionalLight g_Pipeline_DirectionalLight;
};

ScreenParameters GetScreenParameters()
{
    ScreenParameters result;
    result.ScreenSize = g_Pipeline_Screen_Resolution;
    result.OneOverScreenSize = g_Pipeline_Screen_TexelSize;
    return result;
}

struct MeshletBounds
{
    /* bounding sphere, useful for frustum and occlusion culling */
    float3 center;
    float radius;

    /* normal cone, useful for backface culling */
    float3 coneApex;
    float3 coneAxis;
    float coneCutoff; /* = cos(angle/2) */
};

struct Meshlet
{
    MeshletBounds bounds;

    uint32_t vertexOffset;
    uint32_t triangleOffset;

    uint32_t vertexCount;
    uint32_t triangleCount;
};

StructuredBuffer<CommonVertexAttributes> _CommonVertexBuffer : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
ByteAddressBuffer _CommonIndexBuffer : register(t1, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
StructuredBuffer<Meshlet> _MeshletsBuffer : register(t2, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);


#endif

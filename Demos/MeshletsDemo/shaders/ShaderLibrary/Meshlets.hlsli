#ifndef MESHLETS_HLSLI
#define MESHLETS_HLSLI

#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "CommonVertexAttributes.hlsli"

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

    uint vertexOffset;
    uint indexOffset;

    uint vertexCount;
    uint indexCount;
};

StructuredBuffer<CommonVertexAttributes> _CommonVertexBuffer : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
ByteAddressBuffer _CommonIndexBuffer : register(t1, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
StructuredBuffer<Meshlet> _MeshletsBuffer : register(t2, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

#endif

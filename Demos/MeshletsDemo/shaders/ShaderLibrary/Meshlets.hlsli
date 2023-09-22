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

    uint transformIndex;

    uint vertexOffset;
    uint indexOffset;

    uint vertexCount;
    uint indexCount;
};

struct Transform
{
    float4x4 worldMatrix;
    float4x4 inverseTransposeWorldMatrix;
};

StructuredBuffer<CommonVertexAttributes> _CommonVertexBuffer : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
ByteAddressBuffer _CommonIndexBuffer : register(t1, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
StructuredBuffer<Meshlet> _MeshletsBuffer : register(t2, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
StructuredBuffer<Transform> _TransformsBuffer : register(t3, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

ROOT_CONSTANTS_BEGIN
    uint g_Meshlet_Index;
    uint g_Meshlet_Flags;
ROOT_CONSTANTS_END

const static uint MESHLET_FLAGS_PASSED_CONE_CULLING = 1 << 0;
const static uint MESHLET_FLAGS_PASSED_FRUSTUM_CULLING = 1 << 1;
const static uint MESHLET_FLAGS_PASSED_OCCLUSION_CULLING = 1 << 2;

#define MESHLET_COLORS_COUNT 5

const static float3 MESHLET_COLORS[MESHLET_COLORS_COUNT] =
{
    float3(1, 0, 0),
    float3(1, 1, 0),
    float3(1, 1, 1),
    float3(0, 1, 1),
    float3(0, 0, 1),
};

#endif

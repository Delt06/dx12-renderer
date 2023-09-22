#define THREAD_BLOCK_SIZE 32

#define DEBUG_CULLING 1
#define OCCLUSION_CULLING_AABB 1

#include "ShaderLibrary/Meshlets.hlsli"
#include "ShaderLibrary/Geometry.hlsli"

struct D3D12_DRAW_ARGUMENTS
{
    uint VertexCountPerInstance;
    uint InstanceCount;
    uint StartVertexLocation;
    uint StartInstanceLocation;
};

struct IndirectCommand
{
    uint meshletIndex;
    uint flags;
    D3D12_DRAW_ARGUMENTS drawArguments;
};

Texture2D<float> _HDB : register(t0);
AppendStructuredBuffer<IndirectCommand> _IndirectCommands : register(u0);

cbuffer CBuffer : register(b0)
{
    float3 _CameraPosition;
    uint _TotalCount;
    float4 _FrustumPlanes[6];
    float4x4 _ViewProjection;
    uint _HDB_Resolution_Width;
    uint _HDB_Resolution_Height;
}

struct MeshletInfo
{
    Meshlet meshlet;
    Transform transform;
    BoundingSphere boundingSphere;
    AABB aabb;
};

bool ConeCulling(const MeshletInfo meshletInfo)
{
    const Transform transform = meshletInfo.transform;
    const Meshlet meshlet = meshletInfo.meshlet;
    const float3 coneApex = mul(transform.worldMatrix, float4(meshlet.bounds.coneApex, 1.0f)).xyz;
    const float3 coneAxis = mul((float3x3) transform.inverseTransposeWorldMatrix, meshlet.bounds.coneAxis);

    const float dotResult = dot(normalize(coneApex - _CameraPosition), normalize(coneAxis));
    // using !>= handles the case when the meshlet's coneAxis is (0, 0, 0)
    // dotResult stores NaN in this case
    return !(dotResult >= meshlet.bounds.coneCutoff);
}

float DistanceToPlane(const float4 plane, const float3 position)
{
    return dot(float4(position, -1.0), plane);
}

bool FrustumCulling(const MeshletInfo meshletInfo)
{
    const float3 center = meshletInfo.boundingSphere.center;
    const float radius = meshletInfo.boundingSphere.radius;

    // https://gist.github.com/XProger/6d1fd465c823bba7138b638691831288
    const float dist01 = min(DistanceToPlane(_FrustumPlanes[0], center), DistanceToPlane(_FrustumPlanes[1], center));
    const float dist23 = min(DistanceToPlane(_FrustumPlanes[2], center), DistanceToPlane(_FrustumPlanes[3], center));
    const float dist45 = min(DistanceToPlane(_FrustumPlanes[4], center), DistanceToPlane(_FrustumPlanes[5], center));

    return min(min(dist01, dist23), dist45) + radius > 0;
}

bool OcclusionCulling(const BoundingSquareSS boundingSquare)
{
    const float2 boundsSizePixels = (boundingSquare.maxUV - boundingSquare.minUV) * float2(_HDB_Resolution_Width, _HDB_Resolution_Height);
    const float lod = ceil(log2(max(boundsSizePixels.x, boundsSizePixels.y) * 0.5f));

    float maxOccluderDepth = _HDB.SampleLevel(g_Common_LinearClampSampler, boundingSquare.minUV, lod);
    maxOccluderDepth = max(maxOccluderDepth, _HDB.SampleLevel(g_Common_LinearClampSampler, boundingSquare.maxUV, lod));
    maxOccluderDepth = max(maxOccluderDepth, _HDB.SampleLevel(g_Common_LinearClampSampler, float2(boundingSquare.minUV.x, boundingSquare.maxUV.y), lod));
    maxOccluderDepth = max(maxOccluderDepth, _HDB.SampleLevel(g_Common_LinearClampSampler, float2(boundingSquare.maxUV.x, boundingSquare.minUV.y), lod));

    return boundingSquare.minNdcDepth <= maxOccluderDepth;
}

bool OcclusionCullingBoundingSphere(const MeshletInfo meshletInfo)
{
    const BoundingSquareSS boundingSquare = ComputeScreenSpaceBoundingSquareFromSphere(meshletInfo.boundingSphere, _ViewProjection);
    return OcclusionCulling(boundingSquare);
}

bool OcclusionCullingAABB(const MeshletInfo meshletInfo)
{
    const BoundingSquareSS boundingSquare = ComputeScreenSpaceBoundingSquareFromAABB(meshletInfo.aabb, _ViewProjection);
    return OcclusionCulling(boundingSquare);
}

bool Culling(const MeshletInfo meshletInfo)
{
    return
    ConeCulling(meshletInfo) &&
    FrustumCulling(meshletInfo) &&
#if OCCLUSION_CULLING_AABB == 1
    OcclusionCullingAABB(meshletInfo)
#else
    OcclusionCullingBoundingSphere(meshletInfo)
#endif
    ;
}

[numthreads(THREAD_BLOCK_SIZE, 1, 1)]
void main(in uint3 dispatchId : SV_DispatchThreadID)
{
    const uint meshletIndex = dispatchId.x;
    if (meshletIndex >= _TotalCount)
    {
        return;
    }

    const Meshlet meshlet = _MeshletsBuffer[meshletIndex];
    const Transform transform = _TransformsBuffer[meshlet.transformIndex];
    const BoundingSphere boundingSphere = BoundingSphereObjectToWorldSpace(meshlet.bounds.center, meshlet.bounds.radius, transform.worldMatrix);
    const AABB aabb = AABBObjectToWorldSpace(meshlet.bounds.aabbCenter, meshlet.bounds.aabbHalfSize, transform.worldMatrix);
    const MeshletInfo meshletInfo = { meshlet, transform, boundingSphere, aabb };

#if DEBUG_CULLING == 0
    if (Culling(meshletInfo))
#endif
    {
        IndirectCommand indirectCommand;
        indirectCommand.meshletIndex = meshletIndex;
        indirectCommand.drawArguments.VertexCountPerInstance = meshlet.indexCount;
        indirectCommand.drawArguments.InstanceCount = 1;
        indirectCommand.drawArguments.StartVertexLocation = 0;
        indirectCommand.drawArguments.StartInstanceLocation = 0;
        indirectCommand.flags = 0;

#if DEBUG_CULLING == 1
        if (ConeCulling(meshletInfo))
        {
            indirectCommand.flags |= MESHLET_FLAGS_PASSED_CONE_CULLING;
        }

        if (FrustumCulling(meshletInfo))
        {
            indirectCommand.flags |= MESHLET_FLAGS_PASSED_FRUSTUM_CULLING;
        }

        if (OcclusionCullingAABB(meshletInfo))
        {
            indirectCommand.flags |= MESHLET_FLAGS_PASSED_OCCLUSION_CULLING;
        }
#endif

        _IndirectCommands.Append(indirectCommand);
    }
}

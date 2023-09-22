#define THREAD_BLOCK_SIZE 32

#include "ShaderLibrary/Meshlets.hlsli"

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

bool ConeCulling(const in Meshlet meshlet, const in Transform transform)
{
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

bool FrustumCulling(const float3 center, const float radius)
{
    // https://gist.github.com/XProger/6d1fd465c823bba7138b638691831288
    const float dist01 = min(DistanceToPlane(_FrustumPlanes[0], center), DistanceToPlane(_FrustumPlanes[1], center));
    const float dist23 = min(DistanceToPlane(_FrustumPlanes[2], center), DistanceToPlane(_FrustumPlanes[3], center));
    const float dist45 = min(DistanceToPlane(_FrustumPlanes[4], center), DistanceToPlane(_FrustumPlanes[5], center));

    return min(min(dist01, dist23), dist45) + radius > 0;
}

void ComputeScreenSpaceBoundingSquare(const float3 center, const float radius, out float outMinDepth, out float2 outMin, out float2 outMax)
{
    const int aabbVerticesCount = 8;
    const float3 aabbVertices[aabbVerticesCount] =
    {
        center + float3(-radius, -radius, -radius),
        center + float3(radius, -radius, -radius),
        center + float3(-radius, radius, -radius),
        center + float3(-radius, -radius, radius),
        center + float3(-radius, radius, radius),
        center + float3(radius, -radius, radius),
        center + float3(radius, radius, -radius),
        center + float3(radius, radius, radius),
    };

    float2 minNdc = 1;
    float2 maxNdc = -1;
    outMinDepth = 1;

    [unroll]
    for (uint i = 0; i < aabbVerticesCount; ++i)
    {
        const float4 positionCs = mul(_ViewProjection, float4(aabbVertices[i], 1.0f));
        const float3 ndc = positionCs.xyz / positionCs.w;
        minNdc = min(minNdc, ndc.xy);
        maxNdc = max(maxNdc, ndc.xy);
        outMinDepth = min(outMinDepth, ndc.z);
    }

    outMin = saturate(minNdc * 0.5 + 0.5);
    outMin.y = 1 - outMin.y;
    outMax = saturate(maxNdc * 0.5 + 0.5);
    outMax.y = 1 - outMax.y;
}

bool OcclusionCulling(const float3 center, const float radius)
{
    float minDepth;
    float2 boundsMin, boundsMax;
    ComputeScreenSpaceBoundingSquare(center, radius, minDepth, boundsMin, boundsMax);

    const float2 boundsSizePixels = (boundsMax - boundsMin) * float2(_HDB_Resolution_Width, _HDB_Resolution_Height);
    const float lod = ceil(log2(max(boundsSizePixels.x, boundsSizePixels.y) * 0.5f));

    float maxOccluderDepth = _HDB.SampleLevel(g_Common_LinearClampSampler, boundsMin, lod);
    maxOccluderDepth = max(maxOccluderDepth, _HDB.SampleLevel(g_Common_LinearClampSampler, boundsMax, lod));
    maxOccluderDepth = max(maxOccluderDepth, _HDB.SampleLevel(g_Common_LinearClampSampler, float2(boundsMin.x, boundsMax.y), lod));
    maxOccluderDepth = max(maxOccluderDepth, _HDB.SampleLevel(g_Common_LinearClampSampler, float2(boundsMax.x, boundsMin.y), lod));

    return minDepth <= maxOccluderDepth;
}

bool Culling(const in Meshlet meshlet, const in Transform transform, const float3 center, const float radius)
{
    return
    // ConeCulling(meshlet, transform) &&
    // FrustumCulling(center, radius) &&
    OcclusionCulling(center, radius);
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
    const float3 center = mul(transform.worldMatrix, float4(meshlet.bounds.center, 1.0f)).xyz;
    const float maxScale = max(transform.worldMatrix[0][0], max(transform.worldMatrix[1][1], transform.worldMatrix[2][2]));
    const float radius = meshlet.bounds.radius * maxScale;

    // if (Culling(meshlet, transform, center, radius))
    {
        IndirectCommand indirectCommand;
        indirectCommand.meshletIndex = meshletIndex;
        indirectCommand.drawArguments.VertexCountPerInstance = meshlet.indexCount;
        indirectCommand.drawArguments.InstanceCount = 1;
        indirectCommand.drawArguments.StartVertexLocation = 0;
        indirectCommand.drawArguments.StartInstanceLocation = 0;
        indirectCommand.flags = 0;

        if (ConeCulling(meshlet, transform))
        {
            indirectCommand.flags |= MESHLET_FLAGS_PASSED_CONE_CULLING;
        }

        if (FrustumCulling(center, radius))
        {
            indirectCommand.flags |= MESHLET_FLAGS_PASSED_FRUSTUM_CULLING;
        }

        if (OcclusionCulling(center, radius))
        {
            indirectCommand.flags |= MESHLET_FLAGS_PASSED_OCCLUSION_CULLING;
        }

        _IndirectCommands.Append(indirectCommand);
    }
}

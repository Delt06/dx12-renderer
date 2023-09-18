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

AppendStructuredBuffer<IndirectCommand> _IndirectCommands : register(u0);

cbuffer CBuffer : register(b0)
{
    float3 _CameraPosition;
    uint _TotalCount;
    float4 _FrustumPlanes[6];
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

bool FrustumCulling(const in Meshlet meshlet, const in Transform transform)
{
    const float3 center = mul(transform.worldMatrix, float4(meshlet.bounds.center, 1.0f)).xyz;
    const float maxScale = max(transform.worldMatrix[0][0], max(transform.worldMatrix[1][1], transform.worldMatrix[2][2]));
    const float radius = meshlet.bounds.radius * maxScale;

    // https://gist.github.com/XProger/6d1fd465c823bba7138b638691831288
    const float dist01 = min(DistanceToPlane(_FrustumPlanes[0], center), DistanceToPlane(_FrustumPlanes[1], center));
    const float dist23 = min(DistanceToPlane(_FrustumPlanes[2], center), DistanceToPlane(_FrustumPlanes[3], center));
    const float dist45 = min(DistanceToPlane(_FrustumPlanes[4], center), DistanceToPlane(_FrustumPlanes[5], center));

    return min(min(dist01, dist23), dist45) + radius > 0;
}

bool Culling(const in Meshlet meshlet, const in Transform transform)
{
    return
    ConeCulling(meshlet, transform) &&
    FrustumCulling(meshlet, transform);
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

    // if (Culling(meshlet, transform))
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

        if (FrustumCulling(meshlet, transform))
        {
            indirectCommand.flags |= MESHLET_FLAGS_PASSED_FRUSTUM_CULLING;
        }

        _IndirectCommands.Append(indirectCommand);
    }
}

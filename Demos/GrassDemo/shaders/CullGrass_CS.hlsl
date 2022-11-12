#include "GrassModel.hlsli"

#define THREAD_BLOCK_SIZE 128

struct IndirectCommand
{
    uint2 ModelCBV;
    uint2 MaterialCBV;
    uint4 DrawArguments;
    uint2 _Padding;
};

struct Plane
{
    float3 Normal;
    float Distance;
};

const static uint PLANES_IN_FRUSTUM = 6;

struct Frustum
{
    Plane Planes[PLANES_IN_FRUSTUM];
};

cbuffer Parameters : register(b0)
{
    Frustum _Frustum;
    float4 _BoundsExtents;
    uint _Count;
};

StructuredBuffer<GrassModel> _GrassModelCBs : register(t0);
StructuredBuffer<IndirectCommand> _InputCommands : register(t1);
AppendStructuredBuffer<IndirectCommand> _OutputCommands : register(u0);

float GetSignedDistanceToPlane(const in Plane plane, float3 position)
{
    return dot(plane.Normal, position) - plane.Distance;
}

bool IsOnPositiveSide(const in Plane plane, float3 aabbExtents, float3 aabbCenter)
{
    // Compute the projection interval radius of b onto L(t) = b.c + t * p.n
    const float3 absPlaneNormal = abs(plane.Normal);
    const float r = dot(aabbExtents, absPlaneNormal);

    return -r <= GetSignedDistanceToPlane(plane, aabbCenter);
}

bool IsInsideFrustum(const in Frustum frustum, float3 aabbExtents, float3 aabbCenter)
{
    bool inside = true;

    [unroll]
    for (uint i = 0; i < PLANES_IN_FRUSTUM; ++i)
    {
        inside = inside && IsOnPositiveSide(frustum.Planes[i], aabbExtents, aabbCenter);
    }

    return inside;
}

[numthreads(THREAD_BLOCK_SIZE, 1, 1)]
void main(in uint3 dispatchID : SV_DispatchThreadID)
{
    uint index = dispatchID.x;
    if (index >= _Count)
    {
        return;
    }

    float3 aabbCenter = _GrassModelCBs[index].Model._m03_m13_m23;
    float3 aabbExtents = _BoundsExtents.xyz;

    if (IsInsideFrustum(_Frustum, aabbExtents, aabbCenter))
    {
        _OutputCommands.Append(_InputCommands[index]);
    }
}

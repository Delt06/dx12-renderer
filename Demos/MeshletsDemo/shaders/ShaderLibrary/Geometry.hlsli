#ifndef GEOMETRY_HLSLI
#define GEOMETRY_HLSLI

struct BoundingSphere
{
    float3 center;
    float radius;
};

static const uint AabbVerticesCount = 8;

struct AABB
{
    float3 center;
    float3 halfSize;
    float3 vertices[AabbVerticesCount];
};

BoundingSphere BoundingSphereObjectToWorldSpace(const float3 center, const float radius, const float4x4 worldMatrix)
{
    const float3 centerWs = mul(worldMatrix, float4(center, 1.0f)).xyz;
    const float maxScale = max(worldMatrix[0][0], max(worldMatrix[1][1], worldMatrix[2][2]));
    const float radiusWs = radius * maxScale;

    BoundingSphere result;
    result.center = centerWs;
    result.radius = radiusWs;
    return result;
}

AABB AABBObjectToWorldSpace(const float3 center, const float3 halfSize, const float4x4 worldMatrix)
{
    AABB result;

    result.vertices[0] = center.xyz + float3(-halfSize.x, -halfSize.y, -halfSize.z);
    result.vertices[1] = center.xyz + float3(halfSize.x, -halfSize.y, -halfSize.z);
    result.vertices[2] = center.xyz + float3(-halfSize.x, halfSize.y, -halfSize.z);
    result.vertices[3] = center.xyz + float3(-halfSize.x, -halfSize.y, halfSize.z);
    result.vertices[4] = center.xyz + float3(-halfSize.x, halfSize.y, halfSize.z);
    result.vertices[5] = center.xyz + float3(halfSize.x, -halfSize.y, halfSize.z);
    result.vertices[6] = center.xyz + float3(halfSize.x, halfSize.y, -halfSize.z);
    result.vertices[7] = center.xyz + float3(halfSize.x, halfSize.y, halfSize.z);

    float3 minVertex = -100000000;
    float3 maxVertex = 100000000;

    [unroll]
    for (uint i = 0; i < AabbVerticesCount; ++i)
    {
        result.vertices[i] = mul(worldMatrix, float4(result.vertices[i], 1.0f)).xyz;
        minVertex = min(minVertex, result.vertices[i]);
        maxVertex = max(maxVertex, result.vertices[i]);
    }

    result.halfSize = (maxVertex - minVertex) * 0.5f;
    result.center = minVertex + result.halfSize;
    return result;
}

struct BoundingSquareSS
{
    float2 minUV;
    float2 maxUV;
    float minNdcDepth;
};

BoundingSquareSS ComputeScreenSpaceBoundingSquare(const float3 aabbVertices[AabbVerticesCount], const float4x4 viewProjectionMatrix)
{
    float2 minNdc = 1;
    float2 maxNdc = -1;
    BoundingSquareSS result;
    result.minNdcDepth = 1;

    [unroll]
    for (uint i = 0; i < AabbVerticesCount; ++i)
    {
        const float4 positionCs = mul(viewProjectionMatrix, float4(aabbVertices[i], 1.0f));
        const float3 ndc = positionCs.xyz / positionCs.w;
        minNdc = min(minNdc, ndc.xy);
        maxNdc = max(maxNdc, ndc.xy);
        result.minNdcDepth = min(result.minNdcDepth, ndc.z);
    }

    result.minUV = saturate(minNdc * 0.5 + 0.5);
    result.minUV.y = 1 - result.minUV.y;
    result.maxUV = saturate(maxNdc * 0.5 + 0.5);
    result.maxUV.y = 1 - result.maxUV.y;
    return result;
}

BoundingSquareSS ComputeScreenSpaceBoundingSquareFromAABB(const AABB aabb, const float4x4 viewProjectionMatrix)
{
    return ComputeScreenSpaceBoundingSquare(aabb.vertices, viewProjectionMatrix);
}

BoundingSquareSS ComputeScreenSpaceBoundingSquareFromSphere(const BoundingSphere boundingSphere, const float4x4 viewProjectionMatrix)
{
    const float3 center = boundingSphere.center;
    const float radius = boundingSphere.radius;
    const float3 aabbVertices[AabbVerticesCount] =
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
    return ComputeScreenSpaceBoundingSquare(aabbVertices, viewProjectionMatrix);
}

#endif

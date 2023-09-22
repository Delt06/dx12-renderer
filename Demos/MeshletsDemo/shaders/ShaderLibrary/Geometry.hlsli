#ifndef GEOMETRY_HLSLI
#define GEOMETRY_HLSLI

struct BoundingSphere
{
    float3 center;
    float radius;
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

struct BoundingSquareSS
{
    float2 minUV;
    float2 maxUV;
    float minNdcDepth;
};

BoundingSquareSS ComputeScreenSpaceBoundingSquare(const BoundingSphere boundingSphere, const float4x4 viewProjectionMatrix)
{
    const int aabbVerticesCount = 8;
    const float3 center = boundingSphere.center;
    const float radius = boundingSphere.radius;
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
    BoundingSquareSS result;
    result.minNdcDepth = 1;

    [unroll]
    for (uint i = 0; i < aabbVerticesCount; ++i)
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

#endif

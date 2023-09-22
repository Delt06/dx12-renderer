#include "../ShaderLibrary/Meshlets.hlsli"
#include "../ShaderLibrary/Geometry.hlsli"
#include "../ShaderLibrary/Pipeline.hlsli"

#define USE_AABB 1

float2 UvToNdc(const float2 uv)
{
    float2 ndc = uv;
    ndc.y = 1 - ndc.y;
    ndc = ndc * 2 - 1;
    return ndc;
}

float4 main(const CommonVertexAttributes IN) : SV_POSITION
{
    const Meshlet meshlet = _MeshletsBuffer[g_Pipeline_SelectedMeshletIndex];
    const MeshletBounds bounds = meshlet.bounds;
    const Transform transform = _TransformsBuffer[meshlet.transformIndex];

#if USE_AABB == 1
    const AABB aabb = AABBObjectToWorldSpace(bounds.aabbCenter, bounds.aabbHalfSize, transform.worldMatrix);
    const BoundingSquareSS boundingSquare = ComputeScreenSpaceBoundingSquareFromAABB(aabb, g_Pipeline_ViewProjection);
#else
    const BoundingSphere boundingSphere = BoundingSphereObjectToWorldSpace(bounds.center, bounds.radius, transform.worldMatrix);
    const BoundingSquareSS boundingSquare = ComputeScreenSpaceBoundingSquareFromSphere(boundingSphere, g_Pipeline_ViewProjection);
#endif

    const float2 minNdc = UvToNdc(boundingSquare.minUV);
    const float2 maxNdc = UvToNdc(boundingSquare.maxUV);
    const float2 halfSizeNdc = (maxNdc - minNdc) * 0.5f;
    const float2 centerNdc = (minNdc + maxNdc) * 0.5f;

    float4 positionCs;
    positionCs.xy = IN.position.xy * halfSizeNdc + centerNdc;
    positionCs.z = boundingSquare.minNdcDepth;
    positionCs.w = 1.0f;

    return positionCs;
}

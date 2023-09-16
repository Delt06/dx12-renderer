#include "MeshletBuilder.h"

#include <meshoptimizer.h>

using namespace DirectX;

namespace
{
    void CopyFloatArrayToDirectXFloat3(XMFLOAT3& destination, const float source[3])
    {
        destination.x = source[0];
        destination.y = source[1];
        destination.z = source[2];
    }
}

MeshletBuilder::MeshletSet MeshletBuilder::BuildMeshlets(const MeshPrototype& meshPrototype)
{
    constexpr size_t maxVertices = 64;
    constexpr size_t maxTriangles = 124;
    constexpr float coneWeight = 0.0f;

    const size_t maxMeshlets = meshopt_buildMeshletsBound(meshPrototype.m_Indices.size(), maxVertices, maxTriangles);
    std::vector<meshopt_Meshlet> meshoptMeshlets(maxMeshlets);
    MeshletSet meshletSet;
    meshletSet.m_Vertices.resize(maxMeshlets * maxVertices);
    meshletSet.m_Indices.resize(maxMeshlets * maxTriangles * 3);

    const size_t meshletCount = meshopt_buildMeshlets(meshoptMeshlets.data(),
        meshletSet.m_Vertices.data(), meshletSet.m_Indices.data(),
        meshPrototype.m_Indices.data(), meshPrototype.m_Indices.size(),
        &meshPrototype.m_Vertices[0].Position.x, meshPrototype.m_Vertices.size(), sizeof(VertexAttributes),
        maxVertices, maxTriangles, coneWeight
    );

    // trim
    const meshopt_Meshlet& last = meshoptMeshlets[meshletCount - 1];
    meshletSet.m_Meshlets.resize(meshletCount);
    meshletSet.m_Vertices.resize(last.vertex_offset + last.vertex_count);
    meshletSet.m_Indices.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));

    for (size_t i = 0; i < meshletSet.m_Meshlets.size(); ++i)
    {
        const meshopt_Meshlet& meshoptMeshlet = meshoptMeshlets[i];
        Meshlet meshlet;

        {
            const meshopt_Bounds meshoptBounds = meshopt_computeMeshletBounds(
                &meshletSet.m_Vertices[meshoptMeshlet.vertex_offset],
                &meshletSet.m_Indices[meshoptMeshlet.triangle_offset],
                meshoptMeshlet.triangle_count,
                &meshPrototype.m_Vertices[0].Position.x, meshPrototype.m_Vertices.size(), sizeof(VertexAttributes)
            );
            MeshletBounds& bounds = meshlet.m_Bounds;

            CopyFloatArrayToDirectXFloat3(bounds.m_Center, meshoptBounds.center);
            bounds.m_Radius = meshoptBounds.radius;

            CopyFloatArrayToDirectXFloat3(bounds.m_ConeApex, meshoptBounds.cone_apex);
            CopyFloatArrayToDirectXFloat3(bounds.m_ConeAxis, meshoptBounds.cone_axis);
            bounds.m_ConeCutoff = meshoptBounds.cone_cutoff;
        }

        {

            meshlet.m_VertexOffset = meshoptMeshlet.vertex_offset;
            meshlet.m_VertexCount = meshoptMeshlet.vertex_count;
            meshlet.m_TriangleOffset = meshoptMeshlet.triangle_offset;
            meshlet.m_TriangleCount = meshoptMeshlet.triangle_count;
        }

        meshletSet.m_Meshlets.push_back(meshlet);
    }

    return meshletSet;
}

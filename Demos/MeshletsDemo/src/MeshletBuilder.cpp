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

    void ComputeMeshletAabb(MeshletBounds& bounds, const meshopt_Meshlet& meshlet,
        const std::vector<unsigned int>& vertices,
        const std::vector<unsigned char>& indices,
        const std::vector<VertexAttributes>& vertexAttributes
    )
    {
        XMVECTOR min = XMVectorReplicate(FLT_MAX);
        XMVECTOR max = XMVectorReplicate(-FLT_MAX);

        const uint32_t indexCount = meshlet.triangle_count * 3;

        for (uint32_t i = 0; i < indexCount; ++i)
        {
            const auto index = indices[meshlet.triangle_offset + i];
            const auto& vertex = vertexAttributes[vertices[meshlet.vertex_offset + index]];

            const XMVECTOR position = XMLoadFloat4(&vertex.Position);
            min = XMVectorMin(position, min);
            max = XMVectorMax(position, max);
        }

        const auto halfSize = (max - min) * 0.5f;
        XMStoreFloat3(&bounds.m_AabbCenter, min + halfSize);
        XMStoreFloat3(&bounds.m_AabbHalfSize, halfSize);
    }

}

MeshletBuilder::MeshletSet MeshletBuilder::BuildMeshlets(const MeshPrototype& meshPrototype, uint32_t baseVertexOffset, uint32_t baseIndexOffset)
{
    constexpr size_t maxVertices = 64;
    constexpr size_t maxTriangles = 124;
    constexpr float coneWeight = 0.5f;

    const size_t maxMeshlets = meshopt_buildMeshletsBound(meshPrototype.m_Indices.size(), maxVertices, maxTriangles);
    std::vector<meshopt_Meshlet> meshoptMeshlets(maxMeshlets);
    std::vector<unsigned int> vertices(maxMeshlets * maxVertices);
    std::vector<unsigned char> indices(maxMeshlets * maxTriangles * 3);

    const size_t meshletCount = meshopt_buildMeshlets(meshoptMeshlets.data(),
        vertices.data(), indices.data(),
        meshPrototype.m_Indices.data(), meshPrototype.m_Indices.size(),
        &meshPrototype.m_Vertices[0].Position.x, meshPrototype.m_Vertices.size(), sizeof(VertexAttributes),
        maxVertices, maxTriangles, coneWeight
    );

    // trim
    const meshopt_Meshlet& last = meshoptMeshlets[meshletCount - 1];
    std::vector<Meshlet> meshlets(meshletCount);
    vertices.resize(last.vertex_offset + last.vertex_count);
    indices.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));

    for (size_t i = 0; i < meshlets.size(); ++i)
    {
        const meshopt_Meshlet& meshoptMeshlet = meshoptMeshlets[i];
        Meshlet meshlet;
        meshlet.m_TransformIndex = -1;

        {
            const meshopt_Bounds meshoptBounds = meshopt_computeMeshletBounds(
                &vertices[meshoptMeshlet.vertex_offset],
                &indices[meshoptMeshlet.triangle_offset],
                meshoptMeshlet.triangle_count,
                &meshPrototype.m_Vertices[0].Position.x, meshPrototype.m_Vertices.size(), sizeof(VertexAttributes)
            );
            MeshletBounds& bounds = meshlet.m_Bounds;

            CopyFloatArrayToDirectXFloat3(bounds.m_Center, meshoptBounds.center);
            bounds.m_Radius = meshoptBounds.radius;

            CopyFloatArrayToDirectXFloat3(bounds.m_ConeApex, meshoptBounds.cone_apex);
            CopyFloatArrayToDirectXFloat3(bounds.m_ConeAxis, meshoptBounds.cone_axis);
            bounds.m_ConeCutoff = meshoptBounds.cone_cutoff;

            ComputeMeshletAabb(bounds, meshoptMeshlet, vertices, indices, meshPrototype.m_Vertices);
        }

        {

            meshlet.m_VertexOffset = baseVertexOffset + meshoptMeshlet.vertex_offset;
            meshlet.m_VertexCount = meshoptMeshlet.vertex_count;
            meshlet.m_IndexOffset = baseIndexOffset + meshoptMeshlet.triangle_offset;
            meshlet.m_IndexCount = meshoptMeshlet.triangle_count * 3;
        }

        meshlets[i] = meshlet;
    }

    // new mesh prototype
    VertexCollectionType vertexBuffer(vertices.size());
    IndexCollectionType indexBuffer(indices.size());

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        vertexBuffer[i] = meshPrototype.m_Vertices[vertices[i]];
    }

    for (size_t i = 0; i < indices.size(); ++i)
    {
        indexBuffer[i] = indices[i];
    }

    return MeshletSet(
        MeshPrototype(std::move(vertexBuffer), std::move(indexBuffer)),
        std::move(meshlets)
    );
}

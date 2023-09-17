#pragma once

#include <vector>

#include <Framework/Mesh.h>

#include "Meshlet.h"

class MeshletBuilder
{
public:
    class MeshletSet
    {
    public:
        MeshPrototype m_MeshPrototype;
        std::vector<Meshlet> m_Meshlets;

        MeshletSet(MeshPrototype&& meshPrototype, std::vector<Meshlet>&& meshlets)
            : m_MeshPrototype(std::move(meshPrototype))
            , m_Meshlets(std::move(meshlets))
        { }

        MeshletSet() = default;

        void Add(const MeshletSet& other)
        {
            m_MeshPrototype.AddVertexAttributes(other.m_MeshPrototype);
            m_Meshlets.insert(m_Meshlets.end(), other.m_Meshlets.begin(), other.m_Meshlets.end());
        }
    };

    static MeshletSet BuildMeshlets(const MeshPrototype& meshPrototype, uint32_t baseVertexOffset = 0, uint32_t baseIndexOffset = 0);
};

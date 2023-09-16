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
    };

    static MeshletSet BuildMeshlets(const MeshPrototype& meshPrototype);
};

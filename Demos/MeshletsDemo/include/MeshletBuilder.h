#pragma once

#include <vector>

#include <Framework/Mesh.h>

#include "Meshlet.h"

class MeshletBuilder
{
public:
    struct MeshletSet
    {
        std::vector<Meshlet> m_Meshlets;
        std::vector<unsigned int> m_Vertices;
        std::vector<unsigned char> m_Indices;
    };

    static MeshletSet BuildMeshlets(const MeshPrototype& meshPrototype);
};

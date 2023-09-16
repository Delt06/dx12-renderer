#pragma once

#include <cstdint>

#include <DirectXMath.h>

struct MeshletBounds
{
    /* bounding sphere, useful for frustum and occlusion culling */
    DirectX::XMFLOAT3 m_Center;
    float m_Radius;

    /* normal cone, useful for backface culling */
    DirectX::XMFLOAT3 m_ConeApex;
    DirectX::XMFLOAT3 m_ConeAxis;
    float m_ConeCutoff; /* = cos(angle/2) */
};

struct Meshlet
{
    MeshletBounds m_Bounds;

    uint32_t m_VertexOffset;
    uint32_t m_TriangleOffset;

    uint32_t m_VertexCount;
    uint32_t m_TriangleCount;
};

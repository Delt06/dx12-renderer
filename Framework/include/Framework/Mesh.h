#pragma once

/*
 *  Copyright(c) 2018 Jeremiah van Oosten
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files(the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions :
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

/**
 *  @file Mesh.h
 *  @date October 24, 2018
 *  @author Jeremiah van Oosten
 *
 *  @brief A mesh class encapsulates the index and vertex buffers for a geometric primitive.
 */

#include <DX12Library/CommandList.h>
#include <DX12Library/VertexBuffer.h>
#include <DX12Library/IndexBuffer.h>

#include <DirectXMath.h>
#include <d3d12.h>

#include <wrl.h>

#include <memory> // For std::unique_ptr
#include <vector>
#include <map>
#include <string>

#include <Framework/Aabb.h>
#include <Framework/Armature.h>
#include <Framework/Bone.h>

struct Bone;

struct VertexAttributes
{
    VertexAttributes()
        : Position()
        , Normal()
        , Uv()
    { }

    VertexAttributes(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal,
        const DirectX::XMFLOAT2& textureCoordinate)
        : Position(position),
        Normal(normal),
        Uv(textureCoordinate)
    { }

    VertexAttributes(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal,
        const DirectX::XMFLOAT2& textureCoordinate, const DirectX::XMFLOAT3& tangent,
        const DirectX::XMFLOAT3& bitangent)
        : Position(position),
        Normal(normal),
        Uv(textureCoordinate),
        Tangent(tangent),
        Bitangent(bitangent)
    { }

    VertexAttributes(DirectX::FXMVECTOR position, DirectX::FXMVECTOR normal, DirectX::FXMVECTOR textureCoordinate)
    {
        XMStoreFloat3(&this->Position, position);
        XMStoreFloat3(&this->Normal, normal);
        XMStoreFloat2(&this->Uv, textureCoordinate);
    }

    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal{};
    DirectX::XMFLOAT2 Uv{};
    DirectX::XMFLOAT3 Tangent{};
    DirectX::XMFLOAT3 Bitangent{};

    static constexpr uint32_t VERTEX_BUFFER_SLOT_INDEX = 0;
    static constexpr int INPUT_ELEMENT_COUNT = 5;
    static const D3D12_INPUT_ELEMENT_DESC INPUT_ELEMENTS[INPUT_ELEMENT_COUNT];
};

struct SkinningVertexAttributes
{
    static constexpr uint32_t BONES_PER_VERTEX = 4;

    uint32_t BoneIds[BONES_PER_VERTEX];
    float Weights[BONES_PER_VERTEX];

    void NormalizeWeights();

    static constexpr uint32_t VERTEX_BUFFER_SLOT_INDEX = 1;
    static constexpr int INPUT_ELEMENT_COUNT = 2;
    static const D3D12_INPUT_ELEMENT_DESC INPUT_ELEMENTS[INPUT_ELEMENT_COUNT];
};

using VertexCollectionType = std::vector<VertexAttributes>;
using SkinningVertexCollectionType = std::vector<SkinningVertexAttributes>;
using IndexCollectionType = std::vector<uint16_t>;

struct MeshPrototype
{
    VertexCollectionType m_Vertices;
    IndexCollectionType m_Indices;

    SkinningVertexCollectionType m_SkinningVertexAttributes;
    Armature m_Armature;

    MeshPrototype(VertexCollectionType&& vertices, IndexCollectionType&& indices, bool rhCoords = false, bool generateTangents = false);
};

class Mesh final
{
public:
    void Draw(CommandList& commandList, uint32_t instanceCount = 1) const;
    void Bind(CommandList& commandList) const;

    UINT GetIndexCount() const;

    static std::shared_ptr<Mesh> CreateCube(CommandList& commandList, float size = 1, bool rhCoords = false);
    static std::shared_ptr<Mesh> CreateSphere(CommandList& commandList, float diameter = 1, size_t tessellation = 16,
        bool rhCoords = false);
    static std::shared_ptr<Mesh> CreateCone(CommandList& commandList, float diameter = 1, float height = 1,
        size_t tessellation = 32, bool rhCoords = false);
    static std::shared_ptr<Mesh> CreateTorus(CommandList& commandList, float diameter = 1, float thickness = 0.333f,
        size_t tessellation = 32, bool rhCoords = false);
    static std::shared_ptr<Mesh> CreatePlane(CommandList& commandList, float width = 1, float height = 1,
        bool rhCoords = false);
    static std::shared_ptr<Mesh> CreateVerticalQuad(CommandList& commandList, float width = 1, float height = 1,
        bool rhCoords = false);
    static std::shared_ptr<Mesh> CreateSpotlightPyramid(CommandList& commandList, float width = 1.0f, float depth = 1.0f, bool rhCoords = false);

    static std::shared_ptr<Mesh> CreateBlitTriangle(CommandList& commandList);

    static std::shared_ptr<Mesh> CreateMesh(CommandList& commandList, VertexCollectionType& vertices,
        IndexCollectionType& indices, bool rhCoords = false,
        bool generateTangents = false);
    static std::shared_ptr<Mesh> CreateMesh(CommandList& commandList, const MeshPrototype& prototype);

    Mesh(const Mesh& copy) = delete;
    virtual ~Mesh();
    Mesh();
    const Aabb& GetAabb() const;

    void SetSkinningVertexAttributes(CommandList& commandList, const SkinningVertexCollectionType& vertexAttributes);

    const Armature& GetArmature() const;
    Armature& GetArmature();

private:
    void Initialize(CommandList& commandList, VertexCollectionType& vertices, IndexCollectionType& indices,
        bool rhCoords);
    void Initialize(CommandList& commandList, const MeshPrototype& prototype);
    void CalculateAabb(const VertexCollectionType& vertices);

    VertexBuffer m_VertexBuffer;
    IndexBuffer m_IndexBuffer;
    VertexBuffer m_SkinningVertexBuffer;
    Armature m_Armature;

    Aabb m_Aabb{};
    UINT m_IndexCount;
};

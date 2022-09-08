﻿#include <DX12LibPCH.h>
#include "Mesh.h"

#include <Application.h>

#include "DirectXMesh/DirectXMesh.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace
{
	void GenerateTangents(VertexCollectionType& vertices, const IndexCollectionType& indices)
	{
		const size_t nVerts = vertices.size();
		const size_t nIndices = indices.size();
		const size_t nFaces = nIndices / 3;

		auto pos = std::make_unique<XMFLOAT3[]>(nVerts);
		auto normals = std::make_unique<XMFLOAT3[]>(nVerts);
		auto texcoords = std::make_unique<XMFLOAT2[]>(nVerts);
		for (size_t j = 0; j < nVerts; ++j)
		{
			const auto& vertexAttributes = vertices[j];
			pos[j] = vertexAttributes.Position;
			normals[j] = vertexAttributes.Normal;
			texcoords[j] = vertexAttributes.Uv;
		}
		const auto tangents = std::make_unique<XMFLOAT3[]>(nVerts);
		const auto bitangents = std::make_unique<XMFLOAT3[]>(nVerts);

		ThrowIfFailed(ComputeTangentFrame(indices.data(), nFaces,
		                                  pos.get(), normals.get(), texcoords.get(), nVerts,
		                                  tangents.get(), bitangents.get()));

		for (size_t j = 0; j < nVerts; ++j)
		{
			auto& vertexAttributes = vertices[j];
			vertexAttributes.Tangent = tangents[j];
			vertexAttributes.Bitangent = bitangents[j];
		}
	}
}

const D3D12_INPUT_ELEMENT_DESC VertexAttributes::INPUT_ELEMENTS[] = {
	{
		"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
};

Mesh::Mesh() : IndexCount(0)
{
}

const Aabb& Mesh::GetAabb() const
{
	return m_Aabb;
}

Mesh::~Mesh() = default;

void Mesh::Draw(CommandList& commandList, const uint32_t instanceCount) const
{
	commandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList.SetVertexBuffer(0, VertexBuffer);
	commandList.SetIndexBuffer(IndexBuffer);
	commandList.DrawIndexed(IndexCount, instanceCount);
}

std::shared_ptr<Mesh> Mesh::CreateSphere(CommandList& commandList, float diameter, size_t tessellation, bool rhcoords)
{
	VertexCollectionType vertices;
	IndexCollectionType indices;

	if (tessellation < 3)
		throw std::out_of_range("tessellation parameter out of range");

	float radius = diameter / 2.0f;
	size_t verticalSegments = tessellation;
	size_t horizontalSegments = tessellation * 2;

	// Create rings of vertices at progressively higher latitudes.
	for (size_t i = 0; i <= verticalSegments; i++)
	{
		float v = 1 - static_cast<float>(i) / verticalSegments;

		float latitude = (i * XM_PI / verticalSegments) - XM_PIDIV2;
		float dy, dxz;

		XMScalarSinCos(&dy, &dxz, latitude);

		// Create a single ring of vertices at this latitude.
		for (size_t j = 0; j <= horizontalSegments; j++)
		{
			float u = static_cast<float>(j) / horizontalSegments;

			float longitude = j * XM_2PI / horizontalSegments;
			float dx, dz;

			XMScalarSinCos(&dx, &dz, longitude);

			dx *= dxz;
			dz *= dxz;

			XMVECTOR normal = XMVectorSet(dx, dy, dz, 0);
			XMVECTOR textureCoordinate = XMVectorSet(u, v, 0, 0);
			XMVECTOR position = radius * normal;
			vertices.push_back(VertexAttributes(position, normal, textureCoordinate));
		}
	}

	// Fill the index buffer with triangles joining each pair of latitude rings.
	size_t stride = horizontalSegments + 1;

	for (size_t i = 0; i < verticalSegments; i++)
	{
		for (size_t j = 0; j <= horizontalSegments; j++)
		{
			size_t nextI = i + 1;
			size_t nextJ = (j + 1) % stride;

			indices.push_back(static_cast<uint16_t>(i * stride + j));
			indices.push_back(static_cast<uint16_t>(nextI * stride + j));
			indices.push_back(static_cast<uint16_t>(i * stride + nextJ));

			indices.push_back(static_cast<uint16_t>(i * stride + nextJ));
			indices.push_back(static_cast<uint16_t>(nextI * stride + j));
			indices.push_back(static_cast<uint16_t>(nextI * stride + nextJ));
		}
	}

	return CreateMesh(commandList, vertices, indices, rhcoords, true);
}

std::shared_ptr<Mesh> Mesh::CreateCube(CommandList& commandList, float size, bool rhcoords)
{
	// A cube has six faces, each one pointing in a different direction.
	const int FaceCount = 6;

	static const XMVECTORF32 faceNormals[FaceCount] =
	{
		{0, 0, 1},
		{0, 0, -1},
		{1, 0, 0},
		{-1, 0, 0},
		{0, 1, 0},
		{0, -1, 0},
	};

	static const XMVECTORF32 textureCoordinates[4] =
	{
		{1, 0},
		{1, 1},
		{0, 1},
		{0, 0},
	};

	VertexCollectionType vertices;
	IndexCollectionType indices;

	size /= 2;

	// Create each face in turn.
	for (int i = 0; i < FaceCount; i++)
	{
		XMVECTOR normal = faceNormals[i];

		// Get two vectors perpendicular both to the face normal and to each other.
		XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;

		XMVECTOR side1 = XMVector3Cross(normal, basis);
		XMVECTOR side2 = XMVector3Cross(normal, side1);

		// Six indices (two triangles) per face.
		size_t vbase = vertices.size();
		indices.push_back(static_cast<uint16_t>(vbase + 0));
		indices.push_back(static_cast<uint16_t>(vbase + 1));
		indices.push_back(static_cast<uint16_t>(vbase + 2));

		indices.push_back(static_cast<uint16_t>(vbase + 0));
		indices.push_back(static_cast<uint16_t>(vbase + 2));
		indices.push_back(static_cast<uint16_t>(vbase + 3));

		// Four vertices per face.
		vertices.push_back(VertexAttributes((normal - side1 - side2) * size, normal, textureCoordinates[0]));
		vertices.push_back(VertexAttributes((normal - side1 + side2) * size, normal, textureCoordinates[1]));
		vertices.push_back(VertexAttributes((normal + side1 + side2) * size, normal, textureCoordinates[2]));
		vertices.push_back(VertexAttributes((normal + side1 - side2) * size, normal, textureCoordinates[3]));
	}

	return CreateMesh(commandList, vertices, indices, rhcoords, true);
}

// Helper computes a point on a unit circle, aligned to the x/z plane and centered on the origin.
static inline XMVECTOR GetCircleVector(size_t i, size_t tessellation)
{
	float angle = i * XM_2PI / tessellation;
	float dx, dz;

	XMScalarSinCos(&dx, &dz, angle);

	XMVECTORF32 v = {dx, 0, dz, 0};
	return v;
}

static inline XMVECTOR GetCircleTangent(size_t i, size_t tessellation)
{
	float angle = (i * XM_2PI / tessellation) + XM_PIDIV2;
	float dx, dz;

	XMScalarSinCos(&dx, &dz, angle);

	XMVECTORF32 v = {dx, 0, dz, 0};
	return v;
}

// Helper creates a triangle fan to close the end of a cylinder / cone
static void CreateCylinderCap(VertexCollectionType& vertices, IndexCollectionType& indices, size_t tessellation,
                              float height, float radius, bool isTop)
{
	// Create cap indices.
	for (size_t i = 0; i < tessellation - 2; i++)
	{
		size_t i1 = (i + 1) % tessellation;
		size_t i2 = (i + 2) % tessellation;

		if (isTop)
		{
			std::swap(i1, i2);
		}

		size_t vbase = vertices.size();
		indices.push_back(static_cast<uint16_t>(vbase));
		indices.push_back(static_cast<uint16_t>(vbase + i1));
		indices.push_back(static_cast<uint16_t>(vbase + i2));
	}

	// Which end of the cylinder is this?
	XMVECTOR normal = g_XMIdentityR1;
	XMVECTOR textureScale = g_XMNegativeOneHalf;

	if (!isTop)
	{
		normal = -normal;
		textureScale *= g_XMNegateX;
	}

	// Create cap vertices.
	for (size_t i = 0; i < tessellation; i++)
	{
		XMVECTOR circleVector = GetCircleVector(i, tessellation);

		XMVECTOR position = (circleVector * radius) + (normal * height);

		XMVECTOR textureCoordinate = XMVectorMultiplyAdd(XMVectorSwizzle<0, 2, 3, 3>(circleVector), textureScale,
		                                                 g_XMOneHalf);

		vertices.push_back(VertexAttributes(position, normal, textureCoordinate));
	}
}

std::shared_ptr<Mesh> Mesh::CreateCone(CommandList& commandList, float diameter, float height, size_t tessellation,
                                       bool rhcoords)
{
	VertexCollectionType vertices;
	IndexCollectionType indices;

	if (tessellation < 3)
		throw std::out_of_range("tessellation parameter out of range");

	height /= 2;

	XMVECTOR topOffset = g_XMIdentityR1 * height;

	float radius = diameter / 2;
	size_t stride = tessellation + 1;

	// Create a ring of triangles around the outside of the cone.
	for (size_t i = 0; i <= tessellation; i++)
	{
		XMVECTOR circlevec = GetCircleVector(i, tessellation);

		XMVECTOR sideOffset = circlevec * radius;

		float u = static_cast<float>(i) / tessellation;

		XMVECTOR textureCoordinate = XMLoadFloat(&u);

		XMVECTOR pt = sideOffset - topOffset;

		XMVECTOR normal = XMVector3Cross(GetCircleTangent(i, tessellation), topOffset - pt);
		normal = XMVector3Normalize(normal);

		// Duplicate the top vertex for distinct normals
		vertices.push_back(VertexAttributes(topOffset, normal, g_XMZero));
		vertices.push_back(VertexAttributes(pt, normal, textureCoordinate + g_XMIdentityR1));

		indices.push_back(static_cast<uint16_t>(i * 2));
		indices.push_back(static_cast<uint16_t>((i * 2 + 3) % (stride * 2)));
		indices.push_back(static_cast<uint16_t>((i * 2 + 1) % (stride * 2)));
	}

	// Create flat triangle fan caps to seal the bottom.
	CreateCylinderCap(vertices, indices, tessellation, height, radius, false);

	return CreateMesh(commandList, vertices, indices, rhcoords, true);
}

std::shared_ptr<Mesh> Mesh::CreateTorus(CommandList& commandList, float diameter, float thickness, size_t tessellation,
                                        bool rhcoords)
{
	VertexCollectionType vertices;
	IndexCollectionType indices;

	if (tessellation < 3)
		throw std::out_of_range("tessellation parameter out of range");

	size_t stride = tessellation + 1;

	// First we loop around the main ring of the torus.
	for (size_t i = 0; i <= tessellation; i++)
	{
		float u = static_cast<float>(i) / tessellation;

		float outerAngle = i * XM_2PI / tessellation - XM_PIDIV2;

		// Create a transform matrix that will align geometry to
		// slice perpendicularly though the current ring position.
		XMMATRIX transform = XMMatrixTranslation(diameter / 2, 0, 0) * XMMatrixRotationY(outerAngle);

		// Now we loop along the other axis, around the side of the tube.
		for (size_t j = 0; j <= tessellation; j++)
		{
			float v = 1 - static_cast<float>(j) / tessellation;

			float innerAngle = j * XM_2PI / tessellation + XM_PI;
			float dx, dy;

			XMScalarSinCos(&dy, &dx, innerAngle);

			// Create a vertex.
			XMVECTOR normal = XMVectorSet(dx, dy, 0, 0);
			XMVECTOR position = normal * thickness / 2;
			XMVECTOR textureCoordinate = XMVectorSet(u, v, 0, 0);

			position = XMVector3Transform(position, transform);
			normal = XMVector3TransformNormal(normal, transform);

			vertices.push_back(VertexAttributes(position, normal, textureCoordinate));

			// And create indices for two triangles.
			size_t nextI = (i + 1) % stride;
			size_t nextJ = (j + 1) % stride;

			indices.push_back(static_cast<uint16_t>(i * stride + j));
			indices.push_back(static_cast<uint16_t>(i * stride + nextJ));
			indices.push_back(static_cast<uint16_t>(nextI * stride + j));

			indices.push_back(static_cast<uint16_t>(i * stride + nextJ));
			indices.push_back(static_cast<uint16_t>(nextI * stride + nextJ));
			indices.push_back(static_cast<uint16_t>(nextI * stride + j));
		}
	}

	return CreateMesh(commandList, vertices, indices, rhcoords, true);
}

std::shared_ptr<Mesh> Mesh::CreatePlane(CommandList& commandList, float width, float height, bool rhcoords)
{
	VertexCollectionType vertices =
	{
		{XMFLOAT3(-0.5f * width, 0.0f, 0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)}, // 0
		{XMFLOAT3(0.5f * width, 0.0f, 0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f)}, // 1
		{XMFLOAT3(0.5f * width, 0.0f, -0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f)}, // 2
		{XMFLOAT3(-0.5f * width, 0.0f, -0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f)} // 3
	};

	IndexCollectionType indices =
	{
		0, 3, 1, 1, 3, 2
	};

	return CreateMesh(commandList, vertices, indices, rhcoords, true);
}

std::shared_ptr<Mesh> Mesh::CreateVerticalQuad(CommandList& commandList, float width, float height, bool rhCoords)
{
	VertexCollectionType vertices =
	{
		{XMFLOAT3(width * -0.5f, 0.5f * height, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f)}, // 0
		{XMFLOAT3(0.5f * width, 0.5f * height, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f)}, // 1
		{XMFLOAT3(0.5f * width, -0.5f * height, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f)}, // 2
		{XMFLOAT3(-0.5f * width, -0.5f * height, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f)} // 3
	};

	IndexCollectionType indices =
	{
		0, 3, 1, 1, 3, 2
	};

	return CreateMesh(commandList, vertices, indices, rhCoords);
}

std::shared_ptr<Mesh> Mesh::CreateMesh(CommandList& commandList, VertexCollectionType& vertices,
                                       IndexCollectionType& indices, const bool rhCoords, const bool generateTangents)
{
	auto mesh = std::make_shared<Mesh>();
	if (generateTangents)
		GenerateTangents(vertices, indices);
	mesh->Initialize(commandList, vertices, indices, rhCoords);
	return mesh;
}


// Helper for flipping winding of geometric primitives for LH vs. RH coords
static void ReverseWinding(IndexCollectionType& indices, VertexCollectionType& vertices)
{
	assert((indices.size() % 3) == 0);
	for (auto it = indices.begin(); it != indices.end(); it += 3)
	{
		std::swap(*it, *(it + 2));
	}

	for (auto it = vertices.begin(); it != vertices.end(); ++it)
	{
		it->Uv.x = (1.f - it->Uv.x);
	}
}

void Mesh::Initialize(CommandList& commandList, VertexCollectionType& vertices, IndexCollectionType& indices,
                      bool rhCoords)
{
	if (vertices.size() >= USHRT_MAX)
		throw std::exception("Too many vertices for 16-bit index buffer");

	if (!rhCoords)
		ReverseWinding(indices, vertices);

	CalculateAabb(vertices);
	commandList.CopyVertexBuffer(VertexBuffer, vertices);
	commandList.CopyIndexBuffer(IndexBuffer, indices);

	IndexCount = static_cast<UINT>(indices.size());
}

void Mesh::CalculateAabb(const VertexCollectionType& vertices)
{
	m_Aabb = {};

	for (auto& vertex : vertices)
	{
		const XMVECTOR position = XMLoadFloat3(&vertex.Position);
		m_Aabb.Encapsulate(position);
	}
}

#include "Scene.h"

#include "Geometry/Aabb.h"

using namespace DirectX;

BoundingSphere Scene::ComputeBoundingSphere() const
{
	// a very native approach to find a bounding sphere (not ideal)
	Aabb sceneAabb{};

	for (auto& gameObject : m_GameObjects)
	{
		sceneAabb.Encapsulate(gameObject.GetAabb());
	}

	const XMVECTOR center = sceneAabb.GetCenter();
	float radius = 0.0f;

	XMVECTOR sceneAabbPoints[Aabb::POINTS_COUNT];
	sceneAabb.GetAllPoints(sceneAabbPoints);

	for (uint32_t i = 0; i < Aabb::POINTS_COUNT; ++i)
	{
		const XMVECTOR point = sceneAabbPoints[i];
		float distance = XMVectorGetX(XMVector3Length(point - center));
		radius = std::max(distance, radius);
	}

	return {radius, center};
}

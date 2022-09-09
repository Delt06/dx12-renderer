#include "Aabb.h"
#include <DirectXMath.h>

using namespace DirectX;

void Aabb::Encapsulate(const XMVECTOR point)
{
	m_Min = XMVectorMin(m_Min, point);
	XMVectorSetW(m_Min, 1.0f);
	m_Max = XMVectorMax(m_Max, point);
	XMVectorSetW(m_Max, 1.0f);
}

void Aabb::Encapsulate(const Aabb other)
{
	XMVECTOR otherPoints[POINTS_COUNT];
	other.GetAllPoints(otherPoints);

	for (const auto otherPoint : otherPoints)
	{
		Encapsulate(otherPoint);
	}
}

XMVECTOR Aabb::GetCenter() const
{
	return (m_Min + m_Max) * 0.5f;
}

Aabb Aabb::Transform(const XMMATRIX transform, const Aabb& aabb)
{
	XMVECTOR localPositions[POINTS_COUNT];
	aabb.GetAllPoints(localPositions);
	Aabb resultAabb{};

	for (const auto localPosition : localPositions)
	{
		const auto worldPosition = XMVector4Transform(localPosition, transform);
		resultAabb.Encapsulate(worldPosition);
	}

	return resultAabb;
}

void Aabb::GetAllPoints(XMVECTOR points[POINTS_COUNT]) const
{
	const float minX = XMVectorGetX(m_Min);
	const float minY = XMVectorGetX(m_Min);
	const float minZ = XMVectorGetX(m_Min);

	const float maxX = XMVectorGetX(m_Max);
	const float maxY = XMVectorGetX(m_Max);
	const float maxZ = XMVectorGetX(m_Max);

	constexpr float w = 1.0f;
	// bottom
	points[0] = m_Min;
	points[1] = XMVectorSet(maxX, minY, minZ, w);
	points[2] = XMVectorSet(maxX, minY, maxZ, w);
	points[3] = XMVectorSet(minX, minY, maxZ, w);
	// top
	points[4] = XMVectorSet(minX, maxY, minZ, w);
	points[5] = XMVectorSet(maxX, maxY, minZ, w);
	points[6] = m_Max;
	points[7] = XMVectorSet(minX, maxY, maxZ, w);
}

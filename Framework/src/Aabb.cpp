#include <Framework/Aabb.h>
#include <DirectXMath.h>

using namespace DirectX;

void Aabb::Encapsulate(DirectX::XMVECTOR point)
{
	Min = XMVectorMin(Min, point);
	XMVectorSetW(Min, 1.0f);
	Max = XMVectorMax(Max, point);
	XMVectorSetW(Max, 1.0f);
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
	return (Min + Max) * 0.5f;
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
	const float minX = XMVectorGetX(Min);
	const float minY = XMVectorGetX(Min);
	const float minZ = XMVectorGetX(Min);

	const float maxX = XMVectorGetX(Max);
	const float maxY = XMVectorGetX(Max);
	const float maxZ = XMVectorGetX(Max);

	constexpr float w = 1.0f;
	// bottom
	points[0] = Min;
	points[1] = XMVectorSet(maxX, minY, minZ, w);
	points[2] = XMVectorSet(maxX, minY, maxZ, w);
	points[3] = XMVectorSet(minX, minY, maxZ, w);
	// top
	points[4] = XMVectorSet(minX, maxY, minZ, w);
	points[5] = XMVectorSet(maxX, maxY, minZ, w);
	points[6] = Max;
	points[7] = XMVectorSet(minX, maxY, maxZ, w);
}

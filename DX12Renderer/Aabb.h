#pragma once
#include <DirectXMath.h>

struct Aabb
{
	DirectX::XMVECTOR m_Min;
	DirectX::XMVECTOR m_Max;

	void Encapsulate(DirectX::XMVECTOR point);
	void Encapsulate(Aabb other);

	static Aabb Transform(DirectX::XMMATRIX transform, const Aabb& aabb);

private:
	static constexpr uint32_t POINTS_COUNT = 8;
	void GetAllPoints(DirectX::XMVECTOR points[POINTS_COUNT]) const;
};

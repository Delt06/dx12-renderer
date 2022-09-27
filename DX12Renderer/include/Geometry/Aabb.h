#pragma once
#include <DirectXMath.h>

struct Aabb
{
	DirectX::XMVECTOR Min;
	DirectX::XMVECTOR Max;

	void Encapsulate(DirectX::XMVECTOR point);
	void Encapsulate(Aabb other);

	[[nodiscard]] DirectX::XMVECTOR GetCenter() const;

	static Aabb Transform(DirectX::XMMATRIX transform, const Aabb& aabb);

	static constexpr uint32_t POINTS_COUNT = 8;
	void GetAllPoints(DirectX::XMVECTOR points[POINTS_COUNT]) const;
};

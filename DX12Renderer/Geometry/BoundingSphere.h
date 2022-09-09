#pragma once

#include <DirectXMath.h>

struct BoundingSphere
{
	BoundingSphere(float radius, DirectX::XMVECTOR center);

	[[nodiscard]] float GetRadius() const;
	[[nodiscard]] DirectX::XMVECTOR GetCenter() const;

private:
	float m_Radius;
	DirectX::XMVECTOR m_Center;
};

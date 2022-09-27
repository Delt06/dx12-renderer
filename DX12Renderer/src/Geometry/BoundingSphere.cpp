#include <Geometry/BoundingSphere.h>

BoundingSphere::BoundingSphere(const float radius, const DirectX::XMVECTOR center)
	: m_Radius(radius)
	  , m_Center(center)
{
}

float BoundingSphere::GetRadius() const
{
	return m_Radius;
}

DirectX::XMVECTOR BoundingSphere::GetCenter() const
{
	return m_Center;
}

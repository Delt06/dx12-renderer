#pragma once
#include <DirectXMath.h>
#include <memory>

class Mesh;

class GameObject
{
public:
	GameObject(DirectX::XMMATRIX worldMatrix, std::unique_ptr<Mesh>& mesh);

	DirectX::XMMATRIX m_WorldMatrix;
	std::unique_ptr<Mesh> m_Mesh{};
};

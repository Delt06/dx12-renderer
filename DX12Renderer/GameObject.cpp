#include "GameObject.h"
#include "Mesh.h"

GameObject::GameObject(const DirectX::XMMATRIX worldMatrix, std::unique_ptr<Mesh>& mesh) :
	m_WorldMatrix(worldMatrix),
	m_Mesh(std::move(mesh))
{
}

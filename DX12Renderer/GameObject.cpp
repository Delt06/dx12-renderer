#include "GameObject.h"
#include "Mesh.h"
#include "Model.h"

GameObject::GameObject(const DirectX::XMMATRIX worldMatrix, const std::shared_ptr<Model> model)
	: m_WorldMatrix(worldMatrix)
	  , m_Aabb{}
	  , m_Model(model)
{
	RecalculateAabb();
}

void GameObject::Draw(
	const std::function<void(CommandList& commandList, DirectX::XMMATRIX worldMatrix)>& setMatricesFunc,
	CommandList& commandList, const uint32_t materialRootParameterIndex, const uint32_t mapsRootParameterIndex) const
{
	setMatricesFunc(commandList, m_WorldMatrix);
	m_Model->Draw(commandList, materialRootParameterIndex, mapsRootParameterIndex);
}

const DirectX::XMMATRIX& GameObject::GetWorldMatrix() const
{
	return m_WorldMatrix;
}

std::shared_ptr<const Model> GameObject::GetModel() const
{
	return m_Model;
}

void GameObject::RecalculateAabb()
{
	m_Aabb = {};

	for (auto& mesh : m_Model->GetMeshes())
	{
		const auto meshAabb = Aabb::Transform(m_WorldMatrix, mesh->GetAabb());
		m_Aabb.Encapsulate(meshAabb);
	}
}

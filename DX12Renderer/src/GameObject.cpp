#include "GameObject.h"
#include <Framework/Mesh.h>
#include <Framework/Model.h>
#include "MaterialBase.h"

GameObject::GameObject(const DirectX::XMMATRIX worldMatrix, const std::shared_ptr<Model> model, std::shared_ptr<MaterialBase> material)
	: m_WorldMatrix(worldMatrix)
	, m_PreviousWorldMatrix(worldMatrix)
	, m_Aabb{}
	, m_Model(model)
	, m_Material(material)
{
	RecalculateAabb();
}

void GameObject::Draw(
	const std::function<void(CommandList& commandList, DirectX::XMMATRIX worldMatrix)>& setMatricesFunc,
	CommandList& commandList, const uint32_t materialRootParameterIndex, const uint32_t mapsRootParameterIndex) const
{
	setMatricesFunc(commandList, m_WorldMatrix);
	m_Model->Draw(commandList);
}

const DirectX::XMMATRIX& GameObject::GetWorldMatrix() const
{
	return m_WorldMatrix;
}

std::shared_ptr<const Model> GameObject::GetModel() const
{
	return m_Model;
}

const Aabb& GameObject::GetAabb() const
{
	return m_Aabb;
}

void GameObject::OnRenderedFrame()
{
	m_PreviousWorldMatrix = m_WorldMatrix;
}

const DirectX::XMMATRIX& GameObject::GetPreviousWorldMatrix() const
{
	return m_PreviousWorldMatrix;
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

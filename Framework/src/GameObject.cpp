#include "GameObject.h"
#include <Framework/Mesh.h>
#include <Framework/Model.h>
#include <Framework/Material.h>

GameObject::GameObject(const DirectX::XMMATRIX& worldMatrix, const std::shared_ptr<Model>& pModel, const std::shared_ptr<Material>& pMaterial)
    : m_WorldMatrix(worldMatrix)
    , m_PreviousWorldMatrix(worldMatrix)
    , m_Aabb{}
    , m_Model(pModel)
    , m_Material(pMaterial)
{
    RecalculateAabb();
}

void GameObject::Draw(CommandList& commandList) const
{
    m_Material->Bind(commandList);
    m_Model->Draw(commandList);
    m_Material->Unbind(commandList);
}

const DirectX::XMMATRIX& GameObject::GetWorldMatrix() const
{
    return m_WorldMatrix;
}

DirectX::XMMATRIX& GameObject::GetWorldMatrix()
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

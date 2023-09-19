#pragma once

#include <DirectXMath.h>
#include <functional>
#include <memory>

#include <Framework/Aabb.h>

class Model;
class CommandList;
class Material;

class GameObject
{
public:
    GameObject(const DirectX::XMMATRIX& worldMatrix, const std::shared_ptr<Model>& pModel, const std::shared_ptr<Material>& pMaterial);

    void Draw(CommandList& commandList) const;

    [[nodiscard]] const DirectX::XMMATRIX& GetWorldMatrix() const;
    [[nodiscard]] DirectX::XMMATRIX& GetWorldMatrix();
    [[nodiscard]] std::shared_ptr<const Model> GetModel() const;
    [[nodiscard]] const Aabb& GetAabb() const;

    const std::shared_ptr<Material>& GetMaterial() const
    {
        return m_Material;
    }

    void OnRenderedFrame();

    [[nodiscard]] const DirectX::XMMATRIX& GetPreviousWorldMatrix() const;

private:
    void RecalculateAabb();

    DirectX::XMMATRIX m_WorldMatrix;
    DirectX::XMMATRIX m_PreviousWorldMatrix;
    Aabb m_Aabb;
    std::shared_ptr<Model> m_Model{};
    std::shared_ptr<Material> m_Material;
};

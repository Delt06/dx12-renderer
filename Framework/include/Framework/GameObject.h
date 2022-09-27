#pragma once

#include <DirectXMath.h>
#include <functional>
#include <memory>

#include <Framework/Aabb.h>

class Model;
class CommandList;
class MaterialBase;

class GameObject
{
public:
	GameObject(DirectX::XMMATRIX worldMatrix, std::shared_ptr<Model> model, std::shared_ptr<MaterialBase> material);

	void Draw(const std::function<void(CommandList& commandList, DirectX::XMMATRIX worldMatrix)>& setMatricesFunc,
	          CommandList& commandList,
	          uint32_t materialRootParameterIndex, uint32_t mapsRootParameterIndex) const;

	[[nodiscard]] const DirectX::XMMATRIX& GetWorldMatrix() const;
	[[nodiscard]] std::shared_ptr<const Model> GetModel() const;
	[[nodiscard]] const Aabb& GetAabb() const;

	template<class TMaterial>
	std::shared_ptr<TMaterial> GetMaterial() const
	{
		static_assert(std::is_base_of<MaterialBase, TMaterial>::value, "type parameter of this class must derive from MaterialBase");
		return std::static_pointer_cast<TMaterial>(m_Material);
	}

	void OnRenderedFrame();

	[[nodiscard]] const DirectX::XMMATRIX& GetPreviousWorldMatrix() const;
private:
	void RecalculateAabb();

	DirectX::XMMATRIX m_WorldMatrix;
	DirectX::XMMATRIX m_PreviousWorldMatrix;
	Aabb m_Aabb;
	std::shared_ptr<Model> m_Model{};
	std::shared_ptr<MaterialBase> m_Material;
};

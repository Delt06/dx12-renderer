#pragma once

#include <DirectXMath.h>
#include <functional>
#include <memory>

#include "Geometry/Aabb.h"

class Model;
class CommandList;

class GameObject
{
public:
	GameObject(DirectX::XMMATRIX worldMatrix, std::shared_ptr<Model> model);

	void Draw(const std::function<void(CommandList& commandList, DirectX::XMMATRIX worldMatrix)>& setMatricesFunc,
	          CommandList& commandList,
	          uint32_t materialRootParameterIndex, uint32_t mapsRootParameterIndex) const;

	[[nodiscard]] const DirectX::XMMATRIX& GetWorldMatrix() const;
	[[nodiscard]] std::shared_ptr<const Model> GetModel() const;
	[[nodiscard]] const Aabb& GetAabb() const;
private:
	void RecalculateAabb();

	DirectX::XMMATRIX m_WorldMatrix;
	Aabb m_Aabb;
	std::shared_ptr<Model> m_Model{};
};

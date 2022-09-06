#pragma once

#include <DirectXMath.h>
#include <functional>
#include <memory>

class Model;
class CommandList;

class GameObject
{
public:
	GameObject(DirectX::XMMATRIX worldMatrix, std::shared_ptr<Model> model);

	void Draw(const std::function<void(CommandList& commandList, DirectX::XMMATRIX worldMatrix)>& setMatricesFunc, uint32_t mapsRootParameterIndex, CommandList& commandList) const;

private:
	DirectX::XMMATRIX m_WorldMatrix;
	std::shared_ptr<Model> m_Model{};
};

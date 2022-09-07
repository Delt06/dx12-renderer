#include "GameObject.h"
#include "Mesh.h"
#include "Model.h"

GameObject::GameObject(const DirectX::XMMATRIX worldMatrix, const std::shared_ptr<Model> model)
	: m_WorldMatrix(worldMatrix)
	  , m_Model(model)
{
}

void GameObject::Draw(
	const std::function<void(CommandList& commandList, DirectX::XMMATRIX worldMatrix)>& setMatricesFunc,
	CommandList& commandList, const uint32_t materialRootParameterIndex, const uint32_t mapsRootParameterIndex) const
{
	setMatricesFunc(commandList, m_WorldMatrix);
	m_Model->Draw(commandList, materialRootParameterIndex, mapsRootParameterIndex);
}

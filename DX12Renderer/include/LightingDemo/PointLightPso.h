#pragma once
#include <DirectXMath.h>
#include <wrl.h>

#include <DX12Library/ResourceStateTracker.h>
#include <DX12Library/RootSignature.h>
#include <Framework/Light.h>

class CommandList;
class Mesh;

class PointLightPso final
{
public:
	explicit PointLightPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList);

	void Set(CommandList& commandList) const;
	void Draw(CommandList& commandList, const PointLight& pointLight, DirectX::XMMATRIX viewMatrix,
	          DirectX::XMMATRIX viewProjectionMatrix, DirectX::XMMATRIX projectionMatrix, float scale = 0.5f) const;

private:
	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	std::shared_ptr<Mesh> m_Mesh;
};

#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>

#include "RootSignature.h"

class Texture;
class CommandList;
class Mesh;

class ParticleSystemPso
{
public:
	ParticleSystemPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList);

	void Set(CommandList& commandList) const;
	void Draw(CommandList& commandList, DirectX::XMMATRIX worldMatrix, DirectX::XMMATRIX viewMatrix,
	          DirectX::XMMATRIX viewProjectionMatrix, DirectX::XMMATRIX projectionMatrix) const;

private:
	std::shared_ptr<Mesh> m_QuadMesh;
	std::shared_ptr<Texture> m_Texture;

	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
};

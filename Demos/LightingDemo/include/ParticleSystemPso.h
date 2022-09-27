#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>

#include <DX12Library/RootSignature.h>
#include <DX12Library/VertexBuffer.h>

class Texture;
class CommandList;
class Mesh;

struct ParticleInstanceData
{
	DirectX::XMFLOAT3 m_Pivot;
	DirectX::XMFLOAT4 m_Color;
	float m_Scale;
};

class ParticleSystemPso
{
public:
	ParticleSystemPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList);

	void SetContext(CommandList& commandList) const;
	void UploadInstanceData(CommandList& commandList, const ParticleInstanceData* instanceData,
	                        uint32_t instancesCount);
	void Draw(CommandList& commandList, DirectX::XMMATRIX viewMatrix,
	          DirectX::XMMATRIX viewProjectionMatrix, DirectX::XMMATRIX projectionMatrix) const;

private:
	std::shared_ptr<Mesh> m_QuadMesh;
	std::shared_ptr<Texture> m_Texture;

	uint32_t m_InstancesCount{};
	VertexBuffer m_InstanceDataVertexBuffer;

	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
};

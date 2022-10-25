#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>

#include <DX12Library/RootSignature.h>
#include <DX12Library/VertexBuffer.h>
#include <Framework/Material.h>

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
	ParticleSystemPso(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void Begin(CommandList& commandList) const;
	void End(CommandList& commandList) const;
	void UploadInstanceData(CommandList& commandList, const ParticleInstanceData* instanceData,
		uint32_t instancesCount);
	void Draw(CommandList& commandList) const;

private:
	std::shared_ptr<Mesh> m_QuadMesh;
	std::shared_ptr<Texture> m_Texture;
    std::shared_ptr<Material> m_Material;

	uint32_t m_InstancesCount{};
	VertexBuffer m_InstanceDataVertexBuffer = VertexBuffer(L"Particle System Instance Data");
};

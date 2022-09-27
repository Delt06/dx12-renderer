#pragma once
#include <memory>
#include <vector>
#include <wrl.h>
#include <DirectXMath.h>

#include "ParticleSystemPso.h"

class ParticleSystem
{
public:
	ParticleSystem(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DirectX::XMVECTOR origin);

	void Update(double deltaTime);
	void Draw(CommandList& commandList, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX viewProjectionMatrix,
	          DirectX::XMMATRIX
	          projectionMatrix) const;

private:
	void Emit(double deltaTime);
	void EmitOne();
	void Simulate(float deltaTime);
	void CollectDeadParticles();

	DirectX::XMVECTOR m_Origin;
	float m_EmissionRate = 5000.0f;
	float m_EmissionRadius = 0.15f;
	float m_ScaleOverTime = 8.0f;
	DirectX::XMFLOAT2 m_VelocityConeAngleRange = DirectX::XMFLOAT2(15.0f, 40.0f);
	DirectX::XMVECTOR m_Gravity = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	size_t m_Capacity = 20000;

	double m_EmissionTimer = 0.0f;
	mutable bool m_GpuBufferIsDirty = true;

	static constexpr size_t ALIGNMENT = 16;

	struct alignas(ALIGNMENT) ParticleSimulationData
	{
		DirectX::XMVECTOR m_Position;
		DirectX::XMVECTOR m_Velocity;
		DirectX::XMFLOAT4 m_Color;
		float m_Time;
		float m_StartScale;
		float m_Lifetime;
		bool m_IsValid;
	};

	std::unique_ptr<ParticleSystemPso> m_Pso;

	uint32_t m_LiveInstancesCount;
	std::unique_ptr<ParticleSimulationData[]> m_SimulationData;
	std::unique_ptr<ParticleInstanceData[]> m_InstanceData;
	std::vector<uint32_t> m_DeadInstancesIndices{};
};

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
	void Draw(CommandList& commandList, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX viewProjectionMatrix, DirectX::XMMATRIX
	            projectionMatrix) const;

private:
	DirectX::XMVECTOR m_Origin;
	float m_EmissionRate = 2500.0f;
	float m_EmissionRadius = 0.15f;
	float m_ScaleOverTime = 8.0f;
	DirectX::XMFLOAT2 m_VelocityConeAngleRange = DirectX::XMFLOAT2(15.0f, 40.0f);
	DirectX::XMVECTOR m_Gravity = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	double m_EmissionTimer = 0.0f;

	static constexpr size_t ALIGNMENT = 16;

	struct alignas(ALIGNMENT) ParticleSimulationData
	{
		DirectX::XMVECTOR m_Position;
		DirectX::XMVECTOR m_Velocity;
		DirectX::XMFLOAT4 m_Color;
		float m_Time;
		float m_StartScale;
		float m_Lifetime;
	};
	
	std::unique_ptr<ParticleSystemPso> m_ParticleSystemPso;
	std::vector<ParticleSimulationData> m_ParticleSimulationData{};
	std::vector<ParticleInstanceData> m_ParticleInstanceData{};
};

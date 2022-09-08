#include "ParticleSystem.h"

#include <cmath>
#include <DirectXMath.h>

#include "Helpers.h"

using namespace DirectX;

namespace
{
	float RandomRange(const float from, const float to)
	{
		const float r = from + static_cast<float>(rand()) / (RAND_MAX / (to - from));
		return r;
	}

	XMVECTOR RangeInsideUnitSphere()
	{
		const float x = RandomRange(-1.0f, 1.0f);
		const float y = RandomRange(-1.0f, 1.0f);
		const float z = RandomRange(-1.0f, 1.0f);
		auto offset = XMVectorSet(x, y, z, 0.0f);
		offset = XMVector4ClampLength(offset, 0.0f, 1.0f);
		return offset;
	}

	XMFLOAT4 LerpColor(const XMFLOAT4 color1, const XMFLOAT4 color2, const float t)
	{
		const XMVECTOR resultV = XMVectorLerp(XMLoadFloat4(&color1), XMLoadFloat4(&color2), t);
		XMFLOAT4 result;
		XMStoreFloat4(&result, resultV);
		return result;
	}

	XMVECTOR RandomDirectionInCone(const float coneAngleDegrees)
	{
		// imagine a cone with an apex at the origin, an axis pointing upwards, and an between the height and slat = alpha
		// the cap of the cone has a radius of one
		// thus, the height is exactly cot(alpha)
		float sin, cos;
		XMScalarSinCos(&sin, &cos, XMConvertToRadians(coneAngleDegrees));
		const float height = cos / sin; // cotangent

		const float randomAngleRad = RandomRange(0.0f, 2.0f * Math::PI);
		float x, z;
		XMScalarSinCos(&z, &x, randomAngleRad);

		XMVECTOR direction = XMVectorSet(x, height, z, 0.0f);
		direction = XMVector4Normalize(direction);
		return direction;
	}
}

ParticleSystem::ParticleSystem(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList,
                               const XMVECTOR origin) :
	m_Origin(origin)
{
	m_ParticleSystemPso = std::make_unique<ParticleSystemPso>(device, commandList);
}

void ParticleSystem::Update(const double deltaTime)
{
	// TODO: add destruction
	m_EmissionTimer += deltaTime;

	const double emissionPeriod = 1 / m_EmissionRate;

	while (m_EmissionTimer >= emissionPeriod)
	{
		const auto offset = RangeInsideUnitSphere() * m_EmissionRadius;

		const auto velocity = RandomDirectionInCone(RandomRange(m_VelocityConeAngleRange.x, m_VelocityConeAngleRange.y))
			*
			RandomRange(0.5f, 1.0f);
		const auto color = LerpColor(XMFLOAT4(0.9f, 0.9f, 0.9f, 0.5f), XMFLOAT4(0.6f, 0.6f, 0.6f, 0.25f),
		                             RandomRange(0, 1));
		m_ParticleSimulationData.push_back(ParticleSimulationData{
			m_Origin + offset,
			velocity,
			color,
			0.0f,
			RandomRange(0.05f, 0.1f),
			RandomRange(2.0f, 3.0f),
		});
		m_ParticleInstanceData.push_back(ParticleInstanceData{});
		m_EmissionTimer -= emissionPeriod;
	}

	const auto fDeltaTime = static_cast<float>(deltaTime);

	for (size_t i = 0; i < m_ParticleSimulationData.size(); ++i)
	{
		auto& simulationData = m_ParticleSimulationData[i];
		simulationData.m_Time += fDeltaTime;
		simulationData.m_Position += simulationData.m_Velocity * fDeltaTime;
		simulationData.m_Velocity += m_Gravity * fDeltaTime;

		const float normalizedTime = min(1.0f, simulationData.m_Time / simulationData.m_Lifetime);

		auto& instanceData = m_ParticleInstanceData[i];
		XMStoreFloat3(&instanceData.m_Pivot, simulationData.m_Position);
		instanceData.m_Color = simulationData.m_Color;
		instanceData.m_Color.w *= max(1.0f - normalizedTime, 0.0f);
		instanceData.m_Scale = simulationData.m_StartScale * std::lerp(1.0f, m_ScaleOverTime, normalizedTime);
	}
}

void ParticleSystem::Draw(CommandList& commandList, const XMMATRIX viewMatrix, const XMMATRIX viewProjectionMatrix,
                          const XMMATRIX
                          projectionMatrix) const
{
	if (m_ParticleInstanceData.empty()) return;

	m_ParticleSystemPso->SetContext(commandList);
	m_ParticleSystemPso->UploadInstanceData(commandList, m_ParticleInstanceData);
	m_ParticleSystemPso->Draw(commandList, viewMatrix, viewProjectionMatrix, projectionMatrix);
}

#include <ParticleSystem.h>

#include <cmath>
#include <DirectXMath.h>

#include <DX12Library/Helpers.h>

using namespace DirectX;

namespace
{
    float RandomRange(const float from, const float to)
    {
        const float r = from + static_cast<float>(rand()) / (RAND_MAX / (to - from));
        return r;
    }

    size_t RandomRange(const size_t from, const size_t toExc)
    {
        return rand() % (toExc - from) + from;
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

ParticleSystem::ParticleSystem(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, DirectX::XMVECTOR origin) :
    m_Origin(origin)
{
    m_Pso = std::make_unique<ParticleSystemPso>(rootSignature, commandList);

    m_SimulationData = std::make_unique<ParticleSimulationData[]>(m_Capacity);
    memset(m_SimulationData.get(), 0, m_Capacity * sizeof(ParticleSimulationData));
    m_InstanceData = std::make_unique<ParticleInstanceData[]>(m_Capacity);
    m_LiveInstancesCount = 0;
}

void ParticleSystem::Update(const double deltaTime)
{
    Emit(deltaTime);
    Simulate(static_cast<float>(deltaTime));
    CollectDeadParticles();

    m_GpuBufferIsDirty = true;
}

void ParticleSystem::Draw(CommandList& commandList) const
{
    if (m_LiveInstancesCount == 0) return;

    m_Pso->Begin(commandList);

    if (m_GpuBufferIsDirty)
    {
        m_Pso->UploadInstanceData(commandList, m_InstanceData.get(), m_LiveInstancesCount);
        m_GpuBufferIsDirty = false;
    }

    m_Pso->Draw(commandList);

    m_Pso->End(commandList);
}

void ParticleSystem::Emit(const double deltaTime)
{
    m_EmissionTimer += deltaTime;

    const double emissionPeriod = 1 / m_EmissionRate;

    while (m_EmissionTimer >= emissionPeriod)
    {
        EmitOne();
        m_EmissionTimer -= emissionPeriod;
    }
}

void ParticleSystem::EmitOne()
{
    size_t index;
    if (m_LiveInstancesCount == m_Capacity)
    {
        index = RandomRange(0, m_Capacity);
    }
    else
    {
        index = m_LiveInstancesCount;
        m_LiveInstancesCount++;
    }

    const auto offset = RangeInsideUnitSphere() * m_EmissionRadius;

    const auto velocity = RandomDirectionInCone(RandomRange(m_VelocityConeAngleRange.x, m_VelocityConeAngleRange.y))
        *
        RandomRange(0.5f, 1.0f);
    const auto color = LerpColor(XMFLOAT4(0.9f, 0.9f, 0.9f, 0.35f), XMFLOAT4(0.6f, 0.6f, 0.6f, 0.1f),
        RandomRange(0.0f, 1.0f));
    m_SimulationData[index] = {
        m_Origin + offset,
        velocity,
        color,
        0.0f,
        RandomRange(0.04f, 0.07f),
        RandomRange(2.0f, 3.0f),
        true
    };
}

void ParticleSystem::Simulate(const float deltaTime)
{
    for (uint32_t i = 0; i < m_LiveInstancesCount; ++i)
    {
        auto& simulationData = m_SimulationData[i];
        simulationData.m_Time += deltaTime;
        simulationData.m_Position += simulationData.m_Velocity * deltaTime;
        simulationData.m_Velocity += m_Gravity * deltaTime;

        const float normalizedTime = simulationData.m_Time / simulationData.m_Lifetime;
        if (normalizedTime > 1.0f)
        {
            m_DeadInstancesIndices.push_back(i);
            continue;
        }

        auto& instanceData = m_InstanceData[i];
        XMStoreFloat3(&instanceData.m_Pivot, simulationData.m_Position);
        instanceData.m_Color = simulationData.m_Color;
        instanceData.m_Color.w *= max(1.0f - normalizedTime, 0.0f);
        instanceData.m_Scale = simulationData.m_StartScale * std::lerp(1.0f, m_ScaleOverTime, normalizedTime);
    }
}

void ParticleSystem::CollectDeadParticles()
{
    for (long i = static_cast<long>(m_DeadInstancesIndices.size() - 1); i >= 0; --i)
    {
        const size_t deadIndex = m_DeadInstancesIndices[i];
        const size_t lastLiveInstanceIndex = m_LiveInstancesCount - 1;

        if (deadIndex != lastLiveInstanceIndex)
        {
            m_SimulationData[deadIndex] = m_SimulationData[lastLiveInstanceIndex];
            m_InstanceData[deadIndex] = m_InstanceData[lastLiveInstanceIndex];
        }

        m_SimulationData[lastLiveInstanceIndex].m_IsValid = false;
        m_LiveInstancesCount--;
    }

    m_DeadInstancesIndices.clear();
}

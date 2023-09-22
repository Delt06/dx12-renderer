#include "GrassChunk.h"

#include <random>

#include <DX12Library/Helpers.h>

#include "CBuffers.h"
#include "GrassIndirectCommand.h"

using namespace DirectX;

namespace
{
    struct CullGrassCBuffer
    {
        Camera::Frustum m_Frustum;
        DirectX::XMFLOAT4 m_BoundsExtents;
        uint32_t m_Count;
    };

    static std::normal_distribution<float> s_Distribution05(0.5f, 0.15f);
    static std::mt19937 s_Gen;

    constexpr uint32_t CULL_THREAD_GROUP_SIZE = 32u;

    constexpr XMFLOAT2 POISSON_DISK[] = {
        {0.94558609f, -0.76890725f},
        {-0.81544232f, -0.87912464f},
        {0.97484398f, 0.75648379f},
        {-0.91588581f, 0.45771432f},
        {-0.94201624f, -0.39906216f},
        {-0.094184101f, -0.92938870f},
        {0.34495938f, 0.29387760f},
        {-0.38277543f, 0.27676845f},
        {0.44323325f, -0.97511554f},
        {0.53742981f, -0.47373420f},
        {-0.26496911f, -0.41893023f},
        {0.79197514f, 0.19090188f},
        {-0.24188840f, 0.99706507f},
        {-0.81409955f, 0.91437590f},
        {0.19984126f, 0.78641367f},
        {0.14383161f, -0.14100790f}
    };

    constexpr XMFLOAT4 BOUNDS_EXTENTS = { 0.5f, 20.0f, 0.5f, 0.0f };

    inline float GetSignedDistanceToPlane(const Camera::FrustumPlane& plane, const XMVECTOR& position)
    {
        const auto normal = XMLoadFloat3(&plane.m_Normal);
        float distance;
        XMStoreFloat(&distance, XMVector3Dot(normal, position));
        return distance - plane.m_Distance;
    }

    inline bool IsOnPositiveSide(const Camera::FrustumPlane& plane, const Aabb& aabb)
    {
        // Compute the projection interval radius of b onto L(t) = b.c + t * p.n
        const auto normal = XMLoadFloat3(&plane.m_Normal);
        const auto absPlaneNormal = XMVectorAbs(normal);
        const auto aabbExtents = (aabb.Max - aabb.Min) * 0.5f;
        const auto aabbCenter = aabb.Min + aabbExtents;
        float r;
        XMStoreFloat(&r, XMVector3Dot(aabbExtents, absPlaneNormal));

        return -r <= GetSignedDistanceToPlane(plane, aabbCenter);
    }

    inline bool IsInsideFrustum(const Camera::Frustum& frustum, const Aabb& aabb)
    {
        for (uint32_t i = 0; i < Camera::Frustum::PLANES_COUNT; ++i)
        {
            if (!IsOnPositiveSide(frustum.m_Planes[i], aabb))
                return false;
        }

        return true;
    }
}

GrassChunk::GrassChunk(
    const std::shared_ptr<CommonRootSignature>& rootSignature,
    const Microsoft::WRL::ComPtr<ID3D12CommandSignature>& indirectCommandSignature,
    const std::shared_ptr<Mesh>& mesh,
    CommandList& commandList,
    uint32_t sideCount,
    float spacing,
    const DirectX::XMFLOAT3& origin
)
    : m_TotalCount(sideCount * sideCount)
    , m_RootSignature(rootSignature)
    , m_CommandSignature(indirectCommandSignature)
    , m_ChunkAabb{ XMLoadFloat3(&origin), XMLoadFloat3(&origin) }
{
    const auto modelsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_TotalCount * sizeof(Demo::Grass::ModelCBuffer));
    m_ModelsStructuredBuffer = std::make_shared<StructuredBuffer>(modelsBufferDesc, m_TotalCount, sizeof(Demo::Grass::ModelCBuffer), L"Models Buffer");
    const auto materialsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_TotalCount * sizeof(Demo::Grass::MaterialCBuffer));
    m_MaterialsStructuredBuffer = std::make_shared<StructuredBuffer>(materialsBufferDesc, m_TotalCount, sizeof(Demo::Grass::MaterialCBuffer), L"Materials Buffer");
    const auto positionsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_TotalCount * sizeof(XMFLOAT4));
    m_PositionsBuffer = std::make_shared<StructuredBuffer>(positionsBufferDesc, m_TotalCount, sizeof(XMFLOAT4), L"Grass Positions Buffer");

    std::vector<Demo::Grass::ModelCBuffer> modelCBuffers(m_TotalCount);
    std::vector<Demo::Grass::MaterialCBuffer> materialCBuffers(m_TotalCount);
    std::vector<XMFLOAT4> positions(m_TotalCount);

    for (size_t i = 0; i < m_TotalCount; ++i)
    {
        const auto& poissonSample = POISSON_DISK[i % _countof(POISSON_DISK)];
        XMFLOAT4 position =
        {
            (i / sideCount - sideCount * 0.5f + poissonSample.x) * spacing + origin.x,
            origin.y,
            (i % sideCount - sideCount * 0.5f + poissonSample.y) * spacing + origin.z,
            1.0f
        };
        modelCBuffers[i] = { DirectX::XMMatrixScaling(0.01f, s_Distribution05(s_Gen) * 0.02f, 0.01f) * DirectX::XMMatrixTranslation(position.x, position.y, position.z) };
        materialCBuffers[i] = { DirectX::XMFLOAT4{s_Distribution05(s_Gen) * 0.15f, s_Distribution05(s_Gen) + 0.15f, s_Distribution05(s_Gen) * 0.02f, 1.0f} };
        positions[i] = position;

        Aabb grassAabb;
        grassAabb.Min = XMLoadFloat4(&position) - XMLoadFloat4(&BOUNDS_EXTENTS);
        grassAabb.Max = grassAabb.Min + XMLoadFloat4(&BOUNDS_EXTENTS);

        m_ChunkAabb.Encapsulate(grassAabb);
    }

    commandList.CopyBuffer(*m_ModelsStructuredBuffer, m_TotalCount, sizeof(Demo::Grass::ModelCBuffer), modelCBuffers.data());
    commandList.CopyBuffer(*m_MaterialsStructuredBuffer, m_TotalCount, sizeof(Demo::Grass::MaterialCBuffer), materialCBuffers.data());
    commandList.CopyStructuredBuffer(*m_PositionsBuffer, m_TotalCount, sizeof(XMFLOAT4), positions.data());

    const auto commandsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_TotalCount * sizeof(GrassIndirectCommand));
    m_InputCommandsBuffer = std::make_shared<StructuredBuffer>(commandsBufferDesc, m_TotalCount, sizeof(GrassIndirectCommand), L"Input Grass Commands Buffer");

    std::vector<GrassIndirectCommand> commands(m_TotalCount);

    for (uint32_t i = 0; i < m_TotalCount; ++i)
    {
        auto& command = commands[i];

        command.Index = i;

        command.DrawArguments.BaseVertexLocation = 0;
        command.DrawArguments.IndexCountPerInstance = mesh->GetIndexCount();
        command.DrawArguments.InstanceCount = 1;
        command.DrawArguments.StartIndexLocation = 0;
        command.DrawArguments.StartInstanceLocation = 0;
    }

    commandList.CopyStructuredBuffer(*m_InputCommandsBuffer, commands);

    for (uint32_t i = 0; i < _countof(m_OutputCommandsBuffers); ++i)
    {
        const auto resultBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_TotalCount * sizeof(GrassIndirectCommand), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        auto buffer = std::make_shared<StructuredBuffer>(resultBufferDesc,
            m_TotalCount,
            sizeof(GrassIndirectCommand),
            L"Output Grass Commands Buffer " + std::to_wstring(i)
            );
        commandList.CopyStructuredBuffer(*buffer, m_TotalCount, sizeof(GrassIndirectCommand), nullptr);
        m_OutputCommandsBuffers[i] = buffer;
    }
}

void GrassChunk::SetFrustum(const Camera::Frustum& frustum)
{
    m_Frustum = frustum;
}

bool GrassChunk::IsVisible()
{
    return IsInsideFrustum(m_Frustum, m_ChunkAabb);
}

void GrassChunk::DispatchCulling(CommandList& commandList)
{
    const auto& outputCommandsBuffer = m_OutputCommandsBuffers[m_FrameIndex];
    m_FrameIndex = (m_FrameIndex + 1) % Window::BUFFER_COUNT;

    {
        PIXScope(commandList, "Cull Grass");

        commandList.CopyByteAddressBuffer<uint32_t>(
            outputCommandsBuffer->GetCounterBuffer(),
            0U
            );

        {
            CullGrassCBuffer cullGrassCBuffer;
            cullGrassCBuffer.m_Frustum = m_Frustum;
            cullGrassCBuffer.m_BoundsExtents = BOUNDS_EXTENTS;
            cullGrassCBuffer.m_Count = m_TotalCount;
            m_RootSignature->SetComputeConstantBuffer(commandList, cullGrassCBuffer);
        }

        {
            m_RootSignature->SetComputeShaderResourceView(commandList, 0,
                ShaderResourceView(m_PositionsBuffer)
            );
        }

        {
            m_RootSignature->SetComputeShaderResourceView(commandList, 1,
                ShaderResourceView(m_InputCommandsBuffer)
            );
        }

        {
            m_RootSignature->SetUnorderedAccessView(commandList, 0,
                UnorderedAccessView(outputCommandsBuffer)
            );
        }

        commandList.Dispatch(Math::AlignUp(m_TotalCount, CULL_THREAD_GROUP_SIZE) / CULL_THREAD_GROUP_SIZE, 1, 1);
    }
}

void GrassChunk::Draw(CommandList& commandList, bool ignoreCulling)
{
    PIXScope(commandList, "Draw Grass");

    const auto& commandsBuffer = ignoreCulling ? m_InputCommandsBuffer : m_OutputCommandsBuffers[m_FrameIndex];

    {
        commandList.TransitionBarrier(*commandsBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        commandList.TransitionBarrier(commandsBuffer->GetCounterBuffer(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        commandList.FlushResourceBarriers();
    }

    {
        m_RootSignature->SetPipelineShaderResourceView(commandList, 0, ShaderResourceView(m_ModelsStructuredBuffer));
        m_RootSignature->SetPipelineShaderResourceView(commandList, 1, ShaderResourceView(m_MaterialsStructuredBuffer));
    }

    commandList.CommitStagedDescriptors();
    commandList.GetGraphicsCommandList()->ExecuteIndirect(
        m_CommandSignature.Get(),
        m_TotalCount,
        commandsBuffer->GetD3D12Resource().Get(),
        0,
        ignoreCulling ? nullptr : commandsBuffer->GetCounterBuffer().GetD3D12Resource().Get(),
        0
    );
}

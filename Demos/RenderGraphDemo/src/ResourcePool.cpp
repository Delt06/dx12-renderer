#include "ResourcePool.h"

#include <DX12Library/Helpers.h>
#include <DX12Library/Texture.h>

#include "RenderMetadata.h"
#include "RenderPass.h"
#include "ResourceDescription.h"

namespace
{
    UINT GetMsaaQualityLevels(const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice, DXGI_FORMAT format, UINT sampleCount)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
        msLevels.Format = format;
        msLevels.SampleCount = sampleCount;
        msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        ThrowIfFailed(pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels)));
        return msLevels.NumQualityLevels;
    }

    D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice, const CD3DX12_RESOURCE_DESC& desc)
    {
        return pDevice->GetResourceAllocationInfo(0, 1, &desc);
    }

    std::shared_ptr<Texture> CreateTextureImpl(
        const RenderGraph::ResourceDescription& desc,
        const Microsoft::WRL::ComPtr<ID3D12Heap>& pHeap,
        const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice
    )
    {
        const bool useClearValue =
            desc.m_TextureUsageType == TextureUsageType::RenderTarget ||
            desc.m_TextureUsageType == TextureUsageType::Depth;
        constexpr UINT64 heapOffset = 0u;
        auto texture = std::make_shared<Texture>(
            desc.m_DxDesc,
            pHeap,
            heapOffset,
            useClearValue ? desc.m_TextureDescription.m_ClearValue : ClearValue{},
            desc.m_TextureUsageType,
            RenderGraph::ResourceIds::GetResourceName(desc.m_TextureDescription.m_Id)
        );
        texture->SetAutoBarriersEnabled(false);
        return texture;
    }
}

const Resource& RenderGraph::ResourcePool::GetResource(RenderGraph::ResourceId resourceId) const
{
    if (resourceId < m_Textures.size())
    {
        const auto& pTexture = m_Textures[resourceId];

        if (pTexture != nullptr)
        {
            return *pTexture;
        }
    }

    // TODO: add buffers here
    throw std::exception("Not implemented");
}

const std::shared_ptr<Texture>& RenderGraph::ResourcePool::GetTexture(RenderGraph::ResourceId resourceId) const
{
    Assert(resourceId < m_Textures.size(), "Resource ID out of range.");
    return m_Textures[resourceId];
}

const RenderGraph::TransientResourceAllocator::ResourceLifecycle& RenderGraph::ResourcePool::GetResourceLifecycle(RenderGraph::ResourceId resourceId)
{
    const auto& indices = m_ResourceHeapIndices[resourceId];
    return m_HeapInfos[indices.first].m_ResourceLifecycles[indices.second];
}

bool RenderGraph::ResourcePool::IsRegistered(RenderGraph::ResourceId resourceId) const
{
    return m_ResourceDescriptions.find(resourceId) != m_ResourceDescriptions.end();
}

const RenderGraph::ResourceDescription& RenderGraph::ResourcePool::GetDescription(ResourceId resourceId) const
{
    Assert(IsRegistered(resourceId), "The resource is not registered.");
    return m_ResourceDescriptions.find(resourceId)->second;
}

void RenderGraph::ResourcePool::Clear()
{
    m_ResourceDescriptions.clear();
    m_HeapInfos.clear();
    m_ResourceHeapIndices.clear();

    m_Textures.clear();
    // TODO: clear buffers
}

void RenderGraph::ResourcePool::InitHeaps(const std::vector<RenderPass*>& renderPasses, const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice)
{
    const auto lifecycles = TransientResourceAllocator::GetResourceLifecycles(renderPasses);
    m_HeapInfos = TransientResourceAllocator::CreateHeaps(lifecycles, m_ResourceDescriptions, pDevice);

    for (uint32_t heapIndex = 0; heapIndex < m_HeapInfos.size(); ++heapIndex)
    {
        const auto& heapInfo = m_HeapInfos[heapIndex];

        for (uint32_t lifecycleIndex = 0; lifecycleIndex < heapInfo.m_ResourceLifecycles.size(); ++lifecycleIndex)
        {
            const auto& lifecycle = heapInfo.m_ResourceLifecycles[lifecycleIndex];
            m_ResourceHeapIndices[lifecycle.m_Id] = std::pair<uint32_t, uint32_t>{ heapIndex, lifecycleIndex };
        }
    }
}

void RenderGraph::ResourcePool::RegisterTexture(const RenderGraph::TextureDescription& desc, const std::vector<RenderGraph::RenderPass*>& renderPasses, const RenderGraph::RenderMetadata& renderMetadata, const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice)
{
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
    auto textureUsageType = TextureUsageType::Other;
    {
        bool depth = false;
        bool unorderedAccess = false;
        bool renderTarget = false;

        for (const auto& pRenderPass : renderPasses)
        {
            for (const auto& output : pRenderPass->GetOutputs())
            {
                if (desc.m_Id == output.m_Id)
                {
                    switch (output.m_Type)
                    {
                    case RenderPass::OutputType::RenderTarget:
                        renderTarget = true;
                        break;
                    case RenderPass::OutputType::DepthRead:
                    case RenderPass::OutputType::DepthWrite:
                        depth = true;
                        break;
                    case RenderPass::OutputType::UnorderedAccess:
                        unorderedAccess = true;
                        break;
                    case RenderPass::OutputType::CopyDestination:
                        // still valid but do not have a related flag
                        break;
                    default:
                        Assert(false, "Invalid output type.");
                        break;
                    }
                }
            }
        }

        Assert(!(depth && unorderedAccess), "Textures cannot be used for depth-stencil and unordered access at the same time.");
        Assert(!(depth && renderTarget), "Textures cannot be used for depth-stencil and render target access at the same time.");

        if (depth)
        {
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            textureUsageType = TextureUsageType::Depth;
        }
        if (unorderedAccess)
        {
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        if (renderTarget)
        {
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            textureUsageType = TextureUsageType::RenderTarget;
        }
    }

    UINT msaaQualityLevels = desc.m_SampleCount == 0 ? 0 : GetMsaaQualityLevels(pDevice, desc.m_Format, desc.m_SampleCount) - 1;
    auto dxDesc = CD3DX12_RESOURCE_DESC::Tex2D(desc.m_Format,
        desc.m_WidthExpression(renderMetadata), desc.m_HeightExpression(renderMetadata),
        desc.m_ArraySize, desc.m_MipLevels,
        desc.m_SampleCount, msaaQualityLevels,
        resourceFlags);

    ResourceDescription description = {};
    description.m_Id = desc.m_Id;
    description.m_TextureDescription = desc;
    description.m_DxDesc = dxDesc;
    description.m_ResourceType = ResourceType::Texture;

    const auto allocationInfo = GetResourceAllocationInfo(pDevice, dxDesc);
    description.m_Size = allocationInfo.SizeInBytes;
    description.m_Alignment = allocationInfo.Alignment;

    m_ResourceDescriptions[desc.m_Id] = description;
}

const std::shared_ptr<Texture>& RenderGraph::ResourcePool::CreateTexture(ResourceId resourceId, const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice)
{
    Assert(IsRegistered(resourceId), "The resource is not registered.");

    const auto& resourceMetadata = m_ResourceDescriptions[resourceId];
    const auto heapIndex = m_ResourceHeapIndices[resourceId].first;
    const auto& pHeap = m_HeapInfos[heapIndex].m_Heap;
    const auto pTexture = CreateTextureImpl(resourceMetadata, pHeap, pDevice);

    if (resourceId >= m_Textures.size())
    {
        m_Textures.resize(resourceId + 1);
    }

    return m_Textures[resourceId] = pTexture;
}

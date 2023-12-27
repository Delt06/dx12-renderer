#include "ResourcePool.h"

#include <DX12Library/Buffer.h>
#include <DX12Library/ByteAddressBuffer.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/StructuredBuffer.h>
#include <DX12Library/Texture.h>

#include "RenderMetadata.h"
#include "RenderPass.h"
#include "ResourceDescription.h"

using namespace Microsoft::WRL;

namespace
{
    UINT GetMsaaQualityLevels(const ComPtr<ID3D12Device2>& pDevice, DXGI_FORMAT format, UINT sampleCount)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
        msLevels.Format = format;
        msLevels.SampleCount = sampleCount;
        msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        ThrowIfFailed(pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels)));
        return msLevels.NumQualityLevels;
    }

    D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(const ComPtr<ID3D12Device2>& pDevice, const CD3DX12_RESOURCE_DESC& desc)
    {
        return pDevice->GetResourceAllocationInfo(0, 1, &desc);
    }

    D3D12_RESOURCE_ALLOCATION_INFO GetStructuredBufferResourceAllocationInfo(const ComPtr<ID3D12Device2>& pDevice, const D3D12_RESOURCE_DESC& desc, const D3D12_RESOURCE_DESC& counterDesc)
    {
        const D3D12_RESOURCE_DESC descs[2] = { counterDesc, desc };
        return pDevice->GetResourceAllocationInfo(0, 2, descs);
    }

    std::shared_ptr<Texture> CreateTextureImpl(
        const RenderGraph::ResourceDescription& desc,
        const ComPtr<ID3D12Heap>& pHeap
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

    std::shared_ptr<Buffer> CreateBufferImpl(
        const RenderGraph::ResourceDescription& desc,
        const ComPtr<ID3D12Heap>& pHeap
    )
    {
        constexpr UINT64 heapOffset = 0u;
        std::shared_ptr<Buffer> pBuffer;
        if (desc.m_BufferDescription.m_Stride == 1)
        {
            pBuffer = std::make_shared<ByteAddressBuffer>(
                desc.m_DxDesc,
                pHeap,
                heapOffset,
                desc.m_ElementsCount, desc.m_BufferDescription.m_Stride,
                RenderGraph::ResourceIds::GetResourceName(desc.m_BufferDescription.m_Id)
            );
        }
        else
        {
            const auto pStructuredBuffer = std::make_shared<StructuredBuffer>(
                desc.m_DxDesc,
                pHeap,
                heapOffset,
                desc.m_ElementsCount, desc.m_BufferDescription.m_Stride,
                RenderGraph::ResourceIds::GetResourceName(desc.m_BufferDescription.m_Id)
            );
            // TODO: add subresources for the Resource class, override it for the structured buffer
            pStructuredBuffer->GetCounterBuffer().SetAutoBarriersEnabled(false);
            pBuffer = pStructuredBuffer;
        }

        pBuffer->SetAutoBarriersEnabled(false);

        return pBuffer;
    }
}

void RenderGraph::ResourcePool::BeginFrame(CommandList& commandList)
{
    ForEachResource([&commandList, this](const ResourceDescription& desc)
    {
        GetResource(desc.m_Id).ForEachResourceRecursive([&commandList](const Resource& resource)
        {
            commandList.TrackResource(resource);
        });
        return true;
    });

    while (!m_DeferredDeletionQueue.empty())
    {
        if (auto [_, resourceFrameIndex] = m_DeferredDeletionQueue.front();
            Application::GetFrameCount() > resourceFrameIndex + Window::BUFFER_COUNT)
        {
            m_DeferredDeletionQueue.pop();
        }
        else
        {
            break;
        }
    }
}
const Resource& RenderGraph::ResourcePool::GetResource(const ResourceId resourceId) const
{
    Assert(IsRegistered(resourceId), "Resource is not registered.");

    if (resourceId < m_ResourceInstances.size())
    {
        const ResourceInstance& resourceInstance = m_ResourceInstances[resourceId];
        return resourceInstance.GetResource();
    }

    throw std::exception("Not implemented");
}

const std::shared_ptr<Texture>& RenderGraph::ResourcePool::GetTexture(const ResourceId resourceId) const
{
    Assert(IsRegistered(resourceId), "Resource is not registered.");
    Assert(resourceId < m_ResourceInstances.size(), "Resource ID out of range.");
    Assert(m_ResourceDescriptions.at(resourceId).m_ResourceType == ResourceType::Texture, "Invalid resource type.");

    const ResourceInstance& resourceInstance = m_ResourceInstances[resourceId];
    Assert(resourceInstance.m_Type == ResourceInstanceType::Texture, "Invalid resource type.");
    return resourceInstance.m_Texture;
}

const std::shared_ptr<Buffer>& RenderGraph::ResourcePool::GetBuffer(const ResourceId resourceId) const
{
    Assert(IsRegistered(resourceId), "Resource is not registered.");
    Assert(resourceId < m_ResourceInstances.size(), "Resource ID out of range.");
    Assert(m_ResourceDescriptions.at(resourceId).m_ResourceType == ResourceType::Buffer, "Invalid resource type.");

    const ResourceInstance& resourceInstance = m_ResourceInstances[resourceId];
    Assert(resourceInstance.m_Type == ResourceInstanceType::Buffer, "Invalid resource type.");
    return resourceInstance.m_Buffer;
}

std::shared_ptr<StructuredBuffer> RenderGraph::ResourcePool::GetStructuredBuffer(const ResourceId resourceId) const
{
    const std::shared_ptr<Buffer>& pBuffer = GetBuffer(resourceId);
    const std::shared_ptr<StructuredBuffer> pStructuredBuffer = std::dynamic_pointer_cast<StructuredBuffer>(pBuffer);
    Assert(pStructuredBuffer != nullptr, "Invalid cast");
    return pStructuredBuffer;
}

std::shared_ptr<ByteAddressBuffer> RenderGraph::ResourcePool::GetByteAddressBuffer(ResourceId resourceId) const
{
    const std::shared_ptr<Buffer>& pBuffer = GetBuffer(resourceId);
    const std::shared_ptr<ByteAddressBuffer> pByteAddressBuffer = std::dynamic_pointer_cast<ByteAddressBuffer>(pBuffer);
    Assert(pByteAddressBuffer != nullptr, "Invalid cast");
    return pByteAddressBuffer;
}

void RenderGraph::ResourcePool::ForEachResource(const std::function<bool(const ResourceDescription&)>& func)
{
    for (const auto& [resourceId, resourceDescription] : m_ResourceDescriptions)
    {
        if (const bool shouldContinue = func(resourceDescription); !shouldContinue)
        {
            break;
        }
    }
}

const RenderGraph::TransientResourceAllocator::ResourceLifecycle& RenderGraph::ResourcePool::GetResourceLifecycle(ResourceId resourceId)
{
    const ResourceHeapInfo& resourceHeapInfo = m_ResourceHeapInfo[resourceId];
    const TransientResourceAllocator::HeapInfo& heapInfo = m_HeapInfos[resourceHeapInfo.m_HeapIndex];
    return heapInfo.m_ResourceLifecycles[resourceHeapInfo.m_LifecycleIndex];
}

bool RenderGraph::ResourcePool::IsRegistered(const ResourceId resourceId) const
{
    return m_ResourceDescriptions.contains(resourceId);
}

const RenderGraph::ResourceDescription& RenderGraph::ResourcePool::GetDescription(const ResourceId resourceId) const
{
    Assert(IsRegistered(resourceId), "The resource is not registered.");
    return m_ResourceDescriptions.find(resourceId)->second;
}

void RenderGraph::ResourcePool::Clear()
{
    const auto frameCount = Application::GetFrameCount();

    ForEachResource([this, frameCount](const ResourceDescription& desc)
    {
        GetResource(desc.m_Id).ForEachResourceRecursive([this, frameCount](const Resource& resource)
        {
            m_DeferredDeletionQueue.push(std::make_pair(resource.GetD3D12Resource(), frameCount));
        });
        return true;
    });

    m_ResourceDescriptions.clear();
    m_HeapInfos.clear();
    m_ResourceHeapInfo.clear();

    m_ResourceInstances.clear();
}

void RenderGraph::ResourcePool::InitHeaps(const std::vector<RenderPass*>& renderPasses, const ComPtr<ID3D12Device2>& pDevice)
{
    const auto lifecycles = TransientResourceAllocator::GetResourceLifecycles(renderPasses);
    m_HeapInfos = TransientResourceAllocator::CreateHeaps(lifecycles, m_ResourceDescriptions, pDevice);

    for (uint32_t heapIndex = 0; heapIndex < m_HeapInfos.size(); ++heapIndex)
    {
        const auto& heapInfo = m_HeapInfos[heapIndex];

        for (uint32_t lifecycleIndex = 0; lifecycleIndex < heapInfo.m_ResourceLifecycles.size(); ++lifecycleIndex)
        {
            const auto& lifecycle = heapInfo.m_ResourceLifecycles[lifecycleIndex];
            m_ResourceHeapInfo[lifecycle.m_Id] = { heapIndex, lifecycleIndex };
        }
    }
}

void RenderGraph::ResourcePool::RegisterTexture(const TextureDescription& desc, const std::vector<RenderPass*>& renderPasses, const RenderMetadata& renderMetadata, const ComPtr<ID3D12Device2>& pDevice)
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
                    case OutputType::RenderTarget:
                        renderTarget = true;
                        break;
                    case OutputType::DepthRead:
                    case OutputType::DepthWrite:
                        depth = true;
                        break;
                    case OutputType::UnorderedAccess:
                        unorderedAccess = true;
                        break;
                    case OutputType::CopyDestination:
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

    const auto width = desc.m_WidthExpression(renderMetadata);
    const auto height = desc.m_HeightExpression(renderMetadata);

    auto dxDesc = CD3DX12_RESOURCE_DESC::Tex2D(desc.m_Format,
        width, height,
        desc.m_ArraySize, desc.m_MipLevels,
        desc.m_SampleCount, msaaQualityLevels,
        resourceFlags);

    ResourceDescription description = {};
    description.m_Id = desc.m_Id;
    description.m_TextureDescription = desc;
    description.m_DxDesc = dxDesc;
    description.m_ResourceType = ResourceType::Texture;
    description.m_TextureUsageType = textureUsageType;

    const auto allocationInfo = GetResourceAllocationInfo(pDevice, dxDesc);
    description.m_TotalSize = allocationInfo.SizeInBytes;
    description.m_Alignment = allocationInfo.Alignment;

    m_ResourceDescriptions[desc.m_Id] = description;
}

void RenderGraph::ResourcePool::RegisterBuffer(const BufferDescription& desc, const std::vector<RenderPass*>& renderPasses, const RenderMetadata& renderMetadata, const ComPtr<ID3D12Device2>& pDevice)
{
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;

    bool unorderedAccess = false;

    for (const auto& pRenderPass : renderPasses)
    {
        for (const auto& output : pRenderPass->GetOutputs())
        {
            if (desc.m_Id == output.m_Id)
            {
                switch (output.m_Type)
                {
                case OutputType::UnorderedAccess:
                    unorderedAccess = true;
                    break;
                case OutputType::CopyDestination:
                    // still valid but do not have a related flag
                    break;
                default:
                    Assert(false, "Invalid output type.");
                    break;
                }
            }
        }
    }

    if (unorderedAccess)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    const size_t elementsCount = desc.m_SizeExpression(renderMetadata);
    const size_t totalSize = elementsCount * desc.m_Stride;
    const auto dxDesc = CD3DX12_RESOURCE_DESC::Buffer(totalSize, resourceFlags);

    ResourceDescription description = {};
    description.m_Id = desc.m_Id;
    description.m_BufferDescription = desc;
    description.m_DxDesc = dxDesc;
    description.m_ElementsCount = elementsCount;
    description.m_ResourceType = ResourceType::Buffer;

    const auto allocationInfo = GetStructuredBufferResourceAllocationInfo(pDevice, dxDesc, StructuredBuffer::COUNTER_DESC);
    description.m_TotalSize = allocationInfo.SizeInBytes;
    description.m_Alignment = allocationInfo.Alignment;

    m_ResourceDescriptions[desc.m_Id] = description;
}

const std::shared_ptr<Texture>& RenderGraph::ResourcePool::CreateTexture(const ResourceId resourceId)
{
    Assert(IsRegistered(resourceId), "The resource is not registered.");

    const ResourceDescription& resourceDescription = m_ResourceDescriptions[resourceId];
    const uint32_t heapIndex = m_ResourceHeapInfo[resourceId].m_HeapIndex;
    const ComPtr<ID3D12Heap>& pHeap = m_HeapInfos[heapIndex].m_Heap;
    const std::shared_ptr<Texture> pTexture = CreateTextureImpl(resourceDescription, pHeap);

    ResourceInstance resourceInstance = {};
    resourceInstance.m_Type = ResourceInstanceType::Texture;
    resourceInstance.m_Texture = pTexture;

    return AppendResourceInstance(resourceId, resourceInstance).m_Texture;
}

const std::shared_ptr<Buffer>& RenderGraph::ResourcePool::CreateBuffer(const ResourceId resourceId)
{
    Assert(IsRegistered(resourceId), "The resource is not registered.");

    const ResourceDescription& resourceMetadata = m_ResourceDescriptions[resourceId];
    const uint32_t heapIndex = m_ResourceHeapInfo[resourceId].m_HeapIndex;
    const ComPtr<ID3D12Heap>& pHeap = m_HeapInfos[heapIndex].m_Heap;
    const std::shared_ptr<Buffer> pBuffer = CreateBufferImpl(resourceMetadata, pHeap);

    ResourceInstance resourceInstance = {};
    resourceInstance.m_Type = ResourceInstanceType::Buffer;
    resourceInstance.m_Buffer = pBuffer;

    return AppendResourceInstance(resourceId, resourceInstance).m_Buffer;
}

const Resource& RenderGraph::ResourcePool::ResourceInstance::GetResource() const
{
    switch (m_Type)
    {
    case ResourceInstanceType::Texture: // NOLINT(bugprone-branch-clone)
        Assert(m_Texture != nullptr, "Invalid texture.");
        return *m_Texture;
    case ResourceInstanceType::Buffer:
        Assert(m_Buffer != nullptr, "Invalid buffer.");
        return *m_Buffer;
    default:
        Assert(false, "Invalid resource type.");
        return *m_Texture;
    }
}

RenderGraph::ResourcePool::ResourceInstance& RenderGraph::ResourcePool::AppendResourceInstance(const ResourceId resourceId, const ResourceInstance& resourceInstance)
{
    if (resourceId >= m_ResourceInstances.size())
    {
        m_ResourceInstances.resize(resourceId + 1);
    }

    return m_ResourceInstances[resourceId] = resourceInstance;
}

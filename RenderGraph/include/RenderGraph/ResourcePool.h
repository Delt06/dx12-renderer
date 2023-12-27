#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <queue>

#include <d3d12.h>
#include <wrl.h>

#include <DX12Library/Window.h>

#include "ResourceId.h"
#include "TransientResourceAllocator.h"
#include "DX12Library/CommandList.h"

class Texture;
class Buffer;
class ByteAddressBuffer;
class StructuredBuffer;
class Resource;

namespace RenderGraph
{
    class RenderPass;
    struct TextureDescription;
    struct BufferDescription;
    struct RenderMetadata;

    class ResourcePool
    {
    public:
        void BeginFrame(CommandList& commandList);

        const Resource& GetResource(ResourceId resourceId) const;
        const std::shared_ptr<Texture>& GetTexture(ResourceId resourceId) const;
        const std::shared_ptr<Buffer>& GetBuffer(ResourceId resourceId) const;
        std::shared_ptr<StructuredBuffer> GetStructuredBuffer(ResourceId resourceId) const;
        std::shared_ptr<ByteAddressBuffer> GetByteAddressBuffer(ResourceId resourceId) const;

        void ForEachResource(const std::function<bool(const ResourceDescription&)>& func);

        const TransientResourceAllocator::ResourceLifecycle& GetResourceLifecycle(ResourceId resourceId);

        bool IsRegistered(ResourceId resourceId) const;
        const ResourceDescription& GetDescription(ResourceId resourceId) const;

        void Clear();
        void InitHeaps(const std::vector<RenderPass*>& renderPasses, const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice);

        void RegisterTexture(
            const TextureDescription& desc,
            const std::vector<RenderPass*>& renderPasses,
            const RenderMetadata& renderMetadata,
            const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice);
        void RegisterBuffer(
            const BufferDescription& desc,
            const std::vector<RenderPass*>& renderPasses,
            const RenderMetadata& renderMetadata,
            const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice);

        const std::shared_ptr<Texture>& CreateTexture(ResourceId resourceId);
        const std::shared_ptr<Buffer>& CreateBuffer(ResourceId resourceId);

    private:
        enum class ResourceInstanceType
        {
            Texture,
            Buffer,
        };

        struct ResourceInstance
        {
            ResourceInstanceType m_Type;
            std::shared_ptr<Texture> m_Texture;
            std::shared_ptr<Buffer> m_Buffer;

            const Resource& GetResource() const;
        };

        ResourceInstance& AppendResourceInstance(ResourceId resourceId, const ResourceInstance& resourceInstance);

        std::vector<ResourceInstance> m_ResourceInstances;
        std::map<ResourceId, ResourceDescription> m_ResourceDescriptions;
        std::vector<TransientResourceAllocator::HeapInfo> m_HeapInfos;

        std::queue<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, uint64_t>> m_DeferredDeletionQueue;

        struct ResourceHeapInfo
        {
            uint32_t m_HeapIndex;
            uint32_t m_LifecycleIndex;
        };

        std::map<ResourceId, ResourceHeapInfo> m_ResourceHeapInfo;
    };
}

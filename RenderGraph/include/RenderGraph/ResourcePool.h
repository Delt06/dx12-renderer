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
        const std::shared_ptr<StructuredBuffer>& GetBuffer(ResourceId resourceId) const;
        const std::vector<std::shared_ptr<Texture>>& GetAllTextures() const { return m_Textures; }
        const std::vector<std::shared_ptr<StructuredBuffer>>& GetAllBuffers() const { return m_Buffers; }

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
        const std::shared_ptr<StructuredBuffer>& CreateBuffer(ResourceId resourceId);

    private:
        std::vector<std::shared_ptr<Texture>> m_Textures;
        std::vector<std::shared_ptr<StructuredBuffer>> m_Buffers;
        std::map<ResourceId, ResourceDescription> m_ResourceDescriptions;
        std::vector<TransientResourceAllocator::HeapInfo> m_HeapInfos;

        std::queue<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t>> m_DeferredDeletionQueue;

        // pair: heap index, lifecycle index
        std::map<ResourceId, std::pair<uint32_t, uint32_t>> m_ResourceHeapIndices;
    };
}

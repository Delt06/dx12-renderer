#pragma once

#include <map>
#include <memory>
#include <vector>

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>

#include "RenderPass.h"
#include "RenderMetadata.h"
#include "ResourceDescription.h"

namespace RenderGraph
{
    class RenderGraphRoot
    {
    public:
        RenderGraphRoot(
            std::vector<std::unique_ptr<RenderPass>>&& renderPasses,
            std::vector<TextureDescription>&& textures,
            std::vector<BufferDescription>&& buffers
        );

        void Execute(const RenderMetadata& renderMetadata);
        void MarkDirty();

    private:
        void Build(const RenderMetadata& renderMetadata);
        void PrepareResourceForRenderPass(CommandList& commandList, const RenderPass& renderPass);

        D3D12_RESOURCE_STATES GetCurrentResourceState(const Resource& resource) const;
        void SetCurrentResourceState(const Resource& resource, D3D12_RESOURCE_STATES state);
        void TransitionBarrier(const Resource& resource, D3D12_RESOURCE_STATES stateAfter);
        void UavBarrier(const Resource& resource);
        void FlushBarriers(CommandList& commandList);

        const std::shared_ptr<CommandQueue> m_DirectCommandQueue;

        const std::vector<std::unique_ptr<RenderPass>> m_RenderPassesDescription;
        std::vector<std::vector<RenderPass*>> m_RenderPassesSorted;
        std::vector<RenderPass*> m_RenderPassesBuilt;

        const std::vector<TextureDescription> m_TextureDescriptions;
        const std::vector<BufferDescription> m_BufferDescriptions;

        std::vector<std::shared_ptr<Texture>> m_Textures;
        std::map<const RenderPass*, std::shared_ptr<RenderTarget>> m_RenderTargets;
        std::map<const Resource*, D3D12_RESOURCE_STATES> m_ResourceStates;
        std::vector<D3D12_RESOURCE_BARRIER> m_PendingBarriers;

        bool m_Dirty = true;
    };
}

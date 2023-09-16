#pragma once

#include <map>
#include <memory>
#include <vector>

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Window.h>

#include "RenderPass.h"
#include "RenderMetadata.h"
#include "ResourceDescription.h"
#include "ResourcePool.h"

namespace RenderGraph
{
    class RenderGraphRoot
    {
    public:
        RenderGraphRoot(
            std::vector<std::unique_ptr<RenderPass>>&& renderPasses,
            std::vector<TextureDescription>&& textures,
            std::vector<BufferDescription>&& buffers,
            std::vector<TokenDescription>&& tokens
        );

        void Execute(const RenderMetadata& renderMetadata);
        void Present(const std::shared_ptr<Window>& pWindow, ResourceId resourceId = ResourceIds::GraphOutput);
        void MarkDirty();

        struct RenderTargetInfo
        {
            std::shared_ptr<RenderTarget> m_RenderTarget = nullptr;
            bool m_ReadonlyDepth = false;
        };

    private:
        void CheckPotentiallyDirtyResources(const RenderMetadata& renderMetadata);
        void Build(const RenderMetadata& renderMetadata);
        void PrepareResourceForRenderPass(CommandList& commandList, const RenderPass& renderPass, uint32_t renderPassIndex);

        D3D12_RESOURCE_STATES GetCurrentResourceState(const Resource& resource) const;
        void SetCurrentResourceState(const Resource& resource, D3D12_RESOURCE_STATES state);
        void TransitionBarrier(const Resource& resource, D3D12_RESOURCE_STATES stateAfter);
        void UavBarrier(const Resource& resource);
        void AliasingBarrier(const Resource& resourceAfter);
        void FlushBarriers(CommandList& commandList);

        bool IsResourceDefined(ResourceId id);

        std::shared_ptr<CommandQueue> m_DirectCommandQueue;

        const std::vector<std::unique_ptr<RenderPass>> m_RenderPassesDescription;
        std::vector<std::vector<RenderPass*>> m_RenderPassesSorted;
        std::vector<RenderPass*> m_RenderPassesBuilt;

        std::vector<TextureDescription> m_TextureDescriptions;
        std::vector<BufferDescription> m_BufferDescriptions;
        std::vector<TokenDescription> m_TokenDescriptions;

        std::shared_ptr<ResourcePool> m_ResourcePool;
        std::map<const RenderPass*, RenderTargetInfo> m_RenderTargets;
        std::map<const Resource*, D3D12_RESOURCE_STATES> m_ResourceStates;
        std::vector<D3D12_RESOURCE_BARRIER> m_PendingBarriers;

        bool m_Dirty = true;
    };
}

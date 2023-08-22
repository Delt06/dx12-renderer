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

        const std::shared_ptr<CommandQueue> m_DirectCommandQueue;

        const std::vector<std::unique_ptr<RenderPass>> m_RenderPassesDescription;
        std::vector<std::vector<RenderPass*>> m_RenderPassesSorted;
        std::vector<RenderPass*> m_RenderPassesBuilt;

        const std::vector<TextureDescription> m_TextureDescriptions;
        const std::vector<BufferDescription> m_BufferDescriptions;

        std::vector<std::shared_ptr<Texture>> m_Textures;
        std::map<RenderPass*, std::shared_ptr<RenderTarget>> m_RenderTargets;

        bool m_Dirty = true;
    };
}

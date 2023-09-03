#pragma once

#include <memory>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "ResourceId.h"

class Texture;
class Resource;

namespace RenderGraph
{
    class RenderPass;
    struct TextureDescription;
    struct RenderMetadata;

    class ResourcePool
    {
    public:
        const Resource& GetResource(ResourceId resourceId) const;
        const std::shared_ptr<Texture>& GetTexture(ResourceId resourceId) const;
        const std::vector<std::shared_ptr<Texture>>& GetAllTextures() const { return m_Textures; }

        void Clear();
        const std::shared_ptr<Texture>& AddTexture(
            const TextureDescription& desc,
            const std::vector<std::unique_ptr<RenderPass>>& renderPasses,
            const RenderMetadata& renderMetadata,
            const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice);

    private:
        std::vector<std::shared_ptr<Texture>> m_Textures;
    };
}

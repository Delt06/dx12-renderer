#include "ResourcePool.h"

#include <DX12Library/Helpers.h>
#include <DX12Library/Texture.h>

#include "RenderMetadata.h"
#include "RenderPass.h"
#include "ResourceDescription.h"

namespace
{
    UINT GetMsaaQualityLevels(Microsoft::WRL::ComPtr<ID3D12Device2> device, DXGI_FORMAT format, UINT sampleCount)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
        msLevels.Format = format;
        msLevels.SampleCount = sampleCount;
        msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels)));
        return msLevels.NumQualityLevels;
    }

    std::shared_ptr<Texture> CreateTexture(
        const RenderGraph::TextureDescription& desc,
        const std::vector<std::unique_ptr<RenderGraph::RenderPass>>& renderPasses,
        const RenderGraph::RenderMetadata& renderMetadata,
        const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice
    )
    {
        using namespace RenderGraph;

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

        D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
        auto textureUsageType = TextureUsageType::Other;

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


        UINT msaaQualityLevels = desc.m_SampleCount == 0 ? 0 : GetMsaaQualityLevels(pDevice, desc.m_Format, desc.m_SampleCount) - 1;
        auto dxDesc = CD3DX12_RESOURCE_DESC::Tex2D(desc.m_Format,
            desc.m_WidthExpression(renderMetadata), desc.m_HeightExpression(renderMetadata),
            desc.m_ArraySize, desc.m_MipLevels,
            desc.m_SampleCount, msaaQualityLevels,
            resourceFlags);

        auto texture = std::make_shared<Texture>(dxDesc, depth || renderTarget ? desc.m_ClearValue : ClearValue{},
            textureUsageType,
            ResourceIds::GetResourceName(desc.m_Id)
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

void RenderGraph::ResourcePool::Clear()
{
    m_Textures.clear();

    // TODO: clear buffers
}

const std::shared_ptr<Texture>& RenderGraph::ResourcePool::AddTexture(const RenderGraph::TextureDescription& desc, const std::vector<std::unique_ptr<RenderGraph::RenderPass>>& renderPasses, const RenderGraph::RenderMetadata& renderMetadata, const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice)
{
    auto pTexture = CreateTexture(desc, renderPasses, renderMetadata, pDevice);

    if (desc.m_Id >= m_Textures.size())
    {
        m_Textures.resize(desc.m_Id + 1);
    }

    m_Textures[desc.m_Id] = pTexture;

    return m_Textures[desc.m_Id];
}

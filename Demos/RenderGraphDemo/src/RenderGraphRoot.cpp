#include "RenderGraphRoot.h"

#include <d3d12.h>
#include <d3dx12.h>

#include <DX12Library/Helpers.h>

namespace
{
    bool DirectlyDependsOn(const RenderGraph::RenderPass& pass1, const RenderGraph::RenderPass& pass2)
    {
        for (const auto& input : pass1.GetInputs())
        {
            for (const auto& output : pass2.GetOutputs())
            {
                if (input.m_Id == output.m_Id)
                {
                    return true;
                }
            }
        }

        return false;
    }

    std::vector<std::vector<RenderGraph::RenderPass*>> TopologicalSort(const std::vector<std::unique_ptr<RenderGraph::RenderPass>>& renderPassesDescription)
    {
        std::vector<RenderGraph::RenderPass*> tempList;
        tempList.reserve(renderPassesDescription.size());

        std::vector<std::vector<RenderGraph::RenderPass*>> result;

        for (const auto& pRenderPass : renderPassesDescription)
        {
            tempList.push_back(pRenderPass.get());
        }

        while (tempList.size() > 0)
        {
            std::vector<RenderGraph::RenderPass*> passesWithNoDependencies;

            for (const auto& pass : tempList)
            {
                bool hasDependencies = false;

                for (const auto& otherPass : tempList)
                {
                    if (pass == otherPass)
                    {
                        continue;
                    }


                    if (DirectlyDependsOn(*pass, *otherPass))
                    {
                        hasDependencies = true;
                        break;
                    }
                }

                if (!hasDependencies)
                {
                    passesWithNoDependencies.push_back(pass);
                }
            }

            if (passesWithNoDependencies.size() > 0)
            {
                for (const auto& pass : passesWithNoDependencies)
                {
                    tempList.erase(std::remove(tempList.begin(), tempList.end(), pass), tempList.end());
                }

                result.emplace_back(std::move(passesWithNoDependencies));
            }
            else
            {
                Assert(false, "Render graph has a loop.");
            }
        }

        return result;
    }

    bool ResourceIsDefined(RenderGraph::ResourceId id, const std::vector<RenderGraph::TextureDescription>& textures, const std::vector<RenderGraph::BufferDescription>& buffers)
    {
        for (const auto& texture : textures)
        {
            if (texture.m_Id == id)
                return true;
        }

        for (const auto& buffer : buffers)
        {
            if (buffer.m_Id == id)
                return true;
        }

        return false;
    }

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

    std::shared_ptr<RenderTarget> CreateRenderTargetOrDefault(const RenderGraph::RenderPass& renderPass, const std::vector<std::shared_ptr<Texture>>& textures)
    {
        using namespace RenderGraph;

        std::shared_ptr<RenderTarget> pRenderTarget = nullptr;
        uint32_t colorTexturesCount = 0;
        uint32_t depthTexturesCount = 0;

        for (const auto& output : renderPass.GetOutputs())
        {
            switch (output.m_Type)
            {
            case RenderPass::OutputType::RenderTarget:
                {
                    if (pRenderTarget == nullptr)
                    {
                        pRenderTarget = std::make_shared<RenderTarget>();
                    }

                    const auto& pTexture = textures[output.m_Id];
                    Assert(pTexture != nullptr, "Texture is null.");

                    AttachmentPoint attachmentPoint = (AttachmentPoint)((uint32_t)Color0 + colorTexturesCount);
                    pRenderTarget->AttachTexture(attachmentPoint, pTexture);
                    colorTexturesCount++;
                    Assert(colorTexturesCount <= 8, "Too many color textures for the same render target");
                    break;
                }

            case RenderPass::OutputType::DepthWrite:
            case RenderPass::OutputType::DepthRead:
                {
                    if (pRenderTarget == nullptr)
                    {
                        pRenderTarget = std::make_shared<RenderTarget>();
                    }

                    const auto& pTexture = textures[output.m_Id];
                    Assert(pTexture != nullptr, "Texture is null.");

                    pRenderTarget->AttachTexture(DepthStencil, pTexture);
                    depthTexturesCount++;
                    Assert(depthTexturesCount == 1, "Too many depth textures for the same render target");
                }
                break;
            }
        }

        return pRenderTarget;
    }

    const Resource& GetResource(RenderGraph::ResourceId id, const std::vector<std::shared_ptr<Texture>>& textures)
    {
        const auto& pTexture = textures[id];

        if (pTexture != nullptr)
        {
            return *pTexture;
        }

        // TODO: add buffers here
        throw std::exception("Not implemented");
    }
}

RenderGraph::RenderGraphRoot::RenderGraphRoot(
    std::vector<std::unique_ptr<RenderPass>>&& renderPasses,
    std::vector<RenderGraph::TextureDescription>&& textures,
    std::vector<RenderGraph::BufferDescription>&& buffers
)
    : m_DirectCommandQueue(Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT))
    , m_RenderPassesDescription(std::move(renderPasses))
    , m_TextureDescriptions(textures)
    , m_BufferDescriptions(buffers)
{
    for (auto& pRenderPass : m_RenderPassesDescription)
    {
        pRenderPass->Init();
    }

    m_RenderPassesSorted = std::move(TopologicalSort(m_RenderPassesDescription));

    // Ensure all resources are defined
    {
        for (const auto& pass : m_RenderPassesDescription)
        {
            for (const auto& input : pass->GetInputs())
            {
                Assert(ResourceIsDefined(input.m_Id, m_TextureDescriptions, m_BufferDescriptions), "Input undefined.");
            }

            for (const auto& output : pass->GetOutputs())
            {
                Assert(ResourceIsDefined(output.m_Id, m_TextureDescriptions, m_BufferDescriptions), "Output undefined.");
            }
        }
    }
}

void RenderGraph::RenderGraphRoot::Execute(const RenderMetadata& renderMetadata)
{
    if (m_Dirty)
    {
        Build(renderMetadata);
        m_Dirty = false;
    }

    auto pCommandList = m_DirectCommandQueue->GetCommandList();

    for (const auto& pRenderPass : m_RenderPassesBuilt)
    {
        for (const auto& input : pRenderPass->GetInputs())
        {
            const auto& resource = GetResource(input.m_Id, m_Textures);
            pCommandList->TransitionBarrier(resource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        }

        const auto& renderTargetFindResult = m_RenderTargets.find(pRenderPass);
        if (renderTargetFindResult != m_RenderTargets.end())
        {
            const auto& pRenderTarget = renderTargetFindResult->second;
            const auto& textures = pRenderTarget->GetTextures();

            for (size_t i = 0; i < 8; ++i)
            {
                const auto& pRtTexture = textures[i];
                if (pRtTexture != nullptr)
                {
                    pCommandList->TransitionBarrier(*pRtTexture, D3D12_RESOURCE_STATE_RENDER_TARGET);
                }
            }

            const auto& pRtDepthStencil = pRenderTarget->GetTexture(DepthStencil);
            if (pRtDepthStencil != nullptr)
            {
                // TODO: handle ability for depth read state here
                pCommandList->TransitionBarrier(*pRtDepthStencil, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            }

            pCommandList->SetRenderTarget(*pRenderTarget);
        }

        for (const auto& output : pRenderPass->GetOutputs())
        {
            if (output.m_Type == RenderPass::OutputType::UnorderedAccess)
            {
                const auto& resource = GetResource(output.m_Id, m_Textures);
                pCommandList->UavBarrier(resource);
            }
        }

        pRenderPass->Execute(*pCommandList);
    }

    m_DirectCommandQueue->ExecuteCommandList(pCommandList);
}

void RenderGraph::RenderGraphRoot::MarkDirty()
{
    m_Dirty = true;
}

void RenderGraph::RenderGraphRoot::Build(const RenderMetadata& renderMetadata)
{
    const auto& application = Application::Get();
    const auto& pDevice = application.GetDevice();

    // Populate the final render pass list
    m_RenderPassesBuilt.clear();

    // TODO: implement unused node stripping, resource lifetime, etc...
    for (const auto& innerList : m_RenderPassesSorted)
    {
        for (const auto& pRenderPass : innerList)
        {
            m_RenderPassesBuilt.push_back(pRenderPass);
        }
    }

    // Create textures
    m_Textures.clear();

    for (const auto& desc : m_TextureDescriptions)
    {
        auto texture = CreateTexture(desc, m_RenderPassesDescription, renderMetadata, pDevice);

        if (desc.m_Id >= m_Textures.size())
        {
            m_Textures.resize(desc.m_Id + 1);
        }

        m_Textures[desc.m_Id] = texture;
    }

    // TODO: add buffers here

    // Create render targets
    m_RenderTargets.clear();

    for (const auto& pRenderPass : m_RenderPassesDescription)
    {
        const auto& pRenderTarget = CreateRenderTargetOrDefault(*pRenderPass, m_Textures);

        if (pRenderTarget != nullptr)
        {
            m_RenderTargets.insert(std::pair<RenderPass*, std::shared_ptr<RenderTarget>>{ pRenderPass.get(), pRenderTarget});
        }
    }
}

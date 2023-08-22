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
    auto& cmd = *pCommandList;
    Assert(m_PendingBarriers.size() == 0, "Pending barriers were left from after the previous frame.");

    {
        PIXScope(cmd, L"Render Graph");

        for (const auto& pRenderPass : m_RenderPassesBuilt)
        {
            PIXScope(cmd, pRenderPass->GetPassName().c_str());

            PrepareResourceForRenderPass(cmd, *pRenderPass);
            pRenderPass->Execute(cmd);
        }
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

    // Create resources
    {
        m_ResourceStates.clear();

        // Create textures
        m_Textures.clear();

        for (const auto& desc : m_TextureDescriptions)
        {
            auto pTexture = CreateTexture(desc, m_RenderPassesDescription, renderMetadata, pDevice);
            SetCurrentResourceState(*pTexture, D3D12_RESOURCE_STATE_COMMON);

            if (desc.m_Id >= m_Textures.size())
            {
                m_Textures.resize(desc.m_Id + 1);
            }

            m_Textures[desc.m_Id] = pTexture;
        }

        // TODO: add buffers here
    }


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

D3D12_RESOURCE_STATES RenderGraph::RenderGraphRoot::GetCurrentResourceState(const Resource &resource) const
{
    const auto& result = m_ResourceStates.find(&resource);
    Assert(result != m_ResourceStates.end(), "Resource does not have a registered state");
    return result->second;
}

void RenderGraph::RenderGraphRoot::PrepareResourceForRenderPass(CommandList& commandList, const RenderPass& renderPass)
{
    // SRV barriers
    for (const auto& input : renderPass.GetInputs())
    {
        const auto& resource = GetResource(input.m_Id, m_Textures);
        TransitionBarrier(resource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    }

    const auto& renderTargetFindResult = m_RenderTargets.find(&renderPass);
    if (renderTargetFindResult != m_RenderTargets.end())
    {
        const auto& pRenderTarget = renderTargetFindResult->second;
        const auto& textures = pRenderTarget->GetTextures();

        // Render Target barriers
        for (size_t i = 0; i < 8; ++i)
        {
            const auto& pRtTexture = textures[i];
            if (pRtTexture != nullptr && pRtTexture->IsValid())
            {
                TransitionBarrier(*pRtTexture, D3D12_RESOURCE_STATE_RENDER_TARGET);
            }
        }

        // Depth-Stencil barriers
        const auto& pRtDepthStencil = pRenderTarget->GetTexture(DepthStencil);
        if (pRtDepthStencil != nullptr && pRtDepthStencil->IsValid())
        {
            // TODO: handle ability for depth read state here
            TransitionBarrier(*pRtDepthStencil, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }

        commandList.SetRenderTarget(*pRenderTarget);
        commandList.SetAutomaticViewportAndScissorRect(*pRenderTarget);
    }

    // UAV barriers
    for (const auto& output : renderPass.GetOutputs())
    {
        if (output.m_Type == RenderPass::OutputType::UnorderedAccess)
        {
            const auto& resource = GetResource(output.m_Id, m_Textures);
            TransitionBarrier(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            UavBarrier(resource);
        }
    }

    FlushBarriers(commandList);
}

void RenderGraph::RenderGraphRoot::SetCurrentResourceState(const Resource& resource, D3D12_RESOURCE_STATES state)
{
    auto existingEntry = m_ResourceStates.find(&resource);
    if (existingEntry == m_ResourceStates.end())
    {
        m_ResourceStates.insert(std::pair<const Resource*, D3D12_RESOURCE_STATES> {&resource, state});
    }
    else
    {
        existingEntry->second = state;
    }
}

void RenderGraph::RenderGraphRoot::TransitionBarrier(const Resource& resource, D3D12_RESOURCE_STATES stateAfter)
{
    const auto stateBefore = GetCurrentResourceState(resource);
    if (stateBefore == stateAfter)
    {
        // no need for a barrier
        return;
    }

    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.GetD3D12Resource().Get(), stateBefore, stateAfter);
    m_PendingBarriers.push_back(barrier);

    SetCurrentResourceState(resource, stateAfter);
}

void RenderGraph::RenderGraphRoot::UavBarrier(const Resource& resource)
{
    Assert(GetCurrentResourceState(resource) == D3D12_RESOURCE_STATE_UNORDERED_ACCESS, "Resource is supposed to be in UAV state to issue a UAV barrier.");

    // TODO: skip if there was a transition barrier after the previous UAV usage
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.GetD3D12Resource().Get());
    m_PendingBarriers.push_back(barrier);
}

void RenderGraph::RenderGraphRoot::FlushBarriers(CommandList& commandList)
{
    if (m_PendingBarriers.size() == 0)
    {
        return;
    }

    const auto& pDxCmd = commandList.GetGraphicsCommandList();
    pDxCmd->ResourceBarrier((UINT)m_PendingBarriers.size(), m_PendingBarriers.data());
    m_PendingBarriers.clear();
}

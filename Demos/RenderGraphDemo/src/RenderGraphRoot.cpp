#include "RenderGraphRoot.h"

#include <set>
#include <queue>

#include <d3d12.h>
#include <d3dx12.h>

#include <DX12Library/Helpers.h>

#include "RenderContext.h"

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

    std::set<RenderGraph::RenderPass*> FindUnusedPasses(const std::vector<std::vector<RenderGraph::RenderPass*>>& sortedRenderPasses)
    {
        std::set<RenderGraph::ResourceId> usedResources;
        usedResources.insert(RenderGraph::ResourceIds::GraphOutput);

        std::set<RenderGraph::RenderPass*> unusedPasses;

        // initially, all are marked as unused
        for (const auto& passList : sortedRenderPasses)
        {
            for (const auto& pPass : passList)
            {
                unusedPasses.insert(pPass);
            }
        }

        for (auto it = sortedRenderPasses.rbegin(); it != sortedRenderPasses.rend(); ++it)
        {
            const auto& passList = *it;

            for (const auto& pPass : passList)
            {
                const auto& outputs = pPass->GetOutputs();

                // check if any of the outputs is used
                const auto findResult = std::find_if(
                    outputs.begin(), outputs.end(),
                    [&usedResources](const RenderGraph::RenderPass::Output& o) { return usedResources.contains(o.m_Id); }
                );

                if (findResult != outputs.end())
                {
                    // if the pass is used, mark all its inputs is used as well
                    for (const auto& input : pPass->GetInputs())
                    {
                        usedResources.insert(input.m_Id);
                    }

                    unusedPasses.erase(pPass);
                }
            }
        }

        return unusedPasses;
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

    RenderGraph::RenderGraphRoot::RenderTargetInfo CreateRenderTargetOrDefault(const RenderGraph::RenderPass& renderPass, const std::vector<std::shared_ptr<Texture>>& textures)
    {
        using namespace RenderGraph;

        RenderGraphRoot::RenderTargetInfo renderTargetInfo = {};
        uint32_t colorTexturesCount = 0;
        uint32_t depthTexturesCount = 0;

        for (const auto& output : renderPass.GetOutputs())
        {
            switch (output.m_Type)
            {
            case RenderPass::OutputType::RenderTarget:
                {
                    if (renderTargetInfo.m_RenderTarget == nullptr)
                    {
                        renderTargetInfo.m_RenderTarget = std::make_shared<RenderTarget>();
                    }

                    const auto& pTexture = textures[output.m_Id];
                    Assert(pTexture != nullptr, "Texture is null.");

                    AttachmentPoint attachmentPoint = (AttachmentPoint)((uint32_t)Color0 + colorTexturesCount);
                    renderTargetInfo.m_RenderTarget->AttachTexture(attachmentPoint, pTexture);

                    if (output.m_InitAction == RenderPass::OutputInitAction::Clear)
                    {
                        renderTargetInfo.m_ClearColor[Color0 + colorTexturesCount] = true;
                    }

                    colorTexturesCount++;
                    Assert(colorTexturesCount <= 8, "Too many color textures for the same render target");

                    break;
                }

            case RenderPass::OutputType::DepthWrite:
            case RenderPass::OutputType::DepthRead:
                {
                    if (renderTargetInfo.m_RenderTarget == nullptr)
                    {
                        renderTargetInfo.m_RenderTarget = std::make_shared<RenderTarget>();
                    }

                    const auto& pTexture = textures[output.m_Id];
                    Assert(pTexture != nullptr, "Texture is null.");

                    renderTargetInfo.m_RenderTarget->AttachTexture(DepthStencil, pTexture);
                    depthTexturesCount++;
                    Assert(depthTexturesCount == 1, "Too many depth textures for the same render target");

                    if (output.m_Type == RenderPass::OutputType::DepthRead)
                    {
                        renderTargetInfo.m_ReadonlyDepth = true;
                    }

                    if (output.m_InitAction == RenderPass::OutputInitAction::Clear)
                    {
                        renderTargetInfo.m_ClearDepth = true;
                        renderTargetInfo.m_ClearStencil = true;
                    }
                }
                break;
            }
        }

        return renderTargetInfo;
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
    , m_ResourcePool(std::make_shared<ResourcePool>())
{
    {
        auto pCommandList = m_DirectCommandQueue->GetCommandList();

        {
            PIXScope(*pCommandList, L"Render Graph: Init");

            for (auto& pRenderPass : m_RenderPassesDescription)
            {
                pRenderPass->Init(*pCommandList);
            }
        }

        m_DirectCommandQueue->ExecuteCommandList(pCommandList);
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
        PIXScope(cmd, L"Render Graph: Execute");

        RenderContext context = {};
        context.m_ResourcePool = m_ResourcePool;
        context.m_Metadata = renderMetadata;

        for (const auto& pRenderPass : m_RenderPassesBuilt)
        {
            PIXScope(cmd, pRenderPass->GetPassName().c_str());

            PrepareResourceForRenderPass(cmd, *pRenderPass);
            pRenderPass->Execute(context, cmd);
        }
    }

    m_DirectCommandQueue->ExecuteCommandList(pCommandList);
}

void RenderGraph::RenderGraphRoot::Present(const std::shared_ptr<Window>& pWindow, RenderGraph::ResourceId resourceId)
{
    const auto& pTexture = m_ResourcePool->GetTexture(resourceId);

    auto pCommandList = m_DirectCommandQueue->GetCommandList();

    {
        PIXScope(*pCommandList, L"Render Graph: Prepare Present");

        if (pTexture->GetD3D12ResourceDesc().SampleDesc.Count > 1)
        {
            TransitionBarrier(*pTexture, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        }
        else
        {
            TransitionBarrier(*pTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
        }

        FlushBarriers(*pCommandList);
    }

    m_DirectCommandQueue->ExecuteCommandList(pCommandList);
    pWindow->Present(*pTexture);
}

void RenderGraph::RenderGraphRoot::MarkDirty()
{
    m_Dirty = true;
}

void RenderGraph::RenderGraphRoot::Build(const RenderMetadata& renderMetadata)
{
    const auto& application = Application::Get();
    const auto& pDevice = application.GetDevice();

    const auto unusedPasses = FindUnusedPasses(m_RenderPassesSorted);

    // Populate the final render pass list
    m_RenderPassesBuilt.clear();

    // TODO: implement resource lifetime, etc...
    for (const auto& innerList : m_RenderPassesSorted)
    {
        for (const auto& pRenderPass : innerList)
        {
            if (!unusedPasses.contains(pRenderPass))
            {
                m_RenderPassesBuilt.push_back(pRenderPass);
            }
        }
    }

    // Create resources
    {
        m_ResourceStates.clear();

        m_ResourcePool->Clear();

        for (const auto& desc : m_TextureDescriptions)
        {
            const auto& pTexture = m_ResourcePool->AddTexture(desc, m_RenderPassesDescription, renderMetadata, pDevice);
            SetCurrentResourceState(*pTexture, D3D12_RESOURCE_STATE_COMMON);
        }

        // TODO: add buffers here
    }


    // Create render targets
    m_RenderTargets.clear();

    for (const auto& pRenderPass : m_RenderPassesDescription)
    {
        RenderTargetInfo renderTargetInfo = CreateRenderTargetOrDefault(*pRenderPass, m_ResourcePool->GetAllTextures());

        if (renderTargetInfo.m_RenderTarget != nullptr)
        {
            m_RenderTargets.insert(std::pair<RenderPass*, RenderTargetInfo>{ pRenderPass.get(), renderTargetInfo});
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
    for (const auto& input : renderPass.GetInputs())
    {
        const auto& resource = m_ResourcePool->GetResource(input.m_Id);

        // SRV barriers
        if (input.m_Type == RenderPass::InputType::ShaderResource)
        {
            TransitionBarrier(resource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        }

        if (input.m_Type == RenderPass::InputType::CopySource)
        {
            TransitionBarrier(resource, D3D12_RESOURCE_STATE_COPY_SOURCE);
        }
    }

    // Render Target barriers
    const auto& renderTargetFindResult = m_RenderTargets.find(&renderPass);
    if (renderTargetFindResult != m_RenderTargets.end())
    {
        const auto& renderTargetInfo = renderTargetFindResult->second;
        const auto& pRenderTarget = renderTargetInfo.m_RenderTarget;
        const auto& textures = pRenderTarget->GetTextures();

        // Render Target barriers for color attachments
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
            const auto stateAfter = renderTargetInfo.m_ReadonlyDepth ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
            TransitionBarrier(*pRtDepthStencil, stateAfter);
        }
    }


    for (const auto& output : renderPass.GetOutputs())
    {
        const auto& resource = m_ResourcePool->GetResource(output.m_Id);

        // Copy Destination barriers
        if (output.m_Type == RenderPass::OutputType::CopyDestination)
        {
            TransitionBarrier(resource, D3D12_RESOURCE_STATE_COPY_DEST);
        }

        // UAV barriers
        if (output.m_Type == RenderPass::OutputType::UnorderedAccess)
        {
            TransitionBarrier(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            UavBarrier(resource);
        }
    }



    FlushBarriers(commandList);

    // Setup the render target
    if (renderTargetFindResult != m_RenderTargets.end())
    {
        const auto& renderTargetInfo = renderTargetFindResult->second;
        const auto& pRenderTarget = renderTargetInfo.m_RenderTarget;
        const auto& textures = pRenderTarget->GetTextures();

        commandList.SetRenderTarget(*pRenderTarget);
        commandList.SetAutomaticViewportAndScissorRect(*pRenderTarget);

        for (size_t i = 0; i < 8; ++i)
        {
            const auto& pRtTexture = textures[i];
            if (pRtTexture != nullptr && pRtTexture->IsValid())
            {
                if (renderTargetInfo.m_ClearColor[i])
                {
                    commandList.ClearTexture(*pRtTexture, pRtTexture->GetD3D12ClearValue().Color);
                }
            }
        }

        if (renderTargetInfo.m_ClearDepth || renderTargetInfo.m_ClearStencil)
        {
            const auto& pRtDepthStencil = pRenderTarget->GetTexture(DepthStencil);
            if (pRtDepthStencil != nullptr && pRtDepthStencil->IsValid())
            {
                D3D12_CLEAR_FLAGS clearFlags{};
                if (renderTargetInfo.m_ClearDepth)
                {
                    clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
                }
                if (renderTargetInfo.m_ClearStencil)
                {
                    clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
                }
                commandList.ClearDepthStencilTexture(*pRtDepthStencil, clearFlags);
            }
        }
    }
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

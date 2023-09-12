#include "RenderGraphRoot.h"

#include <functional>
#include <set>
#include <queue>

#include <d3d12.h>
#include <d3dx12.h>

#include <DX12Library/Helpers.h>
#include <DX12Library/StructuredBuffer.h>
#include <DX12Library/Texture.h>

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
                    [&usedResources](const RenderGraph::Output& o) { return usedResources.contains(o.m_Id); }
                );

                if (findResult != outputs.end())
                {
                    // if the pass is used, mark all its inputs as used as well
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
            case OutputType::RenderTarget:
                {
                    if (renderTargetInfo.m_RenderTarget == nullptr)
                    {
                        renderTargetInfo.m_RenderTarget = std::make_shared<RenderTarget>();
                    }

                    const auto& pTexture = textures[output.m_Id];
                    Assert(pTexture != nullptr, "Texture is null.");

                    AttachmentPoint attachmentPoint = (AttachmentPoint)((uint32_t)Color0 + colorTexturesCount);
                    renderTargetInfo.m_RenderTarget->AttachTexture(attachmentPoint, pTexture);

                    colorTexturesCount++;
                    Assert(colorTexturesCount <= 8, "Too many color textures for the same render target");

                    break;
                }

            case OutputType::DepthWrite:
            case OutputType::DepthRead:
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

                    if (output.m_Type == OutputType::DepthRead)
                    {
                        renderTargetInfo.m_ReadonlyDepth = true;
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
    std::vector<RenderGraph::BufferDescription>&& buffers,
    std::vector<RenderGraph::TokenDescription>&& tokens
)
    : m_DirectCommandQueue(Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT))
    , m_RenderPassesDescription(std::move(renderPasses))
    , m_TextureDescriptions(textures)
    , m_BufferDescriptions(buffers)
    , m_TokenDescriptions(tokens)
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
                Assert(IsResourceDefined(input.m_Id), "Input undefined.");
            }

            for (const auto& output : pass->GetOutputs())
            {
                Assert(IsResourceDefined(output.m_Id), "Output undefined.");
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

        uint32_t renderPassIndex = 0;

        for (const auto& pRenderPass : m_RenderPassesBuilt)
        {
            PIXScope(cmd, pRenderPass->GetPassName().c_str());

            PrepareResourceForRenderPass(cmd, *pRenderPass, renderPassIndex);
            pRenderPass->Execute(context, cmd);

            renderPassIndex++;
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

    // Register resources
    {
        m_ResourcePool->Clear();

        for (const auto& desc : m_TextureDescriptions)
        {
            m_ResourcePool->RegisterTexture(desc, m_RenderPassesBuilt, renderMetadata, pDevice);
        }

        for (const auto& desc : m_BufferDescriptions)
        {
            m_ResourcePool->RegisterBuffer(desc, m_RenderPassesBuilt, renderMetadata, pDevice);
        }

        m_ResourcePool->InitHeaps(m_RenderPassesBuilt, pDevice);
    }

    // Create resources
    {
        m_ResourceStates.clear();

        // if the resource is pruned, it won't be registered
        for (const auto& desc : m_TextureDescriptions)
        {
            if (m_ResourcePool->IsRegistered(desc.m_Id))
            {
                const auto& pTexture = m_ResourcePool->CreateTexture(desc.m_Id, pDevice);
                SetCurrentResourceState(*pTexture, D3D12_RESOURCE_STATE_COMMON);
            }
        }

        for (const auto& desc : m_BufferDescriptions)
        {
            if (m_ResourcePool->IsRegistered(desc.m_Id))
            {
                const auto& pBuffer = m_ResourcePool->CreateBuffer(desc.m_Id, pDevice);
                pBuffer->ForEachResourceRecursive([this](const auto& r)
                    {
                        SetCurrentResourceState(r, D3D12_RESOURCE_STATE_COMMON);
                    });
            }
        }
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

void RenderGraph::RenderGraphRoot::PrepareResourceForRenderPass(CommandList& commandList, const RenderPass& renderPass, uint32_t renderPassIndex)
{
    for (const auto& input : renderPass.GetInputs())
    {
        if (input.m_Type == InputType::Token)
        {
            continue;
        }

        const auto& resource = m_ResourcePool->GetResource(input.m_Id);

        // SRV barriers
        if (input.m_Type == InputType::ShaderResource)
        {
            resource.ForEachResourceRecursive([this](const auto& r)
                {
                    TransitionBarrier(r, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
                });
        }

        if (input.m_Type == InputType::CopySource)
        {
            resource.ForEachResourceRecursive([this](const auto& r)
                {
                    TransitionBarrier(r, D3D12_RESOURCE_STATE_COPY_SOURCE);
                });
        }
    }

    for (const auto& output : renderPass.GetOutputs())
    {
        if (output.m_Type == OutputType::Token)
        {
            continue;
        }

        const auto& lifecycle = m_ResourcePool->GetResourceLifecycle(output.m_Id);

        if (lifecycle.m_BeginPassIndex == renderPassIndex)
        {
            const auto& resource = m_ResourcePool->GetResource(output.m_Id);
            resource.ForEachResourceRecursive([&commandList](const auto& r)
                {
                    commandList.AliasingBarrier(nullptr, r.GetD3D12Resource());
                });
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
        if (output.m_Type == OutputType::Token)
        {
            continue;
        }

        const auto& resource = m_ResourcePool->GetResource(output.m_Id);

        // Copy Destination barriers
        if (output.m_Type == OutputType::CopyDestination)
        {
            resource.ForEachResourceRecursive([this](const auto& r)
                {
                    TransitionBarrier(r, D3D12_RESOURCE_STATE_COPY_DEST);
                });
        }

        // UAV barriers
        if (output.m_Type == OutputType::UnorderedAccess)
        {
            resource.ForEachResourceRecursive([this](const auto& r)
                {
                    TransitionBarrier(r, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    UavBarrier(r);
                });
        }
    }

    FlushBarriers(commandList);

    // Process init actions
    for (const auto& output : renderPass.GetOutputs())
    {
        if (output.m_Type == OutputType::Token)
        {
            continue;
        }

        const auto& lifecycle = m_ResourcePool->GetResourceLifecycle(output.m_Id);

        if (lifecycle.m_BeginPassIndex == renderPassIndex)
        {
            const auto& description = m_ResourcePool->GetDescription(output.m_Id);

            switch (description.GetInitAction())
            {
            case ResourceInitAction::Clear:
                {
                    Assert(description.m_ResourceType == ResourceType::Texture, "Only textures support the clear init action.");

                    if (output.m_Type == OutputType::RenderTarget)
                    {
                        const auto& texture = *m_ResourcePool->GetTexture(output.m_Id);
                        commandList.ClearTexture(texture, description.GetClearValue());
                    }
                    else if (output.m_Type == OutputType::DepthRead || output.m_Type == OutputType::DepthWrite)
                    {
                        const auto& texture = *m_ResourcePool->GetTexture(output.m_Id);
                        auto dsClearValue = description.GetClearValue().GetD3D12ClearValue()->DepthStencil;
                        commandList.ClearDepthStencilTexture(texture, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, dsClearValue.Depth, dsClearValue.Stencil);
                    }
                }
                break;
            case ResourceInitAction::CopyDestination:
                // don't do anything here, the copy in the pass should do the job
                break;
            case ResourceInitAction::Discard:
                {
                    const auto& resource = m_ResourcePool->GetResource(output.m_Id);
                    commandList.DiscardResource(resource);
                }
                break;
            default:
                Assert(false, "Unknown resource init action.");
                break;
            }
        }
    }

    // Setup the render target
    if (renderTargetFindResult != m_RenderTargets.end())
    {
        const auto& renderTargetInfo = renderTargetFindResult->second;
        const auto& pRenderTarget = renderTargetInfo.m_RenderTarget;
        const auto& textures = pRenderTarget->GetTextures();

        commandList.SetRenderTarget(*pRenderTarget);
        commandList.SetAutomaticViewportAndScissorRect(*pRenderTarget);
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

bool RenderGraph::RenderGraphRoot::IsResourceDefined(ResourceId id)
{
    for (const auto& texture : m_TextureDescriptions)
    {
        if (texture.m_Id == id)
            return true;
    }

    for (const auto& buffer : m_BufferDescriptions)
    {
        if (buffer.m_Id == id)
            return true;
    }

    for (const auto& token : m_TokenDescriptions)
    {
        if (token.m_Id == id)
            return true;
    }

    return false;
}

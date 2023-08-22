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

    for (const auto& renderPass : m_RenderPassesBuilt)
    {
        renderPass->Execute(*pCommandList);
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

    m_RenderPassesBuilt.clear();

    // TODO: implement barriers, unused node stripping, resource lifetime, etc...
    for (const auto& pRenderPass : m_RenderPassesDescription)
    {
        m_RenderPassesBuilt.push_back(pRenderPass.get());
    }

    m_Textures.clear();

    for (const auto& desc : m_TextureDescriptions)
    {
        bool depth = false;
        bool unorderedAccess = false;
        bool renderTarget = false;

        for (const auto& pRenderPass : m_RenderPassesDescription)
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

        if (desc.m_Id >= m_Textures.size())
        {
            m_Textures.resize(desc.m_Id + 1);
        }
        m_Textures[desc.m_Id] = texture;
    }

}

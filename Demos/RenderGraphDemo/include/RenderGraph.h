#pragma once

#include <memory>
#include <vector>

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>

#include "RenderPass.h"

class RenderGraph
{
public:
    const std::shared_ptr<CommandQueue> m_DirectCommandQueue;
    const std::vector<std::unique_ptr<RenderPass>> m_RenderPassesDescription;
    std::vector<RenderPass*> m_RenderPassesBuilt;
    bool m_Dirty = true;

    RenderGraph(const Application& application, std::vector<std::unique_ptr<RenderPass>>&& renderPasses)
        : m_DirectCommandQueue(application.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT))
        , m_RenderPassesDescription(std::move(renderPasses))
    {}

    void Execute()
    {
        if (m_Dirty)
        {
            Build();
            m_Dirty = false;
        }

        auto pCommandList = m_DirectCommandQueue->GetCommandList();

        for (const auto& renderPass : m_RenderPassesBuilt)
        {
            renderPass->Execute(*pCommandList);
        }

        m_DirectCommandQueue->ExecuteCommandList(pCommandList);
    }

private:
    void Build()
    {
        m_RenderPassesBuilt.clear();

        // TODO: implement topological sorting, pass read/write dependencies, barriers, resource lifetime, etc...)
        for (const auto& renderPass : m_RenderPassesDescription)
        {
            m_RenderPassesBuilt.push_back(renderPass.get());
        }
    }
};
